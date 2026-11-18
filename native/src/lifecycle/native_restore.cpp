#include "anaiero/sdk/lifecycle.hpp"
#include "christeye/living/durable_io.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <system_error>
#ifdef _WIN32
#include <sddl.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace anaiero::sdk::lifecycle {
namespace {
namespace fs=std::filesystem;
namespace os=christeye::living::durable::os;
struct RestoreFailure {Result result;};
[[noreturn]] void fail(Code code,std::string message){throw RestoreFailure{{code,std::move(message)}};}
void require(Result result){if(!result)throw RestoreFailure{std::move(result)};}
std::string path_text(const fs::path& path){const auto bytes=path.generic_u8string();return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};}
fs::path native_path(std::string_view value){return fs::path(std::u8string(reinterpret_cast<const char8_t*>(value.data()),value.size()));}
bool regular(const fs::path& path){
  std::error_code error;const auto status=fs::symlink_status(path,error);
  if(error==std::errc::no_such_file_or_directory)return false;
  if(error)fail(Code::io_error,"cannot inspect restore input");
  if(fs::is_symlink(status))fail(Code::unauthorized,"restore rejects symlinks");
  return fs::is_regular_file(status);
}
Bytes read_file(const fs::path& path,std::size_t limit){
  if(!regular(path))fail(Code::not_found,"restore input is not a regular file");
  std::error_code error;const auto size=fs::file_size(path,error);
  if(error)fail(Code::io_error,"cannot measure restore input");
  if(size>limit)fail(Code::capacity,"restore file exceeds the configured buffer bound");
  std::ifstream stream(path,std::ios::binary);if(!stream)fail(Code::io_error,"cannot read restore input");
  Bytes bytes(static_cast<std::size_t>(size),'\0');
  if(size&&!stream.read(bytes.data(),static_cast<std::streamsize>(size)))fail(Code::io_error,"restore input read was incomplete");
  return bytes;
}
void private_directory(const fs::path& path){
#ifdef _WIN32
  HANDLE token=nullptr;
  if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))fail(Code::io_error,"cannot inspect restore directory owner");
  DWORD required=0;(void)GetTokenInformation(token,TokenUser,nullptr,0,&required);
  if(required==0||required>65536){CloseHandle(token);fail(Code::io_error,"invalid restore owner token size");}
  std::vector<unsigned char> buffer(required);
  const bool obtained=GetTokenInformation(token,TokenUser,buffer.data(),required,&required)!=0;CloseHandle(token);
  if(!obtained)fail(Code::io_error,"cannot read restore directory owner");
  LPWSTR owner=nullptr;
  if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid,&owner))fail(Code::io_error,"cannot encode restore owner");
  const std::wstring descriptor=L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;"+std::wstring(owner)+L")";LocalFree(owner);
  PSECURITY_DESCRIPTOR security=nullptr;
  if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(descriptor.c_str(),SDDL_REVISION_1,&security,nullptr))
    fail(Code::io_error,"cannot construct private restore permissions");
  SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),security,FALSE};
  const bool created=CreateDirectoryW(path.c_str(),&attributes)!=0;const auto code=GetLastError();LocalFree(security);
  if(!created)fail(code==ERROR_ALREADY_EXISTS?Code::conflict:Code::io_error,"cannot create private restore directory");
#else
  if(::mkdir(path.c_str(),0700)!=0)fail(errno==EEXIST?Code::conflict:Code::io_error,"cannot create private restore directory");
