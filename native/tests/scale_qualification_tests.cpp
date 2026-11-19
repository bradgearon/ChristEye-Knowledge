#include "scale_qualification.hpp"
#include "anaiero/sdk/lifecycle.hpp"
#include "anaiero/sdk/storage_tools.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace q=anaiero::qualification;
namespace sdk=anaiero::sdk;
namespace ev=sdk::evidence;
namespace lc=sdk::lifecycle;
namespace fs=std::filesystem;
namespace {
void expect(bool v,const std::string& message){if(!v)throw std::runtime_error(message);}
void require(sdk::Result r,const std::string& message){if(!r)throw std::runtime_error(message+": "+std::string(sdk::code_name(r.code))+": "+r.message);}
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);expect(static_cast<bool>(in),"open profile");return {std::istreambuf_iterator<char>(in),{}};}
std::unique_ptr<ev::ICommitStore> open(const fs::path& path){std::unique_ptr<ev::ICommitStore> store;require(ev::open_native_store(path.string(),{},store),"open fixture");return store;}
ev::CommitBatch request(std::string op){ev::CommitBatch b;b.operation_id=std::move(op);b.request_digest=sdk::sha256(b.operation_id);return b;}
ev::CommitReceipt commit(ev::ICommitStore& store,const ev::CommitBatch& b){ev::CommitReceipt receipt;require(store.commit(b,receipt),"fixture commit");return receipt;}
void interrupted_retention_and_compaction(const fs::path& path){
  auto store=open(path);auto lifecycle=std::make_unique<lc::Service>(*store);require(lifecycle->open(),"open lifecycle");
  const sdk::EngineIdentity id{"scale-fault","test.scale/1",sdk::sha256("module"),sdk::sha256("model"),"test.behavior/1","scale-fault-source","logical",1,0};
  const lc::SourceRef ref{id.source_uid,id.epoch};require(lifecycle->register_engine(id,"owner","register"),"register source");require(lifecycle->activate(ref,"activate"),"activate source");
  sdk::EngineState state{0,0,sdk::Phase::ready,{"test.checkpoint/1","initial"},{"test.configuration/1","configuration"}};
  std::vector<sdk::Digest> objects;ev::CommitBatch original;ev::CommitReceipt original_receipt;
  for(unsigned i=0;i<2;++i){
    sdk::PreparedTransition t{id,"transition-"+std::to_string(i),{},sdk::state_digest(state),state.revision,state,{},{}};t.request_sha256=sdk::sha256(t.operation_id);
    if(i){t.target.revision=1;t.target.tick=1;t.target.phase=sdk::Phase::running;t.target.checkpoint.payload="next";
      t.observations={{"test.observation/1","metric","count","unit",1,1}};}
    auto b=request(t.operation_id);require(lifecycle->stage_commit(b,t),"stage transition");
    for(const auto& object:b.blobs)objects.push_back(object.digest);
    const auto receipt=commit(*store,b);if(i){original=b;original_receipt=receipt;}state=t.target;
  }
  require(lifecycle->begin_drain(ref,"drain"),"drain");lc::Source source;require(lifecycle->source(ref,source),"source");
  require(lifecycle->seal(ref,{source.last_sequence,source.last_payload_sha256},"seal"),"seal");require(lifecycle->retire(ref,"retire"),"retire");
  lc::MaintenanceRequest cleanup{"cleanup",ref,1,0,lc::HistoryAvailability::expired,{},objects};lc::MaintenancePlan plan;
  require(lifecycle->review_maintenance(cleanup,"review",plan),"review");require(lifecycle->activate_maintenance("cleanup",0,"expire"),"expire");
  const auto expired=store->info();ev::set_native_store_fault(*store,ev::FaultPoint::after_transaction);
  expect(!lifecycle->execute_maintenance("cleanup","reclaim").committed(),"delete-before-accounting injection should interrupt publication");
  expect(store->generation()==expired.generation,"interrupted reclaim must retain previous generation");
  expect(!fs::exists(path/"blobs"/objects.front()),"fault occurred after real physical deletion");
  lifecycle.reset();store.reset();store=open(path);lifecycle=std::make_unique<lc::Service>(*store);require(lifecycle->open(),"reopen lifecycle");
  require(lifecycle->execute_maintenance("cleanup","reclaim"),"resume physical/accounting cleanup");
  const auto completed=store->generation();require(lifecycle->execute_maintenance("cleanup","reclaim"),"exact resumed cleanup retry");
  expect(store->generation()==completed,"completed retry must not charge/reclaim twice");
  for(const auto& digest:objects){ev::BlobInfo b;require(store->blob_info(digest,b),"expired blob metadata");expect(!b.present&&b.expired_generation,"all expired files removed");}
  lc::CapacityStatus capacity;require(lifecycle->capacity(capacity),"capacity before compaction");
  expect(capacity.ordinary_remaining_bytes==capacity.ordinary_limit_bytes-capacity.accounted_bytes,"capacity ordinary subtraction");
  expect(capacity.maintenance_remaining_bytes==capacity.maximum_bytes-capacity.accounted_bytes,"capacity absolute subtraction");
  ev::CompactionOptions options;options.operation_id="compact-cleaned-history";ev::CompactionPlan compact;
  require(ev::preview_native_compaction(*store,options,compact),"preview compaction");ev::CompactionReport result;
  auto too_small=options;too_small.operation_id="too-small";too_small.working_budget_bytes=1;ev::CompactionPlan rejected;
  require(ev::preview_native_compaction(*store,too_small,rejected),"review insufficient work budget");const auto stable=store->info();
  expect(ev::execute_native_compaction(*store,rejected,result).code==sdk::Code::capacity,"insufficient staging budget rejected");
  expect(store->info().root_digest==stable.root_digest&&!fs::exists(path/"COMPACTION"),"rejected maintenance creates no intent or mutation");
  ev::set_native_store_fault(*store,ev::FaultPoint::compaction_after_root);
  expect(ev::execute_native_compaction(*store,compact,result).code==sdk::Code::committed_degraded,"fault preserves published canonical base");
  require(lifecycle->capacity(capacity),"degraded capacity status");
  expect(!capacity.writable&&!capacity.index_ready&&capacity.ordinary_remaining_bytes==0&&capacity.maintenance_remaining_bytes==0,
    "degraded capacity never advertises unusable room");
  lifecycle.reset();store.reset();store=open(path);require(ev::resume_native_compaction(*store,result),"resume compacted cleanup");
  lifecycle=std::make_unique<lc::Service>(*store);require(lifecycle->open(),"reopen compacted lifecycle");require(lifecycle->capacity(capacity),"recovered capacity");
  expect(capacity.writable&&capacity.index_ready&&capacity.pending_reclamation_bytes==0,"recovery reconciles quota and admission");
  ev::CommitReceipt retry;require(store->commit(original,retry),"original transition exact retry after reclaim/compact");
  expect(retry.duplicate&&retry.generation==original_receipt.generation&&retry.root_digest==original_receipt.root_digest,"exact historical receipt retained");
  expect(lifecycle->register_engine(id,"owner","resurrect").code==sdk::Code::conflict,"source epoch fence survives reclaimed and compacted evidence");
  auto changed=id;changed.epoch=2;expect(lifecycle->register_engine(changed,"another-owner","change-owner").code==sdk::Code::unauthorized,"ownership survives compacted prefix");
  sdk::Bytes payload;expect(store->get_blob(source.last_payload_sha256,payload).code==sdk::Code::history_expired,"expired data stays expired");
  ev::AuditReport audit;require(store->audit(audit),"compacted interrupted lifecycle audit");
}
void campaign(const fs::path& repository,const fs::path& root){
  const auto bytes=read(repository/"qualification/native/scale-ci.cevqualification");q::ScaleProfile profile;
  require(q::parse_scale_profile(bytes,profile),"parse strict scale profile");q::ScaleProfile ignored;
  expect(q::parse_scale_profile(bytes+"receiver_pages_per_checkpoint=4\n",ignored).code==sdk::Code::invalid,"reject duplicate field");
  expect(q::parse_scale_profile(bytes+"unknown=4\n",ignored).code==sdk::Code::invalid,"reject unknown field");
  auto invalid=bytes;const auto position=invalid.find("receiver_page_observations=4");expect(position!=std::string::npos,"fixture page count");
  invalid.replace(position,std::string("receiver_page_observations=4").size(),"receiver_page_observations=33");
  expect(q::parse_scale_profile(invalid,ignored).code==sdk::Code::invalid,"reject receiver page bound");
  const auto output=root/"positive";fs::create_directory(output);q::ScaleReport report;report.profile_sha256=sdk::sha256(bytes);
  require(q::run_scale_profile(profile,output.string(),report),"actual mixed-namespace scale campaign");
  expect(report.passed&&report.observations==256&&report.epochs==4&&report.maximum_active_sources==1,"complete throughput/source workload");
  expect(report.minimum_file_nodes==7U*256U+128U&&(!report.admission_disk.file_nodes_available||
    report.admission_disk.filesystem_file_nodes_available>=report.minimum_file_nodes),"file-node headroom measured before ingestion");
  expect(report.receiver_observations==32&&report.receiver_expired==24&&report.maximum_retained_receiver_observations==12,"bounded relay window and overlap cleanup");
  expect(report.protected_root_verified&&report.quota_rejection_verified&&report.maintenance_reserve_verified,"actual protection/quota controls");
  expect(report.checkpoints.size()==2&&report.commit_threshold.count>0&&report.lookup_threshold.count==96,"all declared checkpoints and measured samples");
  for(const auto& point:report.checkpoints){
    expect(point.restart_verified&&point.identity_fences_verified&&point.exact_retry_verified,"checkpoint proofs retained");
    expect(point.compaction.activated&&point.compaction.cleanup_complete&&point.audit.base_pages>0,"actual canonical base and cleanup");
    expect(point.oldest.sequence==1&&point.recent.sequence==point.observations,"old and recent regions measured");
    expect(point.capacity.pending_reclamation_bytes==0&&point.capacity.writable,"capacity coherent after cleanup");
  }
  const auto json=q::scale_report_json(report);expect(json.find("\"acceptance_method\":\"exact-nearest-rank-threshold/1\"")!=std::string::npos,"exact gate version published");
  q::ScaleReport repeat;expect(q::run_scale_profile(profile,output.string(),repeat).code==sdk::Code::conflict,"existing canonical output cannot be reused");
  auto negative=profile;negative.compaction_working_bytes=1;const auto failed=root/"negative";fs::create_directory(failed);q::ScaleReport missed;
  expect(q::run_scale_profile(negative,failed.string(),missed).code==sdk::Code::capacity&&!missed.passed&&!missed.violations.empty(),"actual insufficient compaction budget remains failed evidence");
  expect(missed.checkpoints.size()==1&&missed.checkpoints.front().required_working_bytes>1&&!missed.checkpoints.front().compaction.activated,"failed report preserves exact unmet staging requirement");
  expect(!fs::exists(failed/"store"/"COMPACTION"),"insufficient-budget campaign leaves no cleanup intent");
}
}
int main(int argc,char** argv){
  try{
    expect(argc==3,"expected repository and isolated test root");const fs::path root=argv[2];std::error_code ec;fs::remove_all(root,ec);expect(!ec,"clear test fixture");fs::create_directories(root);
    campaign(argv[1],root);interrupted_retention_and_compaction(root/"faults");
    std::cout<<"scale/lifecycle qualification, receiver window, quota status, interrupted reclaim/compaction and immutable identity tests passed\n";return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
