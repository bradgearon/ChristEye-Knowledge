// Native storage responsibility boundaries: exact staging recovery and
// file/directory durability adapter without initializing a canonical store.
#include "native_store_filesystem.hpp"
#include "native_store_recovery.hpp"
#include "sealed_index.hpp"
#include <filesystem>
#include <iostream>

namespace storage=anaiero::storage;
namespace fs=std::filesystem;
namespace sdk=anaiero::sdk;
#define REQUIRE(test) do {if(!(test)){std::cerr<<"storage-boundaries line "<<__LINE__<<": " #test "\n";return 1;}}while(false)

int main(int argc,char** argv){
  if(argc!=2)return 2;
  const fs::path root=fs::path(argv[1]);
  fs::remove_all(root);
  storage::fsops::directory(root);
  storage::fsops::directory(root/"blobs");
  storage::fsops::directory(root/"transactions");
  storage::fsops::directory(root/"index");
  storage::fsops::directory(root/"index"/"nodes-a");
  const auto store=sdk::sha256("native-store-boundaries");
  const auto blob=sdk::sha256("uncommitted blob");
  const auto txn=storage::number(1)+".txn";
  storage::write_file(root/"blobs"/blob,"uncommitted blob");
  storage::write_file(root/"transactions"/txn,"uncommitted transaction");
  storage::write_file(root/"STAGING",storage::seal("ANAIERO-STAGING/1",
    {store,storage::number(1),"",blob}));
  storage::recovery::recover_staging(root,store,0,{});
  REQUIRE(!storage::regular_exists(root/"STAGING"));
  REQUIRE(!storage::regular_exists(root/"blobs"/blob));
  REQUIRE(!storage::regular_exists(root/"transactions"/txn));
  storage::write_file(root/"blobs"/blob,"accepted blob");
  const auto root_digest=sdk::sha256("accepted root");
  storage::write_file(root/"STAGING",storage::seal("ANAIERO-STAGING/1",
    {store,storage::number(1),root_digest,blob}));
  storage::recovery::recover_staging(root,store,1,root_digest);
  REQUIRE(storage::regular_exists(root/"blobs"/blob));
  REQUIRE(!storage::regular_exists(root/"STAGING"));
  storage::write_file(root/"STAGING",storage::seal("ANAIERO-STAGING/1",
    {store,storage::number(2),"wrong predecessor",blob}));
  bool rejected=false;
  try{storage::recovery::recover_staging(root,store,1,root_digest);}
  catch(const storage::Failure& error){rejected=error.code==sdk::Code::corrupt;}
  REQUIRE(rejected);
  REQUIRE(storage::regular_exists(root/"STAGING"));
  storage::remove_file(root/"STAGING");
  storage::fsops::directory(root/"index"/"nodes-b");
  storage::write_file(root/"index"/"nodes-b"/"old","obsolete");
  storage::recovery::cleanup_inactive_index(root,"nodes-a");
  REQUIRE(!fs::exists(root/"index"/"nodes-b"));
  REQUIRE(fs::is_directory(root/"index"/"nodes-a"));
  storage::fsops::measure_coexistence(root,root/"index"/"nodes-a",root/"index"/"nodes-b",
    *([]()->sdk::evidence::CompactionReport*{static sdk::evidence::CompactionReport report;return &report;})());
  fs::remove_all(root);
  std::cout<<"native storage boundary recovery passed\n";
  return 0;
}