#endif
}
void verify_private_directory(const fs::path& path){
  std::error_code error;const auto status=fs::symlink_status(path,error);
  if(error||!fs::is_directory(status)||fs::is_symlink(status))fail(Code::unauthorized,"restore staging directory is invalid");
#ifndef _WIN32
  const auto shared=fs::perms::group_all|fs::perms::others_all;
  if((status.permissions()&shared)!=fs::perms::none)fail(Code::unauthorized,"restore staging permissions are not private");
#endif
}
void publish_no_replace(const fs::path& staged,const fs::path& destination){
#ifdef _WIN32
  if(!MoveFileExW(staged.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)){
    const auto error=GetLastError();fail(error==ERROR_ALREADY_EXISTS||error==ERROR_FILE_EXISTS?Code::conflict:Code::io_error,"restore destination publication failed");
  }
#else
#if defined(SYS_renameat2)
  constexpr unsigned int no_replace=1U;
  if(::syscall(SYS_renameat2,AT_FDCWD,staged.c_str(),AT_FDCWD,destination.c_str(),no_replace)!=0)
    fail(errno==EEXIST?Code::conflict:(errno==ENOSYS||errno==EINVAL?Code::unsupported:Code::io_error),"atomic non-replacing restore publication failed");
#else
  (void)staged;(void)destination;fail(Code::unsupported,"host lacks atomic non-replacing directory publication");
#endif
#endif
}
bool within(const fs::path& child,const fs::path& parent){
  std::error_code error;
  const bool parent_exists=fs::exists(parent,error);
  if(error)fail(Code::io_error,"cannot inspect restore tree boundary");
  if(!parent_exists)return false;
  // Compare existing ancestor identities, rather than spelling: Windows case
  // aliases and other names for the same directory must obey the same fence.
  for(auto current=child;!current.empty();){
    const bool exists=fs::exists(current,error);
    if(error)fail(Code::io_error,"cannot inspect restore ancestor");
    if(exists){
      const bool same=fs::equivalent(current,parent,error);
      if(error)fail(Code::io_error,"cannot compare restore directory identities");
      if(same)return true;
    }
    const auto next=current.parent_path();if(next==current)break;current=next;
  }
  return false;
}
struct Inventory {std::vector<fs::path> files,directories;std::uint64_t bytes=0,largest=0;};
Inventory inventory(const fs::path& source,const RestoreOptions& options){
  Inventory result;std::error_code error;
  for(fs::recursive_directory_iterator it(source,error),end;!error&&it!=end;it.increment(error)){
    if(it.depth()>16)fail(Code::capacity,"restore directory depth exceeds the bound");
    const auto status=it->symlink_status(error);if(error)break;
    if(fs::is_symlink(status))fail(Code::unauthorized,"restore rejects all source symlinks");
    if(result.files.size()+result.directories.size()>=options.maximum_files)fail(Code::capacity,"restore entry count exceeds the bound");
    const auto relative=it->path().lexically_relative(source);
    if(fs::is_directory(status)){result.directories.push_back(relative);continue;}
    if(!fs::is_regular_file(status))fail(Code::unauthorized,"restore source contains a special file");
    if(relative=="WRITER")continue; // Recreate the runtime lock instead of copying an open lock file.
    if(result.files.size()>=options.maximum_files)fail(Code::capacity,"restore file count exceeds the bound");
    const auto bytes=it->file_size(error);if(error)break;
    if(bytes>options.maximum_file_bytes||result.bytes>options.store.maximum_bytes||bytes>options.store.maximum_bytes-result.bytes)
      fail(Code::capacity,"restore source exceeds the configured byte quota");
    result.bytes+=bytes;result.largest=std::max(result.largest,static_cast<std::uint64_t>(bytes));result.files.push_back(relative);
  }
  if(error)fail(Code::io_error,"cannot enumerate restore source");
  if(result.bytes>options.store.maximum_bytes||result.largest>options.store.maximum_bytes-result.bytes||
     options.store.maximum_bytes-result.bytes-result.largest<65536)
    fail(Code::capacity,"restore requires headroom for atomic copies and quarantine metadata");
  std::sort(result.files.begin(),result.files.end());std::sort(result.directories.begin(),result.directories.end());
  return result;
}
struct Intent {
  std::string source,destination,operation,source_file_digest,nonce;
  RestoreReport report;
};
Bytes encode(const Intent& intent){return pack({"restore-intent/2",intent.source,intent.destination,intent.operation,intent.source_file_digest,intent.nonce,
  std::to_string(intent.report.copied_files),std::to_string(intent.report.copied_bytes),std::to_string(intent.report.source_generation),
  intent.report.source_root,intent.report.incarnation});}
bool integer(std::string_view bytes,std::uint64_t& value){const auto result=std::from_chars(bytes.data(),bytes.data()+bytes.size(),value);return result.ec==std::errc{}&&result.ptr==bytes.data()+bytes.size();}
Intent decode(std::string_view bytes){
  std::vector<std::string> fields;if(!unpack(bytes,fields,11,16384)||fields.size()!=11||fields[0]!="restore-intent/2")fail(Code::corrupt,"invalid restore intent");
  Intent out;out.source=fields[1];out.destination=fields[2];out.operation=fields[3];out.source_file_digest=fields[4];
  out.nonce=fields[5];
  if(!valid_digest(out.nonce)||!integer(fields[6],out.report.copied_files)||!integer(fields[7],out.report.copied_bytes)||!integer(fields[8],out.report.source_generation))fail(Code::corrupt,"invalid restore intent counters or nonce");
  out.report.source_root=fields[9];out.report.incarnation=fields[10];return out;
}
void save(const fs::path& stage,const Intent& intent){if(!os::atomic_write(stage/"INTENT",encode(intent)))fail(Code::io_error,"cannot persist restore intent");}
}

