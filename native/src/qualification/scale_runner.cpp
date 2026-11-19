#include "scale_qualification.hpp"
#include "native_paths.hpp"
#include "anaiero/sdk/mesh.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <set>
#include <sstream>

namespace anaiero::qualification {
namespace ev=sdk::evidence;
namespace lc=sdk::lifecycle;
namespace mesh=sdk::mesh;
namespace {
using Clock=std::chrono::steady_clock;
std::uint64_t elapsed(Clock::time_point start){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count());}
std::uint64_t now_seconds(){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());}
std::string utc(){
  const auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());std::tm tm{};
#ifdef _WIN32
  if(gmtime_s(&tm,&now)!=0)return "unavailable";
#else
  if(!gmtime_r(&now,&tm))return "unavailable";
#endif
  std::ostringstream out;out<<std::put_time(&tm,"%Y-%m-%dT%H:%M:%SZ");return out.str();
}
std::string utf8(const std::filesystem::path& p){const auto b=p.generic_u8string();return {reinterpret_cast<const char*>(b.data()),b.size()};}
sdk::Result corrupt(std::string message){return {sdk::Code::corrupt,std::move(message)};}
void violation(ScaleReport& r,const std::string& v){if(std::find(r.violations.begin(),r.violations.end(),v)==r.violations.end())r.violations.push_back(v);}
ev::CommitBatch batch(std::string operation){ev::CommitBatch b;b.operation_id=std::move(operation);b.request_digest=sdk::sha256(b.operation_id);return b;}
sdk::Result commit(ev::ICommitStore& store,const ev::CommitBatch& request){ev::CommitReceipt receipt;return store.commit(request,receipt);}
sdk::EngineIdentity identity(std::string source,std::uint64_t epoch){
  return {"scale-fixture","qualification.scale/1",sdk::sha256("scale-fixture-module/1"),sdk::sha256("scale-fixture-model/1"),
    "qualification.scale.behavior/1",std::move(source),"logical",epoch,0};
}
lc::Options lifecycle_options(){lc::Options o;o.max_active_sources=1;o.max_metrics=1;return o;}
sdk::EngineIdentity churn_identity(std::uint64_t ordinal){
  // Alternate a returning UID's next epoch with a newly owned UID. Both reuse
  // the one active slot; their permanent authority metadata remains visible.
  return ordinal%2?identity("scale-churn",(ordinal+1)/2):identity("scale-new-source-"+std::to_string(ordinal),1);
}

// Exercise the ordinary/maintenance boundary using real committed growth in a
// separate small store. It is a quota control, not physical disk exhaustion.
sdk::Result quota_control(const std::string& directory,ScaleReport& report){
  ev::StoreOptions options;options.index_layout=report.layout;
  options.maximum_bytes=2U*1024U*1024U;options.maintenance_reserve_bytes=256U*1024U;
  std::unique_ptr<ev::ICommitStore> store;if(auto r=ev::open_native_store(directory,options,store);!r)return r;
  lc::Service lifecycle(*store);if(auto r=lifecycle.open();!r)return r;
  bool rejected=false;
  for(unsigned i=0;i<128;++i){
    auto b=batch("scale-quota-ordinary-"+std::to_string(i));b.records.push_back({"scale.quota","state",std::string(32768,'q'),{},false});
    const auto before=store->info();const auto result=commit(*store,b);
    if(result.code==sdk::Code::capacity){
      if(store->info().root_digest!=before.root_digest||store->generation()!=before.generation)return corrupt("quota rejection changed the canonical root");
      rejected=true;break;
    }
    if(!result)return result;
  }
  if(!rejected)return corrupt("bounded quota control failed to reach ordinary admission limit");
  report.quota_rejection_verified=true;
  lc::CapacityStatus before;if(auto r=lifecycle.capacity(before);!r)return r;
  if(!before.index_ready||!before.writable||before.maintenance_remaining_bytes<=before.ordinary_remaining_bytes||
     before.maintenance_reserve_bytes!=options.maintenance_reserve_bytes)
    return corrupt("maintenance reserve status differs from authoritative quota");
  auto maintenance=batch("scale-quota-maintenance");maintenance.commit_class=ev::CommitClass::maintenance;
  maintenance.records.push_back({"scale.quota","maintenance","reserved maintenance remains available",{},false});
  if(auto r=commit(*store,maintenance);!r)return r;
  // The reserve is still finite. An oversized maintenance request must leave
  // authority unchanged; exhausted maintenance cannot be reported as free space.
  auto too_large=batch("scale-quota-maintenance-too-large");too_large.commit_class=ev::CommitClass::maintenance;
  too_large.records.push_back({"scale.quota","oversized",std::string(1048576,'x'),{},false});
  const auto root=store->info();
  if(commit(*store,too_large).code!=sdk::Code::capacity||store->info().root_digest!=root.root_digest)
    return corrupt("absolute maintenance quota was not enforced without effects");
  report.maintenance_reserve_verified=true;
  ev::AuditReport audit;return store->audit(audit);
}

