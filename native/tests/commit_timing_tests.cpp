#include "anaiero/sdk/evidence.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#ifndef _WIN32
#include <sys/statvfs.h>
#endif

namespace ev=anaiero::sdk::evidence;
namespace sdk=anaiero::sdk;
namespace fs=std::filesystem;
namespace {
void expect(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void require(sdk::Result r){if(!r)throw std::runtime_error(r.message);}
struct Temporary {
  fs::path path=fs::temp_directory_path()/("anaiero-commit-measurement-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Temporary(){fs::create_directory(path);}
  ~Temporary(){std::error_code error;fs::remove_all(path,error);}
};
std::unique_ptr<ev::ICommitStore> open(const fs::path& path,bool measured){
  ev::StoreOptions options;options.measure_commit_stages=measured;std::unique_ptr<ev::ICommitStore> store;
  require(ev::open_native_store(path.string(),options,store));return store;
}
ev::CommitBatch request(){
  ev::CommitBatch batch;batch.operation_id="measurement-original";batch.request_digest=sdk::sha256(batch.operation_id);
  const std::string bytes(512,'m');const auto digest=sdk::sha256(bytes);
  batch.blobs={{digest,bytes,{}}};batch.records={{"measurement","current","accepted",{digest},false}};return batch;
}
void check_sum(const ev::CommitTimings& t){
  expect(t.enabled&&t.total_ns>0,"enabled measurements have a monotonic duration");
  const auto sum=t.lock_wait_ns+t.validation_ns+t.staging_ns+t.blobs_ns+t.transaction_ns+t.root_ns+t.index_ns+t.cleanup_ns;
  expect(sum==t.total_ns,"all commit stages are disjoint and cover the attempt including lock wait");
}
void normal_and_duplicate(){
  Temporary temp;auto store=open(temp.path,false);const auto batch=request();ev::CommitReceipt receipt;
  require(store->commit(batch,receipt));const auto unmeasured=ev::native_commit_timings(*store);
  expect(!unmeasured.enabled&&unmeasured.total_ns==0,"default production commits do not read instrumentation clocks");
  const auto root=store->info().root_digest;store.reset();store=open(temp.path,true);
  require(store->commit(batch,receipt));auto timing=ev::native_commit_timings(*store);check_sum(timing);
  expect(timing.duplicate&&timing.committed&&timing.generation==1&&timing.staging_ns==0&&timing.index_ns==0,
    "an exact retry measures admission without claiming another publication");
  expect(store->info().root_digest==root,"measurement and exact retry preserve the canonical root");
  auto changed=batch;changed.records[0].payload="changed";
  expect(store->commit(changed,receipt).code==sdk::Code::conflict,"changed operation retry still rejects");
  timing=ev::native_commit_timings(*store);check_sum(timing);
  expect(timing.result==sdk::Code::conflict&&!timing.committed&&!timing.duplicate&&timing.blobs_ns==0,
    "rejected attempt resets prior timings and cannot claim publication");
  ev::AuditReport audit;require(store->audit(audit));
}
void publication_faults(){
  for(const auto cut:{ev::FaultPoint::none,ev::FaultPoint::after_objects,ev::FaultPoint::after_transaction,
      ev::FaultPoint::after_root,ev::FaultPoint::before_index,ev::FaultPoint::after_index}){
    Temporary temp;auto store=open(temp.path,true);const auto batch=request();ev::CommitReceipt receipt;
    ev::set_native_store_fault(*store,cut);const auto result=store->commit(batch,receipt);const auto timing=ev::native_commit_timings(*store);
    check_sum(timing);expect(timing.result==result.code,"measurements retain actual attempt result");
    const bool published=cut==ev::FaultPoint::none||cut==ev::FaultPoint::after_root||cut==ev::FaultPoint::before_index||cut==ev::FaultPoint::after_index;
    expect(timing.committed==published&&!timing.duplicate,"timings distinguish root publication from failed pre-root work");
    expect(timing.validation_ns>0&&timing.staging_ns>0&&timing.blobs_ns>0,"real admission and blob writes measured");
    if(cut==ev::FaultPoint::after_objects)expect(timing.transaction_ns==0&&timing.root_ns==0,"unreached phases stay zero");
    if(cut==ev::FaultPoint::after_root||cut==ev::FaultPoint::before_index)expect(timing.index_ns==0,"pre-index failure is not counted as index publication");
    store.reset();store=open(temp.path,true);require(store->commit(batch,receipt));
    expect(receipt.generation==1&&receipt.duplicate==published,"instrumented fault recovery retains exact once-only receipt");
    ev::AuditReport audit;require(store->audit(audit));
  }
}
void abandoned_write_files(){
  Temporary temp;auto store=open(temp.path,true);const auto batch=request();ev::CommitReceipt receipt;
  ev::set_native_store_fault(*store,ev::FaultPoint::after_objects);
  expect(store->commit(batch,receipt).code==sdk::Code::io_error,"cut leaves staged prepublication intent");
  store.reset();const auto blob=temp.path/"blobs"/(batch.blobs.front().digest+".tmp");
  const auto journal=temp.path/"transactions"/"0000000000000001.txn.tmp";
  const auto unrelated=temp.path/"blobs"/"unrelated.tmp";
  for(const auto& path:{blob,journal,temp.path/"ROOT.tmp",temp.path/"STAGING.tmp",unrelated})std::ofstream(path)<<"partial";
  store=open(temp.path,true);
  expect(!fs::exists(blob)&&!fs::exists(journal)&&!fs::exists(temp.path/"ROOT.tmp")&&!fs::exists(temp.path/"STAGING.tmp"),
    "reopen reclaims only bounded temporary files identified by the pending intent");
  expect(fs::exists(unrelated)&&store->generation()==0,"recovery does not erase unrelated files or publish unfinished work");
  require(store->commit(batch,receipt));expect(!receipt.duplicate&&receipt.generation==1,"recovered request commits exactly once");
  ev::AuditReport audit;require(store->audit(audit));
}
void file_node_reserve(){
#ifndef _WIN32
  Temporary temp;struct statvfs volume{};expect(statvfs(temp.path.c_str(),&volume)==0,"measure temporary volume file-node pool");
  if(volume.f_files==0||static_cast<std::uint64_t>(volume.f_favail)==std::numeric_limits<std::uint64_t>::max())return;
  ev::StoreOptions options;options.minimum_free_file_nodes=std::numeric_limits<std::uint64_t>::max();std::unique_ptr<ev::ICommitStore> store;
  require(ev::open_native_store(temp.path.string(),options,store));auto original=store->info();ev::CommitReceipt receipt;
  const auto refused=store->commit(request(),receipt);expect(refused.code==sdk::Code::capacity&&refused.message.find("file-node")!=std::string::npos,
    "finite entry reserve rejects an otherwise byte-admissible commit");
  expect(store->generation()==original.generation&&store->info().root_digest==original.root_digest&&!fs::exists(temp.path/"STAGING"),
    "file-node refusal occurs before staging or publishing canonical work");
#endif
}
}
int main(){try{normal_and_duplicate();publication_faults();abandoned_write_files();file_node_reserve();std::cout<<"commit stage timing, retry classification and publication recovery passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
