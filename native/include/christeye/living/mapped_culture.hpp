#pragma once
// Host-only read-only mapping of a *committed* CEVRUNSTATE/1 checkpoint.
// Mapping is for fast visualization. Never infer persistence from mmap alone.
#include <cstddef>
#include <filesystem>
#include <span>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace christeye::living::sim3 {
class MappedCulture final {
#ifdef _WIN32
  HANDLE file_=INVALID_HANDLE_VALUE;
  HANDLE mapping_=nullptr;
#else
  int file_=-1;
#endif
  const char* data_=nullptr;
  std::size_t size_=0;
public:
  MappedCulture()=default;
  MappedCulture(const MappedCulture&)=delete;
  MappedCulture&operator=(const MappedCulture&)=delete;
  ~MappedCulture(){close();}
  void close()noexcept{
#ifdef _WIN32
    if(data_)UnmapViewOfFile(data_);
    if(mapping_)CloseHandle(mapping_);
    if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);
    file_=INVALID_HANDLE_VALUE;mapping_=nullptr;
#else
    if(data_)::munmap(const_cast<char*>(data_),size_);
    if(file_>=0)::close(file_);
    file_=-1;
#endif
    data_=nullptr;size_=0;
  }
  bool open(const std::filesystem::path& path)noexcept{
    close();
#ifdef _WIN32
    file_=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,
                      nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file_==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER length{};
    if(!GetFileSizeEx(file_,&length)||length.QuadPart<=0||length.QuadPart>1024*1024){close();return false;}
    size_=static_cast<std::size_t>(length.QuadPart);
    mapping_=CreateFileMappingW(file_,nullptr,PAGE_READONLY,0,0,nullptr);
    if(!mapping_){close();return false;}
    data_=static_cast<const char*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,0));
    if(!data_){close();return false;}
#else
    file_=::open(path.c_str(),O_RDONLY);
    if(file_<0)return false;
    struct stat info{};
    if(::fstat(file_,&info)!=0||info.st_size<=0||info.st_size>1024*1024){close();return false;}
    size_=static_cast<std::size_t>(info.st_size);
    void* mapped=::mmap(nullptr,size_,PROT_READ,MAP_PRIVATE,file_,0);
    if(mapped==MAP_FAILED){close();return false;}
    data_=static_cast<const char*>(mapped);
#endif
    return true;
  }
  std::span<const char> bytes()const noexcept{return {data_,size_};}
};
} // namespace christeye::living::sim3
