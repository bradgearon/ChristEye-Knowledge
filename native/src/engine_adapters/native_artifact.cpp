#include "anaiero/sdk/engine_files.hpp"
#include "christeye/living/durable_io.hpp"
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <fstream>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace anaiero::engines {
namespace {
namespace fs=std::filesystem;
namespace os=christeye::living::durable::os;
using namespace sdk;
constexpr std::size_t maximum_artifact_bytes=8U*1024U*1024U;
constexpr std::size_t maximum_intent_bytes=64U*1024U;
enum class Presence {absent,regular,other,error};
enum class Contents {absent,complete,prefix,different,error};
std::string utf8(const fs::path& path){const auto value=path.generic_u8string();return {reinterpret_cast<const char*>(value.data()),value.size()};}
Presence presence(const fs::path& path){
  std::error_code error;const auto status=fs::symlink_status(path,error);
  if(error==std::errc::no_such_file_or_directory)return Presence::absent;
  if(error)return Presence::error;
  if(!fs::exists(status))return Presence::absent;
  return fs::is_regular_file(status)?Presence::regular:Presence::other;
}
bool bounded_read(const fs::path& path,std::size_t maximum,std::string& bytes){
  if(presence(path)!=Presence::regular)return false;
  std::error_code error;const auto size=fs::file_size(path,error);
  if(error||size>maximum)return false;
  std::ifstream input(path,std::ios::binary);if(!input)return false;
  bytes.resize(static_cast<std::size_t>(size));
  if(size!=0&&!input.read(bytes.data(),static_cast<std::streamsize>(size)))return false;
  return input.peek()==std::char_traits<char>::eof()&&!input.bad();
}
Contents inspect(const fs::path& path,std::string_view expected){
  const auto status=presence(path);
  if(status==Presence::absent)return Contents::absent;
  if(status==Presence::error)return Contents::error;
  if(status!=Presence::regular)return Contents::different;
  std::error_code error;const auto size=fs::file_size(path,error);
  if(error)return Contents::error;
  if(size>expected.size())return Contents::different;
  std::string bytes;if(!bounded_read(path,expected.size(),bytes))return Contents::error;
  if(bytes!=expected.substr(0,bytes.size()))return Contents::different;
  return bytes.size()==expected.size()?Contents::complete:Contents::prefix;
}
bool create_synced(const fs::path& path,std::string_view bytes,bool& owned){
  owned=false;
#ifdef _WIN32
  const auto handle=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(handle==INVALID_HANDLE_VALUE)return false;
  owned=true;bool ok=true;std::size_t offset=0;
  while(offset<bytes.size()){
    DWORD written=0;const auto count=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,1U<<20));
    if(!WriteFile(handle,bytes.data()+offset,count,&written,nullptr)||written==0){ok=false;break;}
    offset+=written;
  }
  if(ok)ok=FlushFileBuffers(handle)!=0;
  CloseHandle(handle);return ok;
#else
  const auto handle=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
  if(handle<0)return false;
  owned=true;bool ok=true;std::size_t offset=0;
  while(offset<bytes.size()){
    const auto count=::write(handle,bytes.data()+offset,bytes.size()-offset);
    if(count<0&&errno==EINTR)continue;
    if(count<=0){ok=false;break;}offset+=static_cast<std::size_t>(count);
  }
  if(ok)ok=::fsync(handle)==0;
  ::close(handle);return ok;
#endif
}
bool sync_existing(const fs::path& path){
  if(presence(path)!=Presence::regular)return false;
#ifdef _WIN32
  const auto handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,
    nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(handle==INVALID_HANDLE_VALUE)return false;
  const auto ok=FlushFileBuffers(handle)!=0;CloseHandle(handle);return ok;
#else
  const auto handle=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
  if(handle<0)return false;
  const auto ok=::fsync(handle)==0;::close(handle);return ok;
#endif
}
bool sync_parent(const fs::path& path){
#ifdef _WIN32
  const auto handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
    nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
  if(handle==INVALID_HANDLE_VALUE)return false;
  const auto ok=FlushFileBuffers(handle)!=0;const auto error=GetLastError();CloseHandle(handle);
  // Directory FlushFileBuffers is not universal; file flushing and the
  // write-through rename retain the platform's supported durability path.
  return ok||error==ERROR_ACCESS_DENIED||error==ERROR_INVALID_FUNCTION;
#else
  return os::sync_directory(path);
#endif
}
bool rename_new(const fs::path& source,const fs::path& target){
#ifdef _WIN32
  return MoveFileExW(source.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH)!=0;
#else
  return ::syscall(SYS_renameat2,AT_FDCWD,source.c_str(),AT_FDCWD,target.c_str(),1U)==0;
#endif
}
std::string filename_key(const fs::path& filename){
#ifdef _WIN32
  const auto input=filename.wstring();
  const auto size=LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_UPPERCASE,input.data(),static_cast<int>(input.size()),nullptr,0,nullptr,nullptr,0);
  if(size<=0)return {};
  std::wstring folded(static_cast<std::size_t>(size),L'\0');
  if(LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_UPPERCASE,input.data(),static_cast<int>(input.size()),folded.data(),size,nullptr,nullptr,0)!=size)return {};
  return utf8(fs::path(folded));
