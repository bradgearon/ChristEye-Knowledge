#include "qualification.hpp"
#include "native_paths.hpp"
#include "anaiero/sdk/lifecycle.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace anaiero::qualification {
namespace ev=sdk::evidence;
namespace lc=sdk::lifecycle;
namespace {
using Clock=std::chrono::steady_clock;
std::uint64_t elapsed(Clock::time_point start){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count());}
std::string utc(){
  const auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());std::tm result{};
#ifdef _WIN32
  if(gmtime_s(&result,&now)!=0)return "unavailable";
#else
  if(!gmtime_r(&now,&result))return "unavailable";
#endif
  std::ostringstream out;out<<std::put_time(&result,"%Y-%m-%dT%H:%M:%SZ");return out.str();
}
std::string utf8(const std::filesystem::path& path){const auto bytes=path.generic_u8string();return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};}
void violation(Report& report,const std::string& message){if(std::find(report.violations.begin(),report.violations.end(),message)==report.violations.end())report.violations.push_back(message);}
sdk::Result checkpoint(const Profile& profile,const std::string& directory,const ev::StoreOptions& options,
                       std::unique_ptr<ev::ICommitStore>& store,Report& report){
  Checkpoint point;point.observations=report.observations;
  const auto prior=store->info();point.store_generation=prior.generation;point.accounted_bytes=prior.physical_bytes;
  // Reopen is measured before queries, so a warm in-process index cannot conceal
  // a full historical startup scan. Disk cache state remains explicitly uncontrolled.
  store.reset();const auto reopen=Clock::now();
  if(auto r=ev::open_native_store(directory,options,store);!r)return r;
  point.reopen_ns=elapsed(reopen);point.reopen_nodes=ev::native_index_statistics(*store).nodes_read;
  const auto reopened=store->info();
  if(!reopened.index_ready||reopened.store_id!=prior.store_id||reopened.root_digest!=prior.root_digest||reopened.generation!=prior.generation)
    return {sdk::Code::corrupt,"qualification reopen changed canonical identity or lost index coverage"};
  ev::StoredRecord cursor;if(auto r=store->latest("qualification","cursor",cursor);!r)return r;
  if(cursor.record.payload!=std::to_string(report.observations))return {sdk::Code::corrupt,"qualification cursor changed after reopen"};
  report.restart_identity_verified=true;
  for(std::uint64_t i=0;i<profile.query_samples;++i){
    // A fixed arithmetic permutation covers old/middle/recent evidence. It is
    // deterministic across runs and independent of process RNG implementations.
    const auto sequence=1+((i*104729+17)%report.observations);
    ev::ObservationClaim claim;ev::reset_native_index_statistics(*store);const auto started=Clock::now();
    if(auto r=store->lookup_observation("qualification-throughput",1,sequence,claim);!r)return r;
    point.lookup.add(elapsed(started));
    point.lookup_maximum_nodes=std::max(point.lookup_maximum_nodes,ev::native_index_statistics(*store).nodes_read);
    if(claim.sequence!=sequence||claim.source_uid!="qualification-throughput")return {sdk::Code::corrupt,"qualification point query returned another observation"};
  }
  ev::ObservationClaim absent;ev::reset_native_index_statistics(*store);
  if(store->lookup_observation("qualification-throughput",1,report.observations+1,absent).code!=sdk::Code::not_found)
    return {sdk::Code::corrupt,"qualification missing observation did not remain explicitly absent"};
  point.lookup_maximum_nodes=std::max(point.lookup_maximum_nodes,ev::native_index_statistics(*store).nodes_read);
  if(auto r=read_resources(point.resources);!r)return r;
  if(auto r=measure_disk(directory,profile.maximum_files,point.disk);!r)return r;
  if(point.resources.peak_resident_bytes>profile.maximum_resident_bytes)violation(report,"process peak resident bytes exceed declared budget");
  if(point.disk.logical_bytes>profile.maximum_disk_bytes||point.disk.allocated_bytes>profile.maximum_disk_bytes)violation(report,"measured store disk bytes exceed declared budget");
  if(point.lookup.percentile_upper_ns(99)>profile.maximum_lookup_p99_us*1000)violation(report,"historical lookup p99 upper bound exceeds declared budget");
  if(point.lookup_maximum_nodes>profile.maximum_lookup_nodes)violation(report,"historical point lookup visits exceed declared budget");
  if(point.reopen_ns>profile.maximum_reopen_us*1000)violation(report,"healthy reopen duration exceeds declared budget");
  if(point.reopen_nodes>profile.maximum_lookup_nodes)violation(report,"healthy reopen visits exceed declared index budget");
  report.checkpoints.push_back(std::move(point));return {};
}
sdk::Result churn(ev::ICommitStore& store,const Profile& profile,Report& report){
  lc::Options limits;limits.max_active_sources=2;limits.max_metrics=4;
  lc::Service service(store,limits);if(auto r=service.open();!r)return r;
  for(std::uint64_t epoch=1;epoch<=profile.epochs;++epoch){
    const std::string operation="qualification-epoch-"+std::to_string(epoch);
    sdk::EngineIdentity identity{"qualification-engine","qualification.synthetic/1",sdk::sha256("qualification-module/1"),
      sdk::sha256("qualification-model/1"),"qualification.behavior/1","qualification-churn","logical",epoch,0};
    if(auto r=service.register_engine(identity,"qualification-owner",operation+"-register");!r)return r;
    if(auto r=service.activate({identity.source_uid,epoch},operation+"-activate");!r)return r;
    sdk::EngineState initial{0,0,sdk::Phase::ready,{"qualification.checkpoint/1","initial"},{"qualification.configuration/1","synthetic"}};
    sdk::PreparedTransition transition{identity,operation+"-attach",sdk::sha256(operation+"-attach"),sdk::state_digest(initial),0,initial,{},{}};
    for(unsigned stage=0;stage<2;++stage){
      if(stage==1){
        transition.operation_id=operation+"-step";transition.request_sha256=sdk::sha256(transition.operation_id);
        transition.target.revision=1;transition.target.tick=1;transition.target.phase=sdk::Phase::running;
        transition.target.checkpoint.payload="step-one";
        transition.observations.push_back({"qualification.observation/1","value","qualification.count","count",1,1});
      }
      ev::CommitBatch batch;batch.operation_id=transition.operation_id;batch.request_digest=transition.request_sha256;
      const auto state=sdk::encode_checkpoint(identity,transition.target),receipt=sdk::encode_transition(transition);
      batch.blobs={{sdk::sha256(state),state,{}},{sdk::sha256(receipt),receipt,{}}};
      if(auto r=service.stage_commit(batch,transition);!r)return r;
      ev::CommitReceipt committed;if(auto r=store.commit(batch,committed);!r)return r;
    }
    lc::Snapshot active;if(auto r=service.snapshot(active);!r)return r;
    report.maximum_active_sources=std::max(report.maximum_active_sources,static_cast<std::uint64_t>(active.sources.size()));
    if(active.sources.size()!=1||active.metrics.size()!=1||active.lifetime_observations!=epoch)
      return {sdk::Code::corrupt,"qualification active lifecycle cardinality or observation count is wrong"};
    if(auto r=service.begin_drain({identity.source_uid,epoch},operation+"-drain");!r)return r;
    lc::Source source;if(auto r=service.source({identity.source_uid,epoch},source);!r)return r;
    if(auto r=service.seal(source.id,{source.last_sequence,source.last_payload_sha256},operation+"-seal");!r)return r;
    if(auto r=service.retire(source.id,operation+"-retire");!r)return r;
    lc::Snapshot retired;if(auto r=service.snapshot(retired);!r)return r;
    if(!retired.sources.empty()||!retired.metrics.empty())return {sdk::Code::corrupt,"retired lifecycle source remained active"};
    ++report.epochs;
  }
  // Reused historical identity and changed owner must remain fenced even after
  // the active array has been fully recycled through all declared epochs.
  sdk::EngineIdentity stale{"qualification-engine","qualification.synthetic/1",sdk::sha256("qualification-module/1"),
    sdk::sha256("qualification-model/1"),"qualification.behavior/1","qualification-churn","logical",1,0};
  if(service.register_engine(stale,"qualification-owner","qualification-resurrection").code!=sdk::Code::conflict)
    return {sdk::Code::corrupt,"qualification retired epoch was not fenced"};
  stale.epoch=profile.epochs+1;
  if(service.register_engine(stale,"another-owner","qualification-ownership-conflict").code!=sdk::Code::unauthorized)
    return {sdk::Code::corrupt,"qualification ownership was not fenced"};
  return {};
}
}
sdk::Result run_profile(const Profile& profile,std::string_view directory,Report& report,std::function<void(std::uint64_t)> progress){
  report.profile=profile;report.started_utc=utc();const auto started=Clock::now();
  const auto run=[&]()->sdk::Result{
    if(auto r=read_resources(report.before);!r)return r;
    if(auto r=read_environment(directory,report.environment);!r)return r;
    const auto store_path=utf8(native_path(directory)/"store");
    ev::StoreOptions options;options.maximum_bytes=profile.maximum_store_bytes;
    std::unique_ptr<ev::ICommitStore> store;if(auto r=ev::open_native_store(store_path,options,store);!r)return r;
    if(store->info().generation!=0)return {sdk::Code::conflict,"qualification requires a fresh isolated store"};
    report.store_id=store->info().store_id;
    std::uint64_t revision=0;std::size_t next_checkpoint=0;
    while(report.observations<profile.observations){
      const auto ingest_start=Clock::now();
      const auto count=std::min({profile.batch_size,profile.observations-report.observations,profile.checkpoints[next_checkpoint]-report.observations});
      ev::CommitBatch batch;batch.operation_id="qualification-batch-"+std::to_string(revision+1);
      batch.request_digest=sdk::sha256(batch.operation_id);batch.expected_generation=store->generation();
      batch.guards.push_back({"qualification","cursor",revision,{}});
      batch.records.push_back({"qualification","cursor",std::to_string(report.observations+count),{},false});
      const auto receipt_bytes=sdk::pack({"qualification.receipt/1",batch.operation_id,std::to_string(count)});
      const auto receipt_digest=sdk::sha256(receipt_bytes);batch.blobs.push_back({receipt_digest,receipt_bytes,{}});
      for(std::uint64_t i=1;i<=count;++i){
        const auto sequence=report.observations+i;
        auto payload=sdk::pack({"qualification.observation/1",std::to_string(sequence)});
        if(payload.size()>profile.payload_bytes)return {sdk::Code::invalid,"declared payload too small for canonical identity"};
        payload.resize(static_cast<std::size_t>(profile.payload_bytes),'q');const auto digest=sdk::sha256(payload);
        batch.blobs.push_back({digest,std::move(payload),{}});
        batch.observations.push_back({"qualification-throughput",1,sequence,digest,receipt_digest});
      }
      ev::CommitReceipt receipt;ev::reset_native_index_statistics(*store);
      const auto commit_start=Clock::now();const auto committed=store->commit(batch,receipt);report.commit.add(elapsed(commit_start));
      if(!committed)return committed;
      const auto work=ev::native_index_statistics(*store);
      report.write_index_reads+=work.nodes_read;report.write_index_nodes+=work.nodes_written;
      report.staged_index_nodes+=work.staged_nodes;report.transient_nodes_elided+=work.transient_nodes_elided;
      report.index_spills+=work.staged_spills;report.index_peak_nodes=std::max(report.index_peak_nodes,work.cache_peak_nodes);
      report.index_peak_bytes=std::max(report.index_peak_bytes,work.cache_peak_bytes);
      report.ingestion_ns+=elapsed(ingest_start);
      report.observations+=count;++revision;
      if(progress)progress(report.observations);
      if(report.observations==profile.checkpoints[next_checkpoint]){
        if(auto r=checkpoint(profile,store_path,options,store,report);!r)return r;
        ++next_checkpoint;
      }
    }
    if(auto r=churn(*store,profile,report);!r)return r;
    const auto before=store->info();store.reset();if(auto r=ev::open_native_store(store_path,options,store);!r)return r;
    if(store->info().root_digest!=before.root_digest||!store->info().index_ready)return {sdk::Code::corrupt,"post-churn recovery changed canonical root"};
    lc::Service recovered(*store);if(auto r=recovered.open();!r)return r;
    lc::Snapshot snapshot;if(auto r=recovered.snapshot(snapshot);!r)return r;
    if(!snapshot.sources.empty()||snapshot.lifetime_observations!=profile.epochs)return {sdk::Code::corrupt,"post-churn recovery resurrected active history"};
    ev::AuditReport audit;const auto audit_start=Clock::now();const auto audited=store->audit(audit);report.audit_ns=elapsed(audit_start);
    report.audit_generations=audit.generations;report.audit_records=audit.records;
    report.audit_canonical_records=audit.canonical_records_read;report.audit_index_nodes=audit.index_nodes_read;
    report.audit_passed=audited.ok();if(!audited)return audited;
    if(report.commit.percentile_upper_ns(99)>profile.maximum_commit_p99_us*1000)violation(report,"batch commit p99 upper bound exceeds declared budget");
    if(auto r=measure_disk(store_path,profile.maximum_files,report.final_disk);!r)return r;
    report.final_generation=store->info().generation;report.final_accounted_bytes=store->info().physical_bytes;
    if(report.final_disk.logical_bytes>profile.maximum_disk_bytes||report.final_disk.allocated_bytes>profile.maximum_disk_bytes)violation(report,"final store disk bytes exceed declared budget");
    return {};
  };
  const auto result=run();report.wall_ns=elapsed(started);report.finished_utc=utc();
  const auto resources_result=read_resources(report.after);
  if(report.after.peak_resident_bytes>profile.maximum_resident_bytes)violation(report,"process peak resident bytes exceed declared budget");
  if(!result)violation(report,std::string(sdk::code_name(result.code))+": "+result.message);
  if(!resources_result)violation(report,resources_result.message);
  report.passed=result.ok()&&resources_result.ok()&&report.violations.empty();
  if(!result)return result;
  if(!resources_result)return resources_result;
  return report.passed?sdk::Result{}:sdk::Result{sdk::Code::capacity,"qualification exceeded its predeclared operating budget"};
}
}