sdk::Result churn_epoch(ev::ICommitStore& store,std::uint64_t epoch,ScaleReport& report){
  lc::Service lifecycle(store,lifecycle_options());if(auto r=lifecycle.open();!r)return r;
  const auto id=churn_identity(epoch);const lc::SourceRef ref{id.source_uid,id.epoch};
  const auto prefix="scale-epoch-"+std::to_string(epoch);
  if(auto r=lifecycle.register_engine(id,"scale-owner",prefix+"-register");!r)return r;
  if(auto r=lifecycle.activate(ref,prefix+"-activate");!r)return r;
  sdk::EngineState state{0,0,sdk::Phase::ready,{"scale.checkpoint/1","initial"},{"scale.configuration/1","synthetic"}};
  std::set<sdk::Digest> objects;
  for(unsigned step=0;step<2;++step){
    sdk::PreparedTransition transition{id,prefix+"-step-"+std::to_string(step),{},sdk::state_digest(state),state.revision,state,{},{}};
    transition.request_sha256=sdk::sha256(transition.operation_id);
    if(step){
      ++transition.target.revision;transition.target.tick=4;transition.target.phase=sdk::Phase::running;
      transition.target.checkpoint.payload="four observations";
      for(std::uint64_t i=1;i<=4;++i)transition.observations.push_back({"scale.observation/1","value","scale.count","count",static_cast<std::int64_t>(i),i});
    }
    auto b=batch(transition.operation_id);b.request_digest=transition.request_sha256;
    if(auto r=lifecycle.stage_commit(b,transition);!r)return r;
    for(const auto& object:b.blobs)objects.insert(object.digest);
    if(auto r=commit(store,b);!r)return r;
    state=transition.target;
  }
  lc::Snapshot active;if(auto r=lifecycle.snapshot(active);!r)return r;
  report.maximum_active_sources=std::max(report.maximum_active_sources,static_cast<std::uint64_t>(active.sources.size()));
  if(active.sources.size()!=1||active.metrics.size()!=1||active.lifetime_observations!=epoch*4)
    return corrupt("source churn active cardinality or observation count is wrong");
  if(auto r=lifecycle.begin_drain(ref,prefix+"-drain");!r)return r;
  lc::Source source;if(auto r=lifecycle.source(ref,source);!r)return r;
  if(auto r=lifecycle.seal(ref,{source.last_sequence,source.last_payload_sha256},prefix+"-seal");!r)return r;
  if(auto r=lifecycle.retire(ref,prefix+"-retire");!r)return r;
  lc::MaintenanceRequest request{prefix+"-expire",ref,4,now_seconds(),lc::HistoryAvailability::expired,{},
    {objects.begin(),objects.end()}};
  lc::MaintenancePlan plan;
  if(!report.protected_root_verified){
    lc::Pin pin{"scale-protected-reader","scale-reader",lc::PinKind::reader,1,0,{*objects.begin()}};
    if(auto r=lifecycle.put_pin(pin,0,now_seconds(),"scale-pin-reader");!r)return r;
    if(lifecycle.review_maintenance(request,"scale-protected-review",plan).code!=sdk::Code::busy)
      return corrupt("retired source maintenance ignored a protected reader root");
    if(auto r=lifecycle.release_pin(pin.id,1,"scale-release-reader");!r)return r;
    report.protected_root_verified=true;
  }
  if(auto r=lifecycle.review_maintenance(request,prefix+"-review",plan);!r)return r;
  if(auto r=lifecycle.activate_maintenance(request.id,now_seconds(),prefix+"-activate-expiry");!r)return r;
  if(auto r=lifecycle.execute_maintenance(request.id,prefix+"-reclaim");!r)return r;
  // Exact repeated maintenance and old activation stay idempotent even though
  // payloads have gone. They must not create a new active source incarnation.
  const auto generation=store.generation();
  if(auto r=lifecycle.execute_maintenance(request.id,prefix+"-reclaim");!r)return r;
  if(auto r=lifecycle.activate(ref,prefix+"-activate");!r)return r;
  if(store.generation()!=generation)return corrupt("exact lifecycle retry changed canonical generation");
  for(const auto& digest:objects){ev::BlobInfo info;if(auto r=store.blob_info(digest,info);!r)return r;
    if(info.present||!info.expired_generation)return corrupt("retired source payload was not physically reclaimed");}
  if(auto r=lifecycle.snapshot(active);!r)return r;
  if(!active.sources.empty()||!active.metrics.empty())return corrupt("retired source remained in active state");
  if(lifecycle.replay_status(ref,0).code!=sdk::Code::history_expired)return corrupt("expired source replay remained available");
  report.reclaimed_payload_bytes+=plan.expected_bytes;++report.epochs;return {};
}

