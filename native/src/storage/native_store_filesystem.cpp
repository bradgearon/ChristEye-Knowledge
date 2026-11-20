#include "native_store_filesystem.hpp"
#ifndef _WIN32
#include <sys/statvfs.h>
#endif

namespace anaiero::storage::fsops {
using namespace sdk::evidence;
void require_file_capacity(const fs::path& path,std::uint64_t temporary,std::uint64_t reserve){
#ifndef _WIN32
  struct statvfs status{};
  if(statvfs(path.c_str(),&status)!=0)throw Failure(Code::io_error,"cannot measure filesystem file-node headroom");
  const auto available=static_cast<std::uint64_t>(status.f_favail);
  if(status.f_files!=0&&available!=any_generation&&
     (available<reserve||temporary>available-reserve))
    throw Failure(Code::capacity,"filesystem file-node admission headroom exhausted; free bytes do not imply free file entries");
#else
  (void)path;(void)temporary;(void)reserve;
#endif
}

void measure_coexistence(const fs::path& store,const fs::path& old_index,const fs::path& new_index,CompactionReport& sample){
  for(const auto& entry:fs::recursive_directory_iterator(store)){
    const auto type=entry.symlink_status().type();
    if(type==fs::file_type::directory)++sample.coexistence_directories;
    else if(type==fs::file_type::regular){
      ++sample.coexistence_files;
      if(entry.path().parent_path()==old_index)++sample.coexistence_old_index_files;
      if(entry.path().parent_path()==new_index)++sample.coexistence_new_index_files;
    }else throw Failure(Code::corrupt,"unexpected entry in compaction file inventory");
  }
  sample.coexistence_measured=true;
}

void directory(const fs::path& p){
  std::error_code ec;const auto s=fs::symlink_status(p,ec);
  if(!ec&&fs::is_symlink(s))throw Failure(Code::corrupt,"storage directory symlink rejected");
  ec.clear();fs::create_directories(p,ec);if(ec||!fs::is_directory(p))throw Failure(Code::io_error,"cannot create storage directory");
}
}