#else
  return utf8(filename);
#endif
}
Bytes intent_wire(std::string_view target,std::string_view bytes){
  const auto body=pack({"ANAIERO-ARTIFACT-INTENT/1",std::string(target),sha256(bytes),std::to_string(bytes.size())});
  return pack({body,sha256(body)});
}
Result match_intent(const fs::path& path,std::string_view expected,bool& exists){
  const auto status=presence(path);exists=status!=Presence::absent;
  if(!exists)return {};
  if(status!=Presence::regular)return {status==Presence::error?Code::io_error:Code::conflict,"artifact intent is not a regular owned record"};
  Bytes raw;if(!bounded_read(path,maximum_intent_bytes,raw))return {Code::corrupt,"artifact intent cannot be read within its bound"};
  std::vector<std::string> envelope,fields;
  if(!unpack(raw,envelope,2,maximum_intent_bytes)||envelope.size()!=2||sha256(envelope[0])!=envelope[1]||
     !unpack(envelope[0],fields,4,maximum_intent_bytes)||fields.size()!=4||fields[0]!="ANAIERO-ARTIFACT-INTENT/1")
    return {Code::corrupt,"artifact intent is invalid; preserved for inspection"};
  if(raw!=expected)return {Code::conflict,"artifact staging belongs to another target, digest or size"};
  return {};
}
bool remove_owned(const fs::path& path){std::error_code error;return fs::remove(path,error)&&!error;}
Result finish_existing(const fs::path& target,const fs::path& intent,const fs::path& staged,std::string_view wire,std::string_view bytes){
  // A successful retry must finish any previously unconfirmed durability barrier.
  if(!sync_existing(target)||!sync_parent(target.parent_path()))return {Code::committed_degraded,"artifact bytes exist; file/directory flush was not confirmed"};
  bool owned=false;const auto matching=match_intent(intent,wire,owned);
  if(!matching||!owned)return {};
  const auto contents=inspect(staged,bytes);
  if(contents==Contents::different||contents==Contents::error)return {}; // Keep unrelated files.
  if(contents!=Contents::absent&&!remove_owned(staged))return {Code::committed_degraded,"artifact is durable; owned staging cleanup needs retry"};
  if(!remove_owned(intent)||!sync_parent(target.parent_path()))return {Code::committed_degraded,"artifact is durable; intent cleanup needs retry"};
  return {};
}
}