struct RetainedPage {mesh::Delivery delivery;sdk::Digest duplicate_receipt;};
struct Relay {
  mesh::Session session;sdk::EngineIdentity source=identity("scale-relay",1);
  lc::Cursor cursor;std::deque<RetainedPage> pages;mesh::Delivery expired_example;
};
sdk::Result start_relay(ev::ICommitStore& store,Relay& relay){
  lc::Service lifecycle(store,lifecycle_options());const auto now=now_seconds();const auto expiry=now+7ULL*24ULL*60ULL*60ULL;
  if(auto r=lifecycle.upsert_peer({"scale-peer",sdk::sha256("synthetic-scale-certificate"),1,expiry,lc::PeerPhase::active},0,now,"scale-enroll-peer");!r)return r;
  mesh::Service service(store);
  if(auto r=service.authenticate({sdk::sha256("synthetic-scale-certificate"),expiry,true},"scale-peer",now,relay.session);!r)return r;
  return service.configure_grant({"scale-peer",relay.source,1,false,true,false,{}},0,"scale-relay-grant");
}
sdk::Result advance_relay(ev::ICommitStore& store,Relay& relay,const ScaleProfile& profile,ScaleReport& report){
  mesh::Service service(store);const lc::SourceRef ref{relay.source.source_uid,relay.source.epoch};
  for(std::uint64_t page_number=0;page_number<profile.receiver_pages_per_checkpoint;++page_number){
    mesh::Delivery page;page.subscription_id="scale-main";page.generation=1;page.source=relay.source;page.from=relay.cursor;
    for(std::uint64_t i=1;i<=profile.receiver_page_observations;++i){
      const auto sequence=relay.cursor.sequence+i;
      const sdk::Observation observation{"scale.relay/1","value","scale.count","count",static_cast<std::int64_t>(sequence),sequence};
      const auto payload=lc::encode_source_observation(ref,sequence,observation);
      page.observations.push_back({{ref.uid,ref.epoch,sequence,sdk::sha256(payload),sdk::sha256("synthetic-original-receipt-"+std::to_string(sequence))},payload});
    }
    page.to={page.observations.back().claim.sequence,page.observations.back().claim.payload_digest};
    mesh::ReceiveReceipt receipt;if(auto r=service.receive(relay.session,page,now_seconds(),receipt);!r)return r;
    auto duplicate=page;duplicate.subscription_id="scale-overlap";
    if(auto r=service.receive(relay.session,duplicate,now_seconds(),receipt);!r)return r;
    relay.pages.push_back({page,receipt.receipt_sha256});relay.cursor=page.to;
    report.receiver_observations+=profile.receiver_page_observations;
    report.maximum_retained_receiver_observations=std::max(report.maximum_retained_receiver_observations,
      report.receiver_observations-report.receiver_expired);
    while(relay.pages.size()>profile.retained_receiver_pages){
      const auto& oldest=relay.pages.front();
      mesh::ReceiverRetentionRequest request{"scale-relay-expire-"+std::to_string(oldest.delivery.to.sequence),relay.session.principal,ref,
        oldest.delivery.from.sequence+1,oldest.delivery.to.sequence,{},{},{oldest.duplicate_receipt}};
      mesh::ReceiverRetentionPlan plan;if(auto r=service.review_receiver_retention(request,request.id+"-review",plan);!r)return r;
      if(auto r=service.activate_receiver_retention(request.id,request.id+"-activate");!r)return r;
      if(auto r=service.reclaim_receiver_retention(request.id,request.id+"-reclaim");!r)return r;
      const auto generation=store.generation();if(auto r=service.reclaim_receiver_retention(request.id,request.id+"-reclaim");!r)return r;
      if(generation!=store.generation())return corrupt("relay retention retry changed canonical history");
      for(const auto& object:plan.objects){ev::BlobInfo info;if(auto r=store.blob_info(object,info);!r)return r;
        if(info.present||!info.expired_generation)return corrupt("relay payload or overlapping receipt was not reclaimed");}
      mesh::DeliveryItem missing;
      if(service.received_observation(relay.session.principal,ref,request.first_sequence,missing).code!=sdk::Code::history_expired)
        return corrupt("receiver retention resurrected an expired observation");
      if(service.receive(relay.session,oldest.delivery,now_seconds(),receipt).code!=sdk::Code::history_expired)
        return corrupt("receiver old delivery retry resurrected expired history");
      if(relay.expired_example.observations.empty())relay.expired_example=oldest.delivery;
      report.receiver_expired+=profile.receiver_page_observations;report.reclaimed_payload_bytes+=plan.expected_bytes;
      relay.pages.pop_front();
    }
  }
  return {};
}

