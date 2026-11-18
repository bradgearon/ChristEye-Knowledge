#include "deployment.hpp"
#include "../sdk/sha256_impl.hpp"
#include "anaiero/infrastructure/native_child_process.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace anaiero::deployment {
namespace {
namespace fs=std::filesystem;
struct Failure:std::runtime_error{Code code;Failure(Code c,std::string text):runtime_error(std::move(text)),code(c){}};
fs::path path(std::string_view s){return fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(s.data()),s.size()));}
std::string utf8(const fs::path& p){const auto s=p.generic_u8string();return {reinterpret_cast<const char*>(s.data()),s.size()};}
void require(bool ok,Code code,std::string text){if(!ok)throw Failure(code,std::move(text));}
Bytes read(const fs::path& file,std::size_t maximum){
  require(fs::is_regular_file(fs::symlink_status(file)),Code::invalid,"package artifact must be a regular file");
  const auto size=fs::file_size(file);require(size<=maximum,Code::capacity,"package artifact exceeds its bounded size");
  std::ifstream input(file,std::ios::binary);require(bool(input),Code::io_error,"cannot read package artifact");
  Bytes bytes(static_cast<std::size_t>(size),'\0');
  require((size==0||bool(input.read(bytes.data(),static_cast<std::streamsize>(size))))&&input.peek()==std::char_traits<char>::eof()&&!input.bad(),
    Code::io_error,"package artifact changed or was truncated while reading");return bytes;
}
fs::path directory(std::string_view requested){
  require(!requested.empty()&&requested.size()<=32768&&requested.find('\0')==requested.npos,Code::invalid,"package path is invalid");
  const auto absolute=fs::absolute(path(requested)).lexically_normal();auto prefix=absolute.root_path();
  for(const auto& part:absolute.relative_path()){
    prefix/=part;require(fs::is_directory(fs::symlink_status(prefix)),Code::invalid,"package root traverses a symlink or non-directory");
  }
  return absolute;
}
struct Entry{std::string path,digest;std::uint64_t bytes=0;};
struct Manifest{std::string schema,profile,configuration,system,module;std::vector<Entry> files;};
// Only the declared manifest grammar is accepted. Unknown/duplicate members,
// non-integer lengths, nested values and trailing bytes cannot be ignored.
class Parser{
  std::string_view input_;std::size_t cursor_=0;
  [[noreturn]] void bad()const{throw Failure(Code::corrupt,"invalid or unsupported native package manifest");}
  void ws(){while(cursor_<input_.size()&&(input_[cursor_]==' '||input_[cursor_]=='\n'||input_[cursor_]=='\r'||input_[cursor_]=='\t'))++cursor_;}
  bool take(char c){ws();if(cursor_<input_.size()&&input_[cursor_]==c){++cursor_;return true;}return false;}
  void need(char c){if(!take(c))bad();}
  static unsigned hex(char c){if(c>='0'&&c<='9')return static_cast<unsigned>(c-'0');if(c>='a'&&c<='f')return static_cast<unsigned>(c-'a'+10);if(c>='A'&&c<='F')return static_cast<unsigned>(c-'A'+10);return 16;}
  unsigned codepoint(){unsigned value=0;for(unsigned i=0;i<4;++i){if(cursor_>=input_.size())bad();const auto v=hex(input_[cursor_++]);if(v==16)bad();value=value*16+v;}return value;}
  std::string text(){
    need('"');std::string value;
    while(cursor_<input_.size()){
      const auto c=static_cast<unsigned char>(input_[cursor_++]);if(c=='"')return value;if(c<32)bad();
      if(c!='\\')value+=static_cast<char>(c);
      else{
        if(cursor_==input_.size())bad();
        const char escaped=input_[cursor_++];
        if(escaped=='"'||escaped=='\\'||escaped=='/')value+=escaped;
        else if(escaped=='b')value+='\b';else if(escaped=='f')value+='\f';else if(escaped=='n')value+='\n';else if(escaped=='r')value+='\r';else if(escaped=='t')value+='\t';
        else if(escaped=='u'){
          auto cp=codepoint();
          if(cp>=0xd800&&cp<=0xdbff){if(cursor_+2>input_.size()||input_.substr(cursor_,2)!="\\u")bad();cursor_+=2;const auto low=codepoint();if(low<0xdc00||low>0xdfff)bad();cp=0x10000+(cp-0xd800)*0x400+(low-0xdc00);}
          else if(cp>=0xdc00&&cp<=0xdfff)bad();
          if(cp<0x80)value+=static_cast<char>(cp);
          else if(cp<0x800){value+=static_cast<char>(0xc0|(cp>>6));value+=static_cast<char>(0x80|(cp&63));}
          else if(cp<0x10000){value+=static_cast<char>(0xe0|(cp>>12));value+=static_cast<char>(0x80|((cp>>6)&63));value+=static_cast<char>(0x80|(cp&63));}
          else{value+=static_cast<char>(0xf0|(cp>>18));value+=static_cast<char>(0x80|((cp>>12)&63));value+=static_cast<char>(0x80|((cp>>6)&63));value+=static_cast<char>(0x80|(cp&63));}
        }else bad();
      }
      if(value.size()>32768)bad();
    }bad();
  }
  std::uint64_t number(){
    ws();const auto start=cursor_;while(cursor_<input_.size()&&input_[cursor_]>='0'&&input_[cursor_]<='9')++cursor_;
    std::uint64_t value=0;const auto raw=input_.substr(start,cursor_-start);const auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),value);
    if(raw.empty()||parsed.ec!=std::errc{}||std::to_string(value)!=raw)bad();
    return value;
  }
  Entry entry(){
    need('{');Entry entry;std::set<std::string> keys;
    do{const auto key=text();if(!keys.insert(key).second)bad();need(':');
      if(key=="path")entry.path=text();else if(key=="sha256")entry.digest=text();else if(key=="bytes")entry.bytes=number();else bad();
    }while(take(','));need('}');if(keys.size()!=3)bad();return entry;
  }
