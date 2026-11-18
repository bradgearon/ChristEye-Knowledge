#include "qualification.hpp"
#include "native_paths.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

#define ANAIERO_STRINGIFY_DETAIL(value) #value
#define ANAIERO_STRINGIFY(value) ANAIERO_STRINGIFY_DETAIL(value)
namespace anaiero::qualification {
namespace fs=std::filesystem;
namespace {
bool add(std::uint64_t& target,std::uint64_t value){
  if(value>std::numeric_limits<std::uint64_t>::max()-target)return false;
  target+=value;return true;
}
#ifdef _WIN32
std::uint64_t ticks(const FILETIME& value){return (static_cast<std::uint64_t>(value.dwHighDateTime)<<32)|value.dwLowDateTime;}
#endif
}
sdk::Result read_resources(Resources& output){
  Resources result;
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS counters{};counters.cb=sizeof(counters);
  FILETIME created{},exited{},kernel{},user{};
  if(!GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters))||!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))
    return {sdk::Code::io_error,"native process resource measurement failed"};
  result.resident_bytes=static_cast<std::uint64_t>(counters.WorkingSetSize);
  result.peak_resident_bytes=static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
  result.cpu_ns=(ticks(kernel)+ticks(user))*100;
#else
  struct rusage usage{};
  if(getrusage(RUSAGE_SELF,&usage)!=0)return {sdk::Code::io_error,"getrusage failed"};
  std::ifstream current("/proc/self/statm");std::uint64_t virtual_pages=0,resident_pages=0;
  const long page_size=sysconf(_SC_PAGESIZE);
  if(!(current>>virtual_pages>>resident_pages)||page_size<=0)return {sdk::Code::io_error,"native resident memory measurement failed"};
  result.resident_bytes=resident_pages*static_cast<std::uint64_t>(page_size);
  result.peak_resident_bytes=static_cast<std::uint64_t>(usage.ru_maxrss)*1024;
  result.cpu_ns=(static_cast<std::uint64_t>(usage.ru_utime.tv_sec)+static_cast<std::uint64_t>(usage.ru_stime.tv_sec))*1000000000+
    (static_cast<std::uint64_t>(usage.ru_utime.tv_usec)+static_cast<std::uint64_t>(usage.ru_stime.tv_usec))*1000;
  result.voluntary_context_switches=static_cast<std::uint64_t>(usage.ru_nvcsw);
  result.involuntary_context_switches=static_cast<std::uint64_t>(usage.ru_nivcsw);
  result.context_switches_available=true;
#endif
  output=result;return {};
}
sdk::Result measure_disk(std::string_view directory,std::uint64_t maximum_files,DiskUsage& output){
  DiskUsage result;result.allocated_bytes_available=true;
  try{
    for(const auto& item:fs::recursive_directory_iterator(native_path(directory))){
      const auto status=item.symlink_status();
      if(fs::is_symlink(status))return {sdk::Code::invalid,"qualification directory contains a symlink"};
      if(fs::is_directory(status))continue;
      if(!fs::is_regular_file(status))return {sdk::Code::invalid,"qualification directory contains a nonregular entry"};
      if(++result.files>maximum_files)return {sdk::Code::capacity,"qualification disk inventory exceeds declared file bound"};
      if(!add(result.logical_bytes,item.file_size()))return {sdk::Code::capacity,"qualification disk inventory overflow"};
#ifdef _WIN32
      const auto file=CreateFileW(item.path().c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
      if(file==INVALID_HANDLE_VALUE)return {sdk::Code::io_error,"allocated file handle measurement failed"};
      FILE_STANDARD_INFO info{};const bool measured=GetFileInformationByHandleEx(file,FileStandardInfo,&info,sizeof(info))!=0;
      CloseHandle(file);
      if(!measured||info.AllocationSize.QuadPart<0)return {sdk::Code::io_error,"allocated file measurement failed"};
      const auto allocated=static_cast<std::uint64_t>(info.AllocationSize.QuadPart);
#else
      struct stat info{};
      if(lstat(item.path().c_str(),&info)!=0||info.st_blocks<0)return {sdk::Code::io_error,"allocated file measurement failed"};
      const auto allocated=static_cast<std::uint64_t>(info.st_blocks)*512;
#endif
      if(!add(result.allocated_bytes,allocated))return {sdk::Code::capacity,"allocated disk inventory overflow"};
    }
  }catch(const fs::filesystem_error& error){return {sdk::Code::io_error,error.what()};}
  output=result;return {};
}
sdk::Result read_environment(std::string_view directory,Environment& output){
  Environment result;result.logical_processors=std::thread::hardware_concurrency();
#ifdef _WIN32
  OSVERSIONINFOW version{};version.dwOSVersionInfoSize=sizeof(version);
  using VersionFunction=LONG (WINAPI*)(OSVERSIONINFOW*);
  const auto module=GetModuleHandleW(L"ntdll.dll");const auto symbol=module?GetProcAddress(module,"RtlGetVersion"):nullptr;
  VersionFunction function=nullptr;static_assert(sizeof(function)==sizeof(symbol));std::memcpy(&function,&symbol,sizeof(function));
  if(function&&function(&version)==0)result.operating_system="Windows "+std::to_string(version.dwMajorVersion)+'.'+std::to_string(version.dwMinorVersion)+'.'+std::to_string(version.dwBuildNumber);
  else result.operating_system="Windows (kernel version unavailable)";
  SYSTEM_INFO system{};GetNativeSystemInfo(&system);
  result.architecture=system.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64?"x86_64":system.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_ARM64?"arm64":"other";
  wchar_t volume[MAX_PATH]{},filesystem[MAX_PATH]{};
  const auto path=fs::absolute(native_path(directory));
  if(!GetVolumePathNameW(path.c_str(),volume,MAX_PATH)||!GetVolumeInformationW(volume,nullptr,0,nullptr,nullptr,nullptr,filesystem,MAX_PATH))
    return {sdk::Code::io_error,"filesystem environment measurement failed"};
  for(const auto ch:filesystem){if(ch==0)break;result.filesystem+=ch<=127?static_cast<char>(ch):'?';}
#else
  struct utsname system{};struct statfs filesystem{};
  if(uname(&system)!=0||statfs(native_path(directory).c_str(),&filesystem)!=0)return {sdk::Code::io_error,"native environment measurement failed"};
  result.operating_system=std::string(system.sysname)+' '+system.release;result.architecture=system.machine;
  std::ostringstream kind;kind<<"linux-statfs-0x"<<std::hex<<static_cast<unsigned long long>(filesystem.f_type);result.filesystem=kind.str();
#endif
#ifdef _MSC_VER
  result.compiler="MSVC " ANAIERO_STRINGIFY(_MSC_VER);
#elif defined(__clang__)
  result.compiler="Clang " __clang_version__;
#elif defined(__GNUC__)
  result.compiler="GCC " __VERSION__;
#else
  result.compiler="unknown";
#endif
#ifdef ANAIERO_BUILTIN_BUILD_ID
  result.build_identity=ANAIERO_BUILTIN_BUILD_ID;
#else
  result.build_identity="unavailable";
#endif
  output=std::move(result);return {};
}
}
