// Real filesystem exhaustion, only on an explicitly provisioned tiny test
// volume. This executable never mounts a filesystem or fills the workspace.
#include "anaiero/sdk/evidence.hpp"
#include "anaiero/sdk/engine_files.hpp"
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <unistd.h>
#endif
namespace fs=std::filesystem;
namespace s=anaiero::sdk;
namespace ev=s::evidence;
constexpr std::uint64_t maximum_fill=64ULL*1024*1024;
void check(bool value,std::string message){if(!value)throw std::runtime_error(std::move(message));}
void require(const s::Result& result){check(result.ok(),std::string(s::code_name(result.code))+": "+result.message);}
std::string read(const fs::path& path,std::uint64_t maximum){
  check(fs::is_regular_file(fs::symlink_status(path))&&fs::file_size(path)<=maximum,"fixture witness must be a bounded regular file");
  std::ifstream input(path,std::ios::binary);check(input.good(),"cannot read fixture witness");std::ostringstream out;out<<input.rdbuf();return out.str();
}
std::string utf8(const fs::path& path){const auto bytes=path.generic_u8string();return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};}
fs::path native_path(std::string_view value){return fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(value.data()),value.size()));}
std::string quoted(std::string_view value){std::ostringstream out;out<<'"';for(const unsigned char c:value){
  if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);else if(c=='\n')out<<"\\n";else if(c=='\r')out<<"\\r";else if(c=='\t')out<<"\\t";else if(c>=32)out<<static_cast<char>(c);
}out<<'"';return out.str();}
std::uint64_t validate_fixture(const fs::path& root,std::string_view nonce,const fs::path& output_parent){
  check(s::valid_token(nonce,96),"fixture nonce is invalid");
  check(fs::is_directory(fs::symlink_status(root)),"fixture root must be a real directory");
  check(read(root/"OWNER",256)=="ANAIERO-STORAGE-PRESSURE/1\nnonce="+std::string(nonce)+"\n","fixture ownership marker/nonce differs");
  const auto space=fs::space(root);
  check(space.capacity>=8U*1024U*1024U&&space.capacity<=maximum_fill,"refusing to fill any filesystem outside 8–64 MiB");
#ifdef _WIN32
  std::array<wchar_t,32768> volume{},outside{};std::array<wchar_t,128> label{},filesystem{};
  check(GetVolumePathNameW(root.c_str(),volume.data(),static_cast<DWORD>(volume.size()))!=0,"cannot identify pressure volume");
  check(GetVolumePathNameW(output_parent.c_str(),outside.data(),static_cast<DWORD>(outside.size()))!=0,"cannot identify report volume");
  check(fs::equivalent(root,fs::path(volume.data())),"pressure directory must be the isolated volume root");
  check(std::wstring(volume.data())!=std::wstring(outside.data()),"pressure and report directories must use different volumes");
  check(GetVolumeInformationW(volume.data(),label.data(),static_cast<DWORD>(label.size()),nullptr,nullptr,nullptr,filesystem.data(),static_cast<DWORD>(filesystem.size()))!=0,"cannot inspect pressure filesystem");
  check(std::wstring(label.data())==L"ANAIERO_PRESSURE"&&std::wstring(filesystem.data())==L"NTFS","pressure fixture requires labelled isolated NTFS volume");
#else
  struct stat root_stat{},parent_stat{},output_stat{};struct statfs filesystem{};
  check(::stat(root.c_str(),&root_stat)==0&&::stat(root.parent_path().c_str(),&parent_stat)==0&&::stat(output_parent.c_str(),&output_stat)==0&&::statfs(root.c_str(),&filesystem)==0,"cannot inspect pressure filesystem");
  check(root_stat.st_dev!=parent_stat.st_dev&&root_stat.st_dev!=output_stat.st_dev,"pressure fixture must be a distinct mount from parent and report output");
  check(filesystem.f_type==0x01021994,"pressure fixture requires an isolated tmpfs mount");
#endif
  return space.capacity;
}
class Filler {
  fs::path path_;bool owned_=false;
#ifdef _WIN32
  HANDLE handle_=INVALID_HANDLE_VALUE;
#else
  int handle_=-1;
#endif
public:
  std::uint64_t bytes=0;unsigned native_error=0;bool exhausted=false;
  explicit Filler(fs::path path):path_(std::move(path)){
#ifdef _WIN32
    handle_=CreateFileW(path_.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(handle_!=INVALID_HANDLE_VALUE,"owned pressure filler already exists or cannot be created");
#else
    handle_=::open(path_.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    check(handle_>=0,"owned pressure filler already exists or cannot be created");
#endif
    owned_=true;
  }
  void fill()noexcept{
    std::array<char,32768> block{};for(std::size_t i=0;i<block.size();++i)block[i]=static_cast<char>((i*31U+17U)%251U);
    while(bytes<maximum_fill){
#ifdef _WIN32
      DWORD written=0;if(!WriteFile(handle_,block.data(),static_cast<DWORD>(block.size()),&written,nullptr)){
        native_error=GetLastError();exhausted=native_error==ERROR_DISK_FULL||native_error==ERROR_HANDLE_DISK_FULL;break;
      }
      if(written==0)break;
      bytes+=written;
#else
      const auto written=::write(handle_,block.data(),block.size());
      if(written<0){if(errno==EINTR)continue;native_error=static_cast<unsigned>(errno);exhausted=errno==ENOSPC;break;}
      if(written==0)break;
      bytes+=static_cast<std::uint64_t>(written);
#endif
    }
#ifdef _WIN32
    if(!FlushFileBuffers(handle_)&&!exhausted){native_error=GetLastError();exhausted=native_error==ERROR_DISK_FULL||native_error==ERROR_HANDLE_DISK_FULL;}
#else
    if(::fsync(handle_)!=0&&!exhausted){native_error=static_cast<unsigned>(errno);exhausted=errno==ENOSPC;}
#endif
  }
  bool release_tail(std::uint64_t amount)noexcept{
    if(bytes<=amount)return false;
#ifdef _WIN32
    LARGE_INTEGER length{};length.QuadPart=static_cast<LONGLONG>(bytes-amount);
    return SetFilePointerEx(handle_,length,nullptr,FILE_BEGIN)&&SetEndOfFile(handle_)&&FlushFileBuffers(handle_);
#else
    return ::ftruncate(handle_,static_cast<off_t>(bytes-amount))==0&&::fsync(handle_)==0;
#endif
  }
  void remove()noexcept{
#ifdef _WIN32
    if(handle_!=INVALID_HANDLE_VALUE){CloseHandle(handle_);handle_=INVALID_HANDLE_VALUE;}
#else
    if(handle_>=0){::close(handle_);handle_=-1;}
#endif
    if(owned_){std::error_code error;(void)fs::remove(path_,error);if(!error)owned_=false;}
  }
  ~Filler(){remove();}
};
struct PressureClock final:ev::IAdmissionClock {
  std::uint64_t instant=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  mutable Filler* filler=nullptr;mutable bool invoked=false,headroom_released=false;
  std::uint64_t unix_seconds()const noexcept override{
    if(filler){auto* current=filler;filler=nullptr;current->fill();invoked=true;headroom_released=current->exhausted&&current->release_tail(65536);}
    return instant;
  }
};
ev::CommitBatch batch(std::string id,std::size_t bytes,char value){
  ev::CommitBatch result;result.operation_id=std::move(id);std::string payload(bytes,value);const auto digest=s::sha256(payload);
  result.request_digest=s::sha256(s::pack({result.operation_id,digest}));result.blobs.push_back({digest,payload,{}});
  result.records.push_back({"pressure.fixture",result.operation_id,digest,{digest},false});return result;
}
int main(int argc,char** argv){
  if(argc!=4){std::cerr<<"Usage: anaiero_storage_pressure_tests ISOLATED_VOLUME OWNER_NONCE NEW_REPORT_DIRECTORY\n";return 2;}
  fs::path report_directory;bool created=false,passed=false,admission_preserved=false,write_preserved=false,recovered=false,retried=false,audited=false;
  std::uint64_t capacity=0,first_bytes=0,second_bytes=0;unsigned first_error=0,second_error=0;
  s::Code admission_code=s::Code::invalid,write_code=s::Code::invalid;std::string failure,original_root,final_root;
  try{
    const auto root=fs::canonical(native_path(argv[1]));report_directory=fs::absolute(native_path(argv[3])).lexically_normal();
    const auto output_parent=fs::canonical(report_directory.parent_path());capacity=validate_fixture(root,argv[2],output_parent);
    check(fs::create_directory(report_directory),"report directory already exists");created=true;
    const std::string profile="ANAIERO-STORAGE-PRESSURE-PROFILE/1\nmaximum_filesystem_bytes=67108864\nmaximum_filler_bytes=67108864\npost_admission_free_bytes=65536\npayload_bytes=1048576\n";
    require(anaiero::engines::write_new_artifact(utf8(report_directory/"profile.txt"),profile));
    check(!fs::exists(root/"store"),"fixture store already exists");PressureClock clock;ev::StoreOptions options;
    options.maximum_bytes=1024ULL*1024ULL*1024ULL;options.minimum_free_bytes=0;options.admission_clock=&clock;
    std::unique_ptr<ev::ICommitStore> store;require(ev::open_native_store(utf8(root/"store"),options,store));
    const auto baseline=batch("pressure-baseline",65536,'b');ev::CommitReceipt baseline_receipt;require(store->commit(baseline,baseline_receipt));
    const auto before=store->info();original_root=before.root_digest;
    {
      Filler filler(root/"owned-pressure-fill.bin");filler.fill();first_bytes=filler.bytes;first_error=filler.native_error;
      check(filler.exhausted,"filler did not encounter actual filesystem exhaustion within its bound");
      const auto rejected=batch("pressure-admission-rejected",65536,'a');ev::CommitReceipt receipt;const auto result=store->commit(rejected,receipt);admission_code=result.code;
      const auto after=store->info();admission_preserved=after.root_digest==before.root_digest&&after.generation==before.generation&&after.writable;
      check(result.code==s::Code::capacity&&result.message.find("filesystem admission headroom")!=std::string::npos&&admission_preserved,"full filesystem admission did not preserve its canonical root");
      check(store->lookup_operation(rejected.operation_id,receipt).code==s::Code::not_found,"rejected operation acquired a receipt");
    }
    const auto pending=batch("pressure-write-interrupted",1048576,'w');
    {
      Filler filler(root/"owned-pressure-fill.bin");clock.filler=&filler;ev::CommitReceipt receipt;
      const auto result=store->commit(pending,receipt);write_code=result.code;second_bytes=filler.bytes;second_error=filler.native_error;
      const auto after=store->info();write_preserved=after.root_digest==before.root_digest&&after.generation==before.generation;
      check(clock.invoked&&clock.headroom_released&&filler.exhausted,"post-admission fixture did not exhaust its isolated filesystem");
      check(result.code==s::Code::io_error&&write_preserved&&!after.writable,"post-admission write failure did not retain the original root and stop admission");
      check(fs::is_regular_file(root/"store"/"STAGING"),"partial blob-write failure did not retain its recovery intent");
    }
    store.reset();require(ev::open_native_store(utf8(root/"store"),options,store));
    const auto reopened=store->info();recovered=reopened.root_digest==before.root_digest&&reopened.generation==before.generation&&reopened.writable;
    check(recovered,"reopening after freeing the owned filler did not recover the original canonical state");
    const auto staged_blob=root/"store"/"blobs"/pending.blobs.front().digest;auto staged_tmp=staged_blob;staged_tmp+=".tmp";
    check(!fs::exists(root/"store"/"STAGING")&&!fs::exists(root/"store"/"STAGING.tmp")&&!fs::exists(staged_blob)&&!fs::exists(staged_tmp),
      "reopen left the abandoned pressure intent or its staged blob bytes behind before retry");
    ev::CommitReceipt old;require(store->lookup_operation(baseline.operation_id,old));check(old.root_digest==baseline_receipt.root_digest&&old.batch_digest==baseline_receipt.batch_digest,"original receipt changed across exhaustion");
    ev::CommitReceipt receipt;require(store->commit(pending,receipt));const auto committed=store->info();ev::CommitReceipt repeated;require(store->commit(pending,repeated));
    retried=repeated.duplicate&&repeated.root_digest==receipt.root_digest&&repeated.generation==receipt.generation&&store->info().root_digest==committed.root_digest;
    check(retried,"exact write retry changed its original receipt");s::Bytes payload;require(store->get_blob(pending.blobs.front().digest,payload));check(payload==pending.blobs.front().bytes,"recovered payload differs");
    ev::AuditReport audit;require(store->audit(audit));audited=true;final_root=store->info().root_digest;passed=true;
  }catch(const std::exception& error){failure=error.what();}
  if(created){
    std::ostringstream report;report<<"{\"schema\":\"anaiero.storage-pressure/1\",\"passed\":"<<(passed?"true":"false")
      <<",\"scope\":\"real isolated filesystem exhaustion and process-level recovery; no device or power-loss durability claim\""
      <<",\"build_identity\":\""<<ANAIERO_BUILTIN_BUILD_ID<<"\",\"filesystem_capacity_bytes\":"<<capacity
      <<",\"maximum_filler_bytes\":"<<maximum_fill<<",\"admission_fill_bytes\":"<<first_bytes<<",\"admission_native_error\":"<<first_error
      <<",\"post_admission_fill_bytes\":"<<second_bytes<<",\"post_admission_native_error\":"<<second_error
      <<",\"admission_result\":"<<quoted(s::code_name(admission_code))<<",\"write_result\":"<<quoted(s::code_name(write_code))
      <<",\"admission_root_preserved\":"<<(admission_preserved?"true":"false")<<",\"write_root_preserved\":"<<(write_preserved?"true":"false")
      <<",\"recovered\":"<<(recovered?"true":"false")<<",\"exact_retry_verified\":"<<(retried?"true":"false")<<",\"audit_passed\":"<<(audited?"true":"false")
      <<",\"original_root\":"<<quoted(original_root)<<",\"final_root\":"<<quoted(final_root)<<",\"failure\":"<<quoted(failure)<<"}\n";
    const auto saved=anaiero::engines::write_new_artifact(utf8(report_directory/"report.json"),report.str());
    if(!saved){std::cerr<<saved.message<<'\n';return 2;}std::cout<<report.str();
  }
  if(!passed){std::cerr<<"Storage-pressure fixture refused or failed: "<<failure<<'\n';return 2;}return 0;
}