sdk::Result measure_lookup(ev::ICommitStore& store,ScaleReport& report,ScaleLookup& point,
                           std::uint64_t begin,std::uint64_t end,std::uint64_t preferred){
  const auto& profile=report.profile.evidence;point.sequence=preferred;
  point.threshold.limit_ns=profile.maximum_lookup_p99_us*1000;
  const auto count=std::max<std::uint64_t>(1,profile.query_samples/3);
  for(std::uint64_t i=0;i<count;++i){
    const auto sequence=i==0?preferred:begin+(i*104729+17)%(end-begin+1);
    ev::ObservationClaim claim;ev::reset_native_index_statistics(store);const auto start=Clock::now();
    if(auto r=store.lookup_observation("scale-throughput",1,sequence,claim);!r)return r;
    const auto duration=elapsed(start);point.latency.add(duration);point.threshold.add(duration);report.lookup_threshold.add(duration);
    point.maximum_nodes=std::max(point.maximum_nodes,ev::native_index_statistics(store).nodes_read);
    if(claim.source_uid!="scale-throughput"||claim.epoch!=1||claim.sequence!=sequence)return corrupt("historical scale lookup returned the wrong identity");
    if(i==0){sdk::Bytes payload,receipt;if(auto r=store.get_blob(claim.payload_digest,payload);!r)return r;
      if(auto r=store.get_blob(claim.receipt_digest,receipt);!r)return r;
      if(payload.size()!=profile.payload_bytes||sdk::sha256(payload)!=claim.payload_digest||sdk::sha256(receipt)!=claim.receipt_digest)
        return corrupt("historical scale lookup payload/receipt failed verification");}
  }
  if(!point.threshold.passes(99))violation(report,"historical lookup exact p99 exceeds declared budget");
  if(point.maximum_nodes>profile.maximum_lookup_nodes)violation(report,"historical lookup work exceeds declared node budget");
  return {};
}
sdk::Result checkpoint(const ScaleProfile& profile,const std::string& directory,const ev::StoreOptions& options,
                       std::unique_ptr<ev::ICommitStore>& store,const ev::CommitBatch& first_batch,
                       const ev::CommitReceipt& first_receipt,Relay& relay,ScaleReport& report){
  report.checkpoints.emplace_back();auto& point=report.checkpoints.back();point.observations=report.observations;
  point.epochs=report.epochs;point.receiver_observations=report.receiver_observations;point.receiver_expired=report.receiver_expired;
  if(auto r=measure_disk(directory,profile.evidence.maximum_files,point.before_disk);!r)return r;
  ev::CompactionOptions compact;compact.operation_id="scale-compact-"+std::to_string(report.observations);
  compact.expected_generation=store->generation();compact.expected_root=store->info().root_digest;
  compact.working_budget_bytes=profile.compaction_working_bytes;
  ev::CompactionPlan plan;if(auto r=ev::preview_native_compaction(*store,compact,plan);!r)return r;
  point.required_working_bytes=plan.required_working_bytes;
  point.retained_records=plan.retained_records;point.retained_blobs=plan.retained_blobs;point.retained_operations=plan.retained_operations;
  const auto previous=store->info();const auto started=Clock::now();
  point.generation=previous.generation;point.root=previous.root_digest;
  {lc::Service lifecycle(*store,lifecycle_options());if(auto r=lifecycle.capacity(point.capacity);!r)return r;}
  const auto compacted=ev::execute_native_compaction(*store,plan,point.compaction);point.compaction_ns=elapsed(started);
  if(!compacted)return compacted;
  if(options.measure_compaction_files){
    if(!point.compaction.coexistence_measured)
      return corrupt("compaction did not return the requested coexistence inventory");
    if(point.compaction.coexistence_files>profile.evidence.maximum_files)
      violation(report,"coexisting compaction files exceed the declared file budget");
  }
  if(!point.compaction.activated||!point.compaction.cleanup_complete||store->generation()!=previous.generation||store->info().root_digest!=previous.root_digest)
    return corrupt("scale compaction changed terminal authority or left pending cleanup");
  if(auto r=measure_disk(directory,profile.evidence.maximum_files,point.after_disk);!r)return r;
  point.generation=previous.generation;point.root=previous.root_digest;
  store.reset();const auto reopening=Clock::now();if(auto r=ev::open_native_store(directory,options,store);!r)return r;
  point.reopen_ns=elapsed(reopening);point.reopen_nodes=ev::native_index_statistics(*store).nodes_read;
  if(store->info().index_backend!=previous.index_backend||!store->info().index_ready||store->info().root_digest!=previous.root_digest||store->generation()!=previous.generation||
     store->info().store_id!=previous.store_id)return corrupt("post-compaction reopen changed canonical identity");
  point.restart_verified=true;
  ev::CommitReceipt duplicate;if(auto r=store->commit(first_batch,duplicate);!r)return r;
  if(!duplicate.duplicate||duplicate.generation!=first_receipt.generation||duplicate.root_digest!=first_receipt.root_digest||
     duplicate.batch_digest!=first_receipt.batch_digest||store->generation()!=point.generation)
    return corrupt("compaction changed an exact original operation receipt");
  point.exact_retry_verified=true;
  lc::Service lifecycle(*store,lifecycle_options());if(auto r=lifecycle.open();!r)return r;
  lc::Snapshot live;if(auto r=lifecycle.snapshot(live);!r)return r;
  if(!live.sources.empty()||!live.metrics.empty()||live.lifetime_observations!=report.epochs*4)
    return corrupt("compaction/reopen resurrected retired active state");
  if(lifecycle.register_engine(identity("scale-churn",1),"scale-owner","scale-resurrection-"+std::to_string(report.observations)).code!=sdk::Code::conflict)
    return corrupt("compaction lost retired source epoch fence");
  if(lifecycle.register_engine(identity("scale-churn",(report.epochs+1)/2+1),"another-owner","scale-owner-change-"+std::to_string(report.observations)).code!=sdk::Code::unauthorized)
    return corrupt("compaction lost source ownership authority");
  if(report.epochs>=2&&lifecycle.register_engine(churn_identity(report.epochs-report.epochs%2),"scale-owner","scale-new-uid-resurrection-"+std::to_string(report.observations)).code!=sdk::Code::conflict)
    return corrupt("compaction lost newly introduced source ownership or retirement fence");
  mesh::Service mesh(*store);mesh::ReceiveReceipt receipt;
  if(mesh.receive(relay.session,relay.expired_example,now_seconds(),receipt).code!=sdk::Code::history_expired)
    return corrupt("compaction lost receiver retry expiry");
  for(const auto& retained:relay.pages){mesh::DeliveryItem item;
    if(auto r=mesh.received_observation(relay.session.principal,{relay.source.source_uid,1},retained.delivery.to.sequence,item);!r)return r;}
  if(store->info().root_digest!=point.root)return corrupt("rejected identity resurrection mutated authority");
  point.identity_fences_verified=true;
  if(auto r=lifecycle.capacity(point.capacity);!r)return r;
  const auto third=std::max<std::uint64_t>(1,report.observations/3);
  if(auto r=measure_lookup(*store,report,point.oldest,1,third,1);!r)return r;
  if(auto r=measure_lookup(*store,report,point.middle,third+1,report.observations-third,(report.observations+1)/2);!r)return r;
  if(auto r=measure_lookup(*store,report,point.recent,report.observations-third+1,report.observations,report.observations);!r)return r;
  const auto auditing=Clock::now();const auto audited=store->audit(point.audit);point.audit_ns=elapsed(auditing);if(!audited)return audited;
  if(auto r=read_resources(point.resources);!r)return r;
  const auto& budgets=profile.evidence;
  if(point.reopen_ns>budgets.maximum_reopen_us*1000)violation(report,"healthy scale reopen exceeds declared duration");
  if(point.reopen_nodes>budgets.maximum_lookup_nodes)violation(report,"healthy scale reopen exceeds declared index work");
  if(point.resources.peak_resident_bytes>budgets.maximum_resident_bytes)violation(report,"scale process resident bytes exceed declared budget");
  for(const auto* d:{&point.before_disk,&point.after_disk})
    if(d->logical_bytes>budgets.maximum_disk_bytes||d->allocated_bytes>budgets.maximum_disk_bytes)
      violation(report,"scale regular-file allocation exceeds declared disk budget");
  if(point.capacity.pending_reclamation_bytes)return corrupt("completed maintenance retained pending byte accounting");
  return {};
}
}