public:
  explicit Parser(std::string_view input):input_(input){}
  Manifest parse(){
    Manifest result;std::set<std::string> keys;need('{');
    do{const auto key=text();if(!keys.insert(key).second)bad();need(':');
      if(key=="schema")result.schema=text();else if(key=="profile")result.profile=text();else if(key=="configuration")result.configuration=text();
      else if(key=="host_system")result.system=text();else if(key=="module_sha256")result.module=text();
      else if(key=="files"){
        need('[');if(!take(']')){do{if(result.files.size()>=8192)bad();result.files.push_back(entry());}while(take(','));need(']');}
      }else bad();
    }while(take(','));need('}');ws();if(keys.size()!=6||cursor_!=input_.size())bad();return result;
  }
};
bool safe_relative(std::string_view name){
  if(name.empty()||name.size()>32768||name=="package-manifest.json"||name.front()=='/'||name.back()=='/')return false;
  for(const unsigned char c:name)if(c<32||c==127||c=='\\'||c==':'||c==';')return false;
  std::size_t from=0;
  while(from<name.size()){const auto end=name.find('/',from);const auto part=name.substr(from,end==name.npos?name.size()-from:end-from);
    if(part.empty()||part=="."||part=="..")return false;
    if(end==name.npos)break;
    from=end+1;}
  return true;
}
Digest hash_file(const fs::path& file,std::uint64_t expected){
  require(fs::is_regular_file(fs::symlink_status(file))&&fs::file_size(file)==expected,Code::corrupt,"package file is missing, changed or not regular");
  std::ifstream input(file,std::ios::binary);require(bool(input),Code::io_error,"cannot read package inventory file");
  std::array<char,65536> buffer{};sdk::sha_internal::Sha256 hash;std::uint64_t total=0;
  while(input){input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=input.gcount();
    if(count<=0)break;
    total+=static_cast<std::uint64_t>(count);require(total<=expected,Code::corrupt,"package file grew during verification");hash.update({buffer.data(),static_cast<std::size_t>(count)});}
  require(!input.bad()&&total==expected,Code::corrupt,"package file changed during verification");return hash.hex();
}
bool same_installation_path(fs::path actual,fs::path expected){
#ifdef _WIN32
  const auto a=actual.make_preferred().wstring(),b=expected.make_preferred().wstring();
  // Normal Windows path case is immaterial, but file identity must also match
  // for explicitly case-sensitive directories. Another hard-link path does
  // not authorize an unselected installation directory.
  return CompareStringOrdinal(a.data(),static_cast<int>(a.size()),b.data(),static_cast<int>(b.size()),TRUE)==CSTR_EQUAL&&fs::equivalent(actual,expected);
#else
  return actual==expected;
#endif
}
}
std::string json_string(std::string_view value){
  std::ostringstream out;out<<'"';for(const unsigned char c:value){if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);
    else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;else out<<static_cast<char>(c);}out<<'"';return out.str();
}
std::string selected_executable(const Package& p){
#ifdef _WIN32
  return p.directory+"/bin/ce_modular_host.exe";
#else
  return p.directory+"/bin/ce_modular_host";
#endif
}
Result verify_native_package(std::string_view requested,std::string_view module,VerifiedPackage& out,bool probe){
  out={};try{
    const auto root=directory(requested);const auto raw=read(root/"package-manifest.json",4U*1024U*1024U);const auto manifest=Parser(raw).parse();
    require(manifest.schema=="anaiero.native-package/1"&&valid_digest(manifest.module)&&valid_token(manifest.profile,128)&&valid_token(manifest.configuration,32)&&!manifest.files.empty(),Code::corrupt,"native package metadata is invalid");
#ifdef _WIN32
    require(manifest.system=="Windows",Code::unsupported,"package is not a Windows native installation");
#else
    require(manifest.system=="Linux",Code::unsupported,"package is not a Linux native installation");
#endif
    require(module.empty()||manifest.module==module,Code::unsupported,"package engine implementation differs; explicit state migration is required");
    std::set<std::string> expected;std::uint64_t total=0;std::string last;
    for(const auto& item:manifest.files){
      require(safe_relative(item.path)&&valid_digest(item.digest)&&item.bytes<=512ULL*1024ULL*1024ULL&&(last.empty()||last<item.path),Code::corrupt,"unsafe, duplicate, unsorted or unbounded package entry");
      last=item.path;expected.insert(item.path);total+=item.bytes;require(total<=8ULL*1024ULL*1024ULL*1024ULL,Code::capacity,"package exceeds the 8 GiB inventory bound");
      auto file=root;for(const auto& component:path(item.path)){
        file/=component;require(!fs::is_symlink(fs::symlink_status(file)),Code::corrupt,"package entry traverses a symlink");
      }
      require(hash_file(file,item.bytes)==item.digest,Code::corrupt,"package file bytes differ from inventory: "+item.path);
    }
    std::set<std::string> actual;
    for(fs::recursive_directory_iterator iterator(root),end;iterator!=end;++iterator){
      const auto status=iterator->symlink_status();require(!fs::is_symlink(status),Code::corrupt,"package contains an unverified symlink");
      if(fs::is_directory(status))continue;
      require(fs::is_regular_file(status),Code::corrupt,"package contains a non-regular entry");
      const auto relative=utf8(iterator->path().lexically_relative(root));if(relative=="package-manifest.json")continue;
      require(actual.size()<8192,Code::capacity,"package inventory exceeds its entry bound");actual.insert(relative);
    }
    require(actual==expected,Code::corrupt,"package contains files outside its exact inventory");
    Package identity;identity.directory=utf8(root);identity.manifest_sha256=sha256(raw);identity.module_sha256=manifest.module;
    auto host=read(path(selected_executable(identity)),8U*1024U*1024U);identity.host_sha256=sha256(host);
    const auto host_name=utf8(path(selected_executable(identity)).lexically_relative(root));
    const auto host_entry=std::find_if(manifest.files.begin(),manifest.files.end(),[&](const Entry& entry){return entry.path==host_name;});
    require(host_entry!=manifest.files.end()&&host_entry->digest==identity.host_sha256&&host_entry->bytes==host.size(),
      Code::conflict,"retained host bytes differ from the exact inventory");
    // Re-read the manifest after the inventory walk; a local writer cannot
    // swap the reviewed manifest without invalidating this admission.
    require(read(root/"package-manifest.json",4U*1024U*1024U)==raw,Code::conflict,"package manifest changed during verification");
    if(probe){
      process::NativeChildProcess child;const auto started=child.start({selected_executable(identity),{"package-probe"}});
      require(started==process::Code::ok,Code::unsupported,"candidate host cannot start on this native system");
      std::string response;const auto exchanged=child.exchange("ANAIERO-PACKAGE-PROBE/1",response,10000);child.stop();
      require(exchanged==process::Code::ok&&response=="ANAIERO-PACKAGE-PROBE/1 "+identity.module_sha256,Code::unsupported,"candidate host does not attest the reviewed compiled identity/probe protocol");
    }
    out={std::move(identity),raw,std::move(host),static_cast<std::uint64_t>(manifest.files.size()),total};return {};
  }catch(const Failure& error){return {error.code,error.what()};}catch(const std::exception& error){return {Code::io_error,error.what()};}
}
Result read_native_plan(std::string_view requested,Plan& out){
  try{return decode_plan(read(path(requested),128U*1024U),out);}catch(const Failure& error){return {error.code,error.what()};}catch(const std::exception& error){return {Code::io_error,error.what()};}
}
Result verify_selected_host(evidence::ICommitStore& store,std::string_view executable,std::string_view module){
  Service service(store);Selection selected;if(auto r=service.selection(selected);!r)return r;if(!selected.revision)return {};
  try{
    const auto actual=fs::absolute(path(executable)).lexically_normal();const auto expected=path(selected_executable(selected.current));
    if(!same_installation_path(actual,expected))return {Code::conflict,"this host path is not the canonical selected package; use package-status and update the stopped supervisor"};
    VerifiedPackage package;if(auto r=verify_native_package(selected.current.directory,module,package,false);!r)return r;
    if(package.identity!=selected.current)return {Code::conflict,"selected package changed after activation"};
    return {};
  }catch(const std::exception& error){return {Code::io_error,error.what()};}
}
}
