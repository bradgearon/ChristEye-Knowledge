#include "mixed_campaign_filesystem.hpp"
#include "native_paths.hpp"
#include <filesystem>
#include <fstream>
namespace anaiero::qualification::campaign_io {
namespace fs=std::filesystem;
namespace os=christeye::living::durable::os;
sdk::Result read_file(const fs::path& path,std::size_t maximum,sdk::Bytes& bytes){
  std::error_code error;
  if(!fs::is_regular_file(fs::symlink_status(path,error))||error)return {sdk::Code::invalid,"campaign input is not a regular file: "+utf8(path)};
  const auto size=fs::file_size(path,error);
  if(error||size>maximum)return {sdk::Code::capacity,"campaign input exceeds its byte bound"};
  std::ifstream input(path,std::ios::binary);if(!input)return {sdk::Code::io_error,"cannot read campaign input"};
  bytes.assign(static_cast<std::size_t>(size),'\0');
  if((size&&!input.read(bytes.data(),static_cast<std::streamsize>(size)))||input.peek()!=std::char_traits<char>::eof()||input.bad())
    return {sdk::Code::corrupt,"campaign input changed during bounded read"};
  return {};
}
sdk::Result lease(const fs::path& directory,os::File& lock){
  std::error_code error;
  if(!fs::is_directory(fs::symlink_status(directory,error))||error)return {sdk::Code::invalid,"campaign directory must be an existing regular directory"};
  const auto path=directory/"campaign.lock";const auto status=fs::symlink_status(path,error);
  if(error&&error!=std::errc::no_such_file_or_directory)return {sdk::Code::io_error,"cannot inspect campaign writer lock"};
  if(fs::exists(status)&&!fs::is_regular_file(status))return {sdk::Code::conflict,"campaign writer lock is not a regular file"};
  if(!lock.open(path))return {sdk::Code::io_error,"cannot open campaign writer lock"};
  if(!lock.lock_exclusive())return {sdk::Code::busy,"campaign already has an active writer; status is a cached checkpoint"};
  return {};
}
sdk::Result publish_cache(const fs::path& directory,std::string_view bytes){
  std::error_code error;const auto path=directory/"status.json";const auto status=fs::symlink_status(path,error);
  if(error&&error!=std::errc::no_such_file_or_directory)return {sdk::Code::io_error,"cannot inspect campaign status cache"};
  if(fs::exists(status)&&!fs::is_regular_file(status))return {sdk::Code::conflict,"campaign status cache is not a regular file"};
  return os::atomic_write(path,bytes)?sdk::Result{}:sdk::Result{sdk::Code::io_error,"cannot publish campaign status cache; canonical progress remains retained"};
}
}