sdk::Result write_new_artifact(std::string_view requested,std::string_view bytes){
  using namespace sdk;
  if(requested.empty()||requested.size()>32768||requested.find('\0')!=std::string_view::npos)return {Code::invalid,"artifact path must be a bounded nonempty UTF-8 path"};
  if(bytes.size()>maximum_artifact_bytes)return {Code::capacity,"artifact exceeds the 8 MiB byte bound"};
  try{
    std::error_code error;const auto input=fs::path(std::u8string(reinterpret_cast<const char8_t*>(requested.data()),requested.size()));
    const auto absolute=fs::absolute(input,error).lexically_normal();
    if(error||absolute.filename().empty()||absolute.filename()=="."||absolute.filename()=="..")return {Code::invalid,"artifact filename is required"};
    const auto parent=fs::canonical(absolute.parent_path(),error);
    if(error||!fs::is_directory(parent))return {Code::invalid,"artifact parent directory must exist"};
    const auto target=parent/absolute.filename();const auto name=filename_key(target.filename());
    if(name.empty())return {Code::invalid,"artifact filename could not be normalized"};
    const auto stem=parent/(".anaiero-artifact-"+sha256(name));auto lock_path=stem;lock_path+=".lock";
    auto intent=stem;intent+=".intent";auto staged=stem;staged+=".stage";
    const auto lock_status=presence(lock_path);
    if(lock_status!=Presence::absent&&lock_status!=Presence::regular)return {Code::conflict,"artifact lock path is not a regular owned file"};
    os::File writer;if(!writer.open(lock_path))return {Code::io_error,"cannot open artifact writer lock"};
    if(!writer.lock_exclusive())return {Code::busy,"artifact target already has a writer"};
    const auto wire=intent_wire(utf8(parent)+"/"+name,bytes);
    if(wire.size()>maximum_intent_bytes)return {Code::capacity,"resolved artifact path exceeds the bounded intent size"};
    const auto existing=inspect(target,bytes);
    if(existing==Contents::complete)return finish_existing(target,intent,staged,wire,bytes);
    if(existing==Contents::error)return {Code::io_error,"cannot inspect artifact destination"};
    if(existing!=Contents::absent)return {Code::conflict,"artifact destination contains different bytes or is not a regular file"};
    bool has_intent=false;const auto owned=match_intent(intent,wire,has_intent);if(!owned)return owned;
    if(!has_intent){
      if(presence(staged)!=Presence::absent)return {Code::conflict,"unowned artifact staging exists; preserved for inspection"};
      bool created=false;
      if(!create_synced(intent,wire,created)){
        if(created)(void)remove_owned(intent);
        return {Code::io_error,"cannot exclusively create and sync artifact intent"};
      }
      if(!sync_parent(parent))return {Code::io_error,"artifact intent directory flush was not confirmed"};
    }
    auto contents=inspect(staged,bytes);
    if(contents==Contents::different)return {Code::conflict,"owned staging has mismatched bytes; preserved for inspection"};
    if(contents==Contents::error)return {Code::io_error,"cannot inspect owned artifact staging"};
    if(contents==Contents::prefix){
      // Matching durable intent and exact bounded prefix establish ownership.
      if(!remove_owned(staged))return {Code::io_error,"cannot recover owned partial artifact staging"};
      contents=Contents::absent;
    }
    if(contents==Contents::absent){
      bool created=false;if(!create_synced(staged,bytes,created))return {Code::io_error,"cannot exclusively create and sync owned artifact staging"};
    }else if(!sync_existing(staged))return {Code::io_error,"complete artifact staging could not be flushed"};
    if(!rename_new(staged,target)){
      if(inspect(target,bytes)==Contents::complete)return finish_existing(target,intent,staged,wire,bytes);
      if(presence(target)!=Presence::absent)return {Code::conflict,"artifact target appeared during publication; all existing bytes were preserved"};
      return {Code::io_error,"atomic no-replace artifact publication failed"};
    }
    return finish_existing(target,intent,staged,wire,bytes);
  }catch(const std::exception& error){return {Code::io_error,error.what()};}
}
}
