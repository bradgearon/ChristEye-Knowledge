#pragma once
// Host-only operating-system file durability and single-writer lock adapter.
// Independent physical safety equipment must not depend on this service.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
namespace christeye::living::durable::os {
namespace fs=std::filesystem;
class File final {
#ifdef _WIN32
  HANDLE handle_=INVALID_HANDLE_VALUE;
  OVERLAPPED overlapped_{};
#else
  int handle_=-1;
#endif
  bool locked_=false;
public:
  File()=default;
  File(const File&)=delete;
  File& operator=(const File&)=delete;
  ~File(){close();}
  bool valid() const noexcept {
#ifdef _WIN32
    return handle_ != INVALID_HANDLE_VALUE;
#else
    return handle_>=0;
#endif
  }
  bool open(const fs::path& path, bool truncate=false, bool append=false) noexcept {
    close();
#ifdef _WIN32
    handle_=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,
      FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr,truncate?CREATE_ALWAYS:OPEN_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,nullptr);
    if(!valid())return false;
    if(append){LARGE_INTEGER zero{},cursor{}; if(!SetFilePointerEx(handle_,zero,&cursor,FILE_END)){close();return false;}}
#else
    int flags=O_RDWR|O_CREAT;
    if(truncate)flags|=O_TRUNC;
    if(append)flags|=O_APPEND;
#ifdef O_CLOEXEC
    flags|=O_CLOEXEC;
#endif
    handle_=::open(path.c_str(),flags,0600);
#endif
    return valid();
  }
  bool lock_exclusive() noexcept {
    if(!valid())return false;
#ifdef _WIN32
    overlapped_={};
    locked_=LockFileEx(handle_,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,
                       0,1,0,&overlapped_)!=0;
#else
    locked_=::flock(handle_,LOCK_EX|LOCK_NB)==0;
#endif
    return locked_;
  }
  bool write_all(std::string_view bytes) noexcept {
    if(!valid())return false;
    std::size_t offset=0;
    while(offset<bytes.size()){
#ifdef _WIN32
      const auto n=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,1u<<20));
      DWORD written=0;
      if(!WriteFile(handle_,bytes.data()+offset,n,&written,nullptr)||written==0)return false;
      offset+=written;
#else
      const auto n=::write(handle_,bytes.data()+offset,bytes.size()-offset);
      if(n<0 && errno==EINTR)continue;
      if(n<=0)return false;
      offset+=static_cast<std::size_t>(n);
#endif
    }
    return true;
  }
  bool sync() noexcept {
    if(!valid())return false;
#ifdef _WIN32
    return FlushFileBuffers(handle_)!=0;
#else
    return ::fsync(handle_)==0;
#endif
  }
  bool resize(std::uint64_t size) noexcept {
    if(!valid())return false;
#ifdef _WIN32
    LARGE_INTEGER offset{};offset.QuadPart=static_cast<LONGLONG>(size);
    return SetFilePointerEx(handle_,offset,nullptr,FILE_BEGIN)!=0 && SetEndOfFile(handle_)!=0;
#else
    return ::ftruncate(handle_,static_cast<off_t>(size))==0;
#endif
  }
  void close() noexcept {
    if(!valid())return;
    if(locked_){
#ifdef _WIN32
      UnlockFileEx(handle_,0,1,0,&overlapped_);
#else
      ::flock(handle_,LOCK_UN);
#endif
      locked_=false;
    }
#ifdef _WIN32
    CloseHandle(handle_);handle_=INVALID_HANDLE_VALUE;
#else
    ::close(handle_);handle_=-1;
#endif
  }
};
inline bool sync_directory(const fs::path& directory) noexcept {
#ifdef _WIN32
  // Windows rename uses MOVEFILE_WRITE_THROUGH; Win32 directory FlushFileBuffers
  // is not universally supported. Power-loss guarantees remain filesystem-dependent.
  (void)directory;
  return true;
#else
  const int dir=::open(directory.c_str(),O_RDONLY
#ifdef O_DIRECTORY
      | O_DIRECTORY
#endif
  );
  if(dir<0)return false;
  const bool ok=::fsync(dir)==0;
  ::close(dir);
  return ok;
#endif
}
inline bool replace_file(const fs::path& temp,const fs::path& final) noexcept {
#ifdef _WIN32
  if(!MoveFileExW(temp.c_str(),final.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
#else
  if(::rename(temp.c_str(),final.c_str())!=0)return false;
#endif
  return sync_directory(final.parent_path());
}
inline bool atomic_write(const fs::path& path,std::string_view contents) noexcept {
  fs::path temp=path;temp+=".tmp"; // safe under held writer lock
  std::error_code ec;
  if(fs::is_symlink(temp,ec))return false;
  File file;
  if(!file.open(temp,true)||!file.write_all(contents)||!file.sync())return false;
  file.close();
  return replace_file(temp,path);
}
inline bool append_synced(const fs::path& path,std::string_view record) noexcept {
  File file;
  if(!file.open(path,false,true))return false;
  const bool ok=file.write_all(record)&&file.sync();
  file.close();
  return ok;
}
inline bool truncate_synced(const fs::path& path,std::uint64_t size) noexcept {
  File file;
  if(!file.open(path))return false;
  const bool ok=file.resize(size)&&file.sync();
  file.close();
  return ok;
}
} // namespace christeye::living::durable::os
