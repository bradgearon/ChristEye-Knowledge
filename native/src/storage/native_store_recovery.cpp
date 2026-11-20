#include "native_store_recovery.hpp"
#include "christeye/living/durable_io.hpp"

namespace anaiero::storage::recovery {
namespace os=christeye::living::durable::os;
constexpr std::size_t decode_limit=64U*1024U*1024U;
void recover_staging(const fs::path& path,std::string_view store_id,
                     std::uint64_t root_generation,const Digest& root_digest){

  // A writer can die before renaming its first STAGING temporary. Its name
  // is fixed and owned by the native writer; it is never canonical authority.
  remove_file(path/"STAGING.tmp");
  if(!regular_exists(path/"STAGING"))return;
  const auto f=unseal(read_file(path/"STAGING",decode_limit),"ANAIERO-STAGING/1");
  if(f.size()<3||f[0]!=store_id)throw Failure(Code::corrupt,"invalid staging owner");
  const auto generation=parse_number(f[1]);
  if(generation>root_generation){
    if(generation!=root_generation+1||f[2]!=root_digest)throw Failure(Code::corrupt,"staging predecessor mismatch");
    for(std::size_t i=3;i<f.size();++i){if(!valid_digest(f[i]))throw Failure(Code::corrupt,"bad staged blob identity");remove_file(path/"blobs"/f[i]);remove_file(path/"blobs"/(f[i]+".tmp"));}
    remove_file(path/"transactions"/(number(generation)+".txn"));remove_file(path/"transactions"/(number(generation)+".txn.tmp"));
    remove_file(path/"ROOT.tmp");
  }else if(generation!=root_generation)throw Failure(Code::corrupt,"stale staging generation");
  remove_file(path/"STAGING");
}
void cleanup_inactive_index(const fs::path& path,std::string_view active_directory){

  const auto obsolete=path/"index"/(active_directory=="nodes-a"?"nodes-b":"nodes-a");
  std::error_code ec;
  if(!fs::exists(obsolete,ec)){if(ec)throw Failure(Code::io_error,"cannot inspect obsolete index generation");return;}
  if(fs::is_symlink(obsolete))throw Failure(Code::corrupt,"obsolete index directory symlink rejected");
  fs::remove_all(obsolete,ec);if(ec||!os::sync_directory(path/"index"))throw Failure(Code::io_error,"obsolete index generation cleanup failed");
}
}
