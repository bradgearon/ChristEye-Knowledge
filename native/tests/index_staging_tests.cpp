#include "sealed_index.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace anaiero;
namespace fs=std::filesystem;
namespace {
void expect(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
struct Temporary{fs::path path;explicit Temporary(const std::string& label){path=fs::temp_directory_path()/("anaiero-index-staging-"+label+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(path/"nodes");}~Temporary(){std::error_code error;fs::remove_all(path,error);}};
std::size_t files(const fs::path& path){return static_cast<std::size_t>(std::distance(fs::directory_iterator(path),fs::directory_iterator{}));}
std::string key(unsigned i){return "key/"+storage::number(i);}
void spill_and_shared_nodes(){
  Temporary tmp("count-bound");storage::SealedIndex index(tmp.path/"nodes");sdk::Digest root;
  index.begin();for(unsigned i=0;i<1536;++i)root=index.put(root,key(i),"first-"+std::to_string(i));
  index.publish_cleanup(root);index.finish_cleanup(root);index.audit(root);
  expect(index.stats.staged_nodes>0&&index.stats.staged_spills>0&&index.stats.transient_nodes_elided>0,"large update exercises both staging and bounded spill fallback");
  expect(index.stats.cache_peak_nodes<=8192&&index.stats.cache_peak_bytes<=32U*1024U*1024U,"every staged path version shares the existing fixed cache bound");expect(files(tmp.path/"nodes")==1536,"no transient files survive node-count spill cleanup");
  index.begin();for(unsigned i=0;i<1536;i+=2)root=index.put(root,key(i),"second-"+std::to_string(i));index.publish_cleanup(root);index.finish_cleanup(root);index.audit(root);
  expect(files(tmp.path/"nodes")==1536,"shared unchanged nodes survive while obsolete versions are reclaimed");
  for(unsigned i=0;i<1536;++i){bool found=false;const auto value=index.get(root,key(i),found);expect(found&&value==(i%2==0?"second-":"first-")+std::to_string(i),"spill keeps exact current value");}
}
void byte_bound(){
  Temporary tmp("byte-bound");storage::SealedIndex index(tmp.path/"nodes");sdk::Digest root;index.begin();
  for(unsigned i=0;i<96;++i)root=index.put(root,key(i),std::string(131072,static_cast<char>('a'+i%26)));
  index.publish_cleanup(root);index.finish_cleanup(root);index.audit(root);
  expect(index.stats.staged_spills>0&&index.stats.cache_peak_nodes<8192&&index.stats.cache_peak_bytes<=32U*1024U*1024U,"large node payload hits byte bound before node-count bound");expect(files(tmp.path/"nodes")==96,"byte-bound spill leaves exactly reachable files");
}
void publication_cuts(){
  Temporary tmp("cuts");sdk::Digest original,unpublished;
  {
    storage::SealedIndex index(tmp.path/"nodes");index.begin();for(unsigned i=0;i<96;++i)original=index.put(original,key(i),"original");index.publish_cleanup(original);index.finish_cleanup(original);
    index.begin();unpublished=original;for(unsigned i=0;i<48;++i)unpublished=index.put(unpublished,key(i),"changed");
    expect(files(tmp.path/"nodes")==96,"unpublished staged path versions do not touch original files");
    // Destructor simulates loss of bounded staged derived memory before flush.
  }
  {
    storage::SealedIndex index(tmp.path/"nodes");index.audit(original);index.begin();sdk::Digest replay=original;for(unsigned i=0;i<48;++i)replay=index.put(replay,key(i),"changed");expect(replay==unpublished,"canonical replay produces exact staged root identity");index.publish_cleanup(replay);
    // Crash before coverage publication: the prior root remains intact, and a
    // GC intent for a different root must not delete its nodes.
  }
  {
    storage::SealedIndex index(tmp.path/"nodes");index.finish_cleanup(original);index.audit(original);bool found=false;expect(index.get(original,key(0),found)=="original"&&found,"unpublished cleanup cannot remove prior coverage");index.begin();sdk::Digest replay=original;for(unsigned i=0;i<48;++i)replay=index.put(replay,key(i),"changed");expect(replay==unpublished,"replay reconciles already written cache nodes");index.publish_cleanup(replay);index.finish_cleanup(replay);index.audit(replay);expect(files(tmp.path/"nodes")==96,"replayed publication reclaims obsolete files exactly");
  }
}
void deferred_filesystem_admission(){
  Temporary tmp("deferred-filesystem");
  // An intentionally unusable directory proves that transient staging does
  // not probe the filesystem. No node can be published through this path.
  const auto blocked=tmp.path/"not-a-directory";
  {std::ofstream output(blocked,std::ios::binary);output<<"preserve";}
  storage::SealedIndex delayed(blocked);delayed.begin();sdk::Digest root;
  for(unsigned i=0;i<32;++i)root=delayed.put(root,key(i),"temporary");
  expect(delayed.stats.nodes_written==0&&delayed.stats.staged_nodes>32,"staging does no filesystem work before publication");
  bool rejected=false;try{delayed.publish_cleanup(root);}catch(const storage::Failure&){rejected=true;}
  expect(rejected,"invalid storage destination fails at actual publication");

  storage::SealedIndex index(tmp.path/"nodes");index.begin();root=index.put({},key(0),"authentic");
  {std::ofstream output(tmp.path/"nodes"/root,std::ios::binary);output<<"unrelated bytes";}
  rejected=false;try{index.publish_cleanup(root);}catch(const storage::Failure& failure){rejected=failure.code==sdk::Code::corrupt;}
  expect(rejected,"deferred CAS still rejects an altered existing reachable node");
  std::ifstream input(tmp.path/"nodes"/root,std::ios::binary);const std::string preserved{std::istreambuf_iterator<char>(input),{}};
  expect(preserved=="unrelated bytes","failed CAS never overwrites existing bytes");
}
}
int main(){try{spill_and_shared_nodes();byte_bound();publication_cuts();deferred_filesystem_admission();std::cout<<"bounded index staging, spill, shared-node cleanup, publication cuts and deferred filesystem admission passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
