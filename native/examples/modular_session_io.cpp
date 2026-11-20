#include "modular_session_io.hpp"
#include <charconv>
#include <chrono>
#include <fstream>
#include <iterator>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace anaiero::operator_io {
namespace s=anaiero::sdk;
namespace ev=anaiero::sdk::evidence;
namespace fs=std::filesystem;
namespace {
bool number(std::string_view text,std::uint64_t& value){
  const auto r=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&r.ec==std::errc{}&&r.ptr==text.data()+text.size();
}
}
bool read_file(const fs::path& path,std::string& bytes,std::size_t maximum=8U*1024U*1024U){
  std::error_code error;
  if(fs::is_symlink(path,error)||error)return false;
  const auto size=fs::file_size(path,error);
  if(error||size>maximum)return false;
  std::ifstream in(path,std::ios::binary);
  if(!in)return false;
  bytes.assign(std::istreambuf_iterator<char>(in),{});
  return !in.bad()&&bytes.size()==size;
}
s::Result stored_budget(ev::ICommitStore& store,std::uint64_t& maximum,bool& exists){
  ev::StoredRecord record;
  const auto found=store.latest("operator.policy","capacity",record);
  exists=false;
  if(found.code==s::Code::not_found)return {};
  if(!found)return found;
  std::vector<std::string> fields;
  if(record.record.tombstone||!s::unpack(record.record.payload,fields,2,1024)||fields.size()!=2||fields[0]!="store-budget/1"||
     !number(fields[1],maximum)||std::to_string(maximum)!=fields[1]||maximum<65536)return {s::Code::corrupt,"persisted store budget is invalid"};
  exists=true;return {};
}
bool canonical_root_exists(const fs::path& directory){
  std::error_code error;
  const auto status=fs::symlink_status(directory/"ROOT",error);
  return !error&&fs::is_regular_file(status);
}
std::uint64_t now(){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
  std::chrono::system_clock::now().time_since_epoch()).count());
  }
fs::path executable(const char* argv0){
#ifdef _WIN32
  (void)argv0;
  std::wstring path(32768,L'\0');
  const auto n=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
  if(!n||n>=path.size())return {};
  path.resize(n);
  return fs::path(path);
#else
  std::string path(8192,'\0');
  const auto n=::readlink("/proc/self/exe",path.data(),path.size());
  if(n>0&&static_cast<std::size_t>(n)<path.size()){path.resize(static_cast<std::size_t>(n));
  return fs::path(path);
  }
  return fs::absolute(argv0);
#endif
}
}