sdk::Result run_scale_profile(const ScaleProfile& profile,std::string_view directory,ScaleReport& report,std::function<void(std::uint64_t)> progress,ev::IndexLayout layout){
  report.layout=layout;
  report.profile=profile;report.started_utc=utc();const auto start=Clock::now();const auto& p=profile.evidence;
  report.commit_threshold.limit_ns=p.maximum_commit_p99_us*1000;report.lookup_threshold.limit_ns=p.maximum_lookup_p99_us*1000;
  const auto run=[&]()->sdk::Result{
    if(auto r=read_resources(report.before);!r)return r;
    if(auto r=read_environment(directory,report.environment);!r)return r;
    // Each distinct payload has a file. The single-node layout also needs
    // three rows per observation in each of two coexisting trees.
    const auto files_per_observation=layout==ev::IndexLayout::packed_pages?1U:7U;
    report.minimum_file_nodes=p.observations*files_per_observation+128U;
    if(auto r=measure_disk(directory,p.maximum_files,report.admission_disk);!r)return r;
    if(auto r=require_file_nodes(report.admission_disk,report.minimum_file_nodes);!r)return r;
    ev::StoreOptions options;options.maximum_bytes=p.maximum_store_bytes;
    options.index_layout=layout;
    options.measure_compaction_files=layout!=ev::IndexLayout::automatic;
    const auto path=utf8(native_path(directory)/"store");std::unique_ptr<ev::ICommitStore> store;
    if(auto r=ev::open_native_store(path,options,store);!r)return r;
    if(store->generation()!=0)return {sdk::Code::conflict,"scale qualification requires a fresh isolated store"};
    report.store_id=store->info().store_id;
    report.index_backend=store->info().index_backend;
    Relay relay;if(auto r=start_relay(*store,relay);!r)return r;
    ev::CommitBatch first_batch;ev::CommitReceipt first_receipt;std::uint64_t revision=0;std::size_t next=0;
    while(report.observations<p.observations){
      const auto count=std::min({p.batch_size,p.observations-report.observations,p.checkpoints[next]-report.observations});
      auto b=batch("scale-throughput-"+std::to_string(revision+1));b.expected_generation=store->generation();
      b.guards.push_back({"scale","cursor",revision,{}});b.records.push_back({"scale","cursor",std::to_string(report.observations+count),{},false});
      const auto receipt=sdk::pack({"scale-throughput-receipt/1",b.operation_id,std::to_string(count)});const auto receipt_digest=sdk::sha256(receipt);
      b.blobs.push_back({receipt_digest,receipt,{}});
      for(std::uint64_t i=1;i<=count;++i){
        const auto sequence=report.observations+i;auto payload=sdk::pack({"scale-throughput-observation/1",std::to_string(sequence)});
        if(payload.size()>p.payload_bytes)return {sdk::Code::invalid,"scale payload budget is smaller than its canonical identity"};
        payload.resize(static_cast<std::size_t>(p.payload_bytes),'s');const auto digest=sdk::sha256(payload);
        b.blobs.push_back({digest,std::move(payload),{}});b.observations.push_back({"scale-throughput",1,sequence,digest,receipt_digest});
      }
      ev::CommitReceipt committed;const auto committing=Clock::now();const auto result=store->commit(b,committed);
      const auto duration=elapsed(committing);report.commit.add(duration);report.commit_threshold.add(duration);if(!result)return result;
      if(revision==0){first_batch=b;first_receipt=committed;}
      ++revision;report.observations+=count;if(progress)progress(report.observations);
      if(report.observations==p.checkpoints[next]){
        const auto target_epochs=(static_cast<std::uint64_t>(next)+1)*p.epochs/p.checkpoints.size();
        while(report.epochs<target_epochs)if(auto r=churn_epoch(*store,report.epochs+1,report);!r)return r;
        if(auto r=advance_relay(*store,relay,profile,report);!r)return r;
        if(auto r=checkpoint(profile,path,options,store,first_batch,first_receipt,relay,report);!r)return r;
        ++next;
      }
    }
    if(!report.commit_threshold.passes(99))violation(report,"batch commit exact p99 exceeds declared budget");
    if(auto r=quota_control(utf8(native_path(directory)/"quota-control"),report);!r)return r;
    return {};
  };
  const auto result=run();
  const auto disk_result=measure_disk(directory,p.maximum_files,report.final_disk);
  report.wall_ns=elapsed(start);report.finished_utc=utc();const auto resources=read_resources(report.after);
  if(!result)violation(report,std::string(sdk::code_name(result.code))+": "+result.message);
  if(!disk_result)violation(report,disk_result.message);
  if(!resources)violation(report,resources.message);
  if(report.final_disk.logical_bytes>p.maximum_disk_bytes||report.final_disk.allocated_bytes>p.maximum_disk_bytes)
    violation(report,"scale output allocation exceeds declared disk budget");
  if(report.after.peak_resident_bytes>p.maximum_resident_bytes)violation(report,"scale process resident bytes exceed declared budget");
  report.passed=result.ok()&&resources.ok()&&disk_result.ok()&&report.violations.empty();
  if(!result)return result;
  if(!resources)return resources;
  if(!disk_result)return disk_result;
  return report.passed?sdk::Result{}:sdk::Result{sdk::Code::capacity,"scale qualification exceeded its predeclared operating budget"};
}
}