Result restore_quarantined_store(std::string_view source_text,std::string_view destination_text,
    std::string operation,RestoreReport& report,const RestoreOptions& options){
  report={};
  bool destination_published=false;
  try{
    if(source_text.empty()||destination_text.empty()||!valid_token(operation,256)||options.maximum_files==0||
       options.maximum_files>1048576||options.maximum_file_bytes==0||options.maximum_file_bytes>64U*1024U*1024U)
      fail(Code::invalid,"invalid bounded restore request");
    std::error_code error;auto source=native_path(source_text),destination=native_path(destination_text);
    if(fs::is_symlink(source,error)||!fs::is_directory(source,error))fail(Code::not_found,"restore source is not a native store directory");
    source=fs::canonical(source,error);if(error)fail(Code::io_error,"cannot resolve restore source");
    if(!regular(source/"ROOT")||!regular(source/"WRITER"))fail(Code::not_found,"restore source has no canonical root/writer lock");
    destination=fs::absolute(destination,error).lexically_normal();if(error)fail(Code::io_error,"cannot resolve restore destination");
    auto parent=fs::weakly_canonical(destination.parent_path(),error);if(error)fail(Code::io_error,"cannot resolve prospective restore parent");
    destination=parent/destination.filename();
    if(within(destination,source)||within(source,destination))fail(Code::invalid,"restore source and destination must be separate trees");
    fs::create_directories(parent,error);if(error)fail(Code::io_error,"cannot create restore destination parent");
    parent=fs::canonical(parent,error);if(error)fail(Code::io_error,"cannot resolve restore destination parent");
    destination=parent/destination.filename();
    if(within(destination,source)||within(source,destination))fail(Code::invalid,"restore source and destination must be separate trees");
    const auto suffix=sha256(pack({path_text(source),path_text(destination),operation})).substr(0,24);
    auto stage_name=destination.filename();stage_name+=".restore-"+suffix;
    const auto stage=parent/stage_name;const auto payload=stage/"payload";
    report.staging_directory=path_text(stage);
    os::File source_lock;
    if(!source_lock.open(source/"WRITER")||!source_lock.lock_exclusive())fail(Code::busy,"restore source still has an active native store owner");
    const auto source_file_digest=sha256(read_file(source/"ROOT",16384));
    Intent intent{path_text(source),path_text(destination),operation,source_file_digest,{}, {}};
    const bool staged=fs::exists(stage,error);if(error)fail(Code::io_error,"cannot inspect restore staging");
    if(staged){
      verify_private_directory(stage);const auto previous=decode(read_file(stage/"INTENT",16384));
      if(previous.source!=intent.source||previous.destination!=intent.destination||previous.operation!=intent.operation||
         previous.source_file_digest!=source_file_digest)fail(Code::conflict,"restore intent no longer matches the source and destination");
      intent=previous;
    }
    const bool exists=fs::exists(destination,error);if(error)fail(Code::io_error,"cannot inspect restore destination");
    if(exists){
      if(!staged||intent.report.incarnation.empty())fail(Code::conflict,"restore destination already exists");
      if(fs::is_symlink(destination,error))fail(Code::unauthorized,"restore destination is a symlink");
      std::unique_ptr<evidence::ICommitStore> existing;require(evidence::open_native_store(path_text(destination),options.store,existing));
      Service lifecycle(*existing,options.lifecycle);require(lifecycle.open());Snapshot snapshot;require(lifecycle.snapshot(snapshot));
      evidence::CommitReceipt receipt;require(existing->lookup_operation(operation,receipt));
      if(snapshot.incarnation!=intent.report.incarnation)fail(Code::conflict,"existing destination belongs to another restore incarnation");
      destination_published=true;
      report=intent.report;report.staging_directory=path_text(stage);report.published=true;
      if(!os::sync_directory(parent)||!os::sync_directory(stage))return {Code::committed_degraded,"restored destination exists but its directory flush still needs reconciliation"};
      return {};
    }
    if(!staged){
      std::random_device random;
      std::vector<std::string> entropy{intent.source,intent.destination,intent.operation,intent.source_file_digest,
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())};
      for(unsigned i=0;i<8;++i)entropy.push_back(std::to_string(random()));
      intent.nonce=sha256(pack(entropy));
      private_directory(stage);save(stage,intent);
    }
    verify_private_directory(stage);
    if(intent.report.incarnation.empty()){
      // A pre-publication partial copy has no authority. Restart it only under
      // this exact persisted source/destination/operation witness.
      if(fs::exists(payload,error)){
        verify_private_directory(payload);fs::remove_all(payload,error);if(error)fail(Code::io_error,"cannot clear partial restore staging");
      }
      private_directory(payload);
      const auto inputs=inventory(source,options);
      const auto space=fs::space(parent,error);
      if(error||space.available<options.store.minimum_free_bytes||inputs.bytes+inputs.largest>space.available-options.store.minimum_free_bytes)
        fail(Code::capacity,"restore destination has insufficient disk headroom");
      for(const auto& directory:inputs.directories){fs::create_directories(payload/directory,error);if(error)fail(Code::io_error,"cannot create restore child directory");}
      for(const auto& relative:inputs.files){
        const auto bytes=read_file(source/relative,options.maximum_file_bytes);
        if(!os::atomic_write(payload/relative,bytes))fail(Code::io_error,"restore copy could not be durably published");
        const auto verified=read_file(payload/relative,options.maximum_file_bytes);
        if(verified!=bytes)fail(Code::corrupt,"restore copy read-back mismatch");
        ++intent.report.copied_files;intent.report.copied_bytes+=static_cast<std::uint64_t>(bytes.size());
      }
      if(intent.report.copied_bytes!=inputs.bytes)fail(Code::conflict,"restore source size changed while locked");
      for(auto it=inputs.directories.rbegin();it!=inputs.directories.rend();++it)
        if(!os::sync_directory(payload/ *it))fail(Code::io_error,"restore child directory flush failed");
      if(!os::sync_directory(payload))fail(Code::io_error,"restore store directory flush failed");
      std::unique_ptr<evidence::ICommitStore> clone;require(evidence::open_native_store(path_text(payload),options.store,clone));
      if(!clone->info().index_ready)require(clone->repair_index());
      evidence::AuditReport audit;require(clone->audit(audit));
      intent.report.source_generation=clone->generation();intent.report.source_root=clone->info().root_digest;
      Service lifecycle(*clone,options.lifecycle);require(lifecycle.open());
      const auto quarantined=lifecycle.quarantine_restore(operation,intent.nonce);
      if(!quarantined.committed())require(quarantined);
      if(!quarantined.ok())require(clone->repair_index());
      Snapshot snapshot;require(lifecycle.snapshot(snapshot));
      if(!snapshot.quarantined||snapshot.incarnation.empty())fail(Code::corrupt,"restore quarantine did not become authoritative");
      intent.report.incarnation=snapshot.incarnation;save(stage,intent);
    }
    {
      std::unique_ptr<evidence::ICommitStore> clone;require(evidence::open_native_store(path_text(payload),options.store,clone));
      Service lifecycle(*clone,options.lifecycle);require(lifecycle.open());Snapshot snapshot;require(lifecycle.snapshot(snapshot));
      if(!snapshot.quarantined||snapshot.incarnation!=intent.report.incarnation)fail(Code::corrupt,"staged restore lost its quarantine witness");
    }
    if(sha256(read_file(source/"ROOT",16384))!=source_file_digest)fail(Code::conflict,"restore source root changed while locked");
    publish_no_replace(payload,destination);
    destination_published=true;
    report=intent.report;report.staging_directory=path_text(stage);report.published=true;
    if(!os::sync_directory(parent)||!os::sync_directory(stage))return {Code::committed_degraded,"quarantined destination is visible but directory durability needs reconciliation"};
    return {};
  }catch(const RestoreFailure& failure){
    if(destination_published){report.published=true;return {Code::committed_degraded,failure.result.message};}
    return failure.result;
  }catch(const std::exception& exception){
    report.published=destination_published;return {destination_published?Code::committed_degraded:Code::io_error,exception.what()};
  }
}
} // namespace anaiero::sdk::lifecycle
