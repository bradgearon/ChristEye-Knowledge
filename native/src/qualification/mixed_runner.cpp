#include "mixed_internal.hpp"
#include "native_paths.hpp"
#include "anaiero/sdk/engine_files.hpp"
#include "anaiero/sdk/native_session.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>

namespace anaiero::qualification {
namespace mixed_detail {
namespace ev=sdk::evidence;
namespace host=sdk::host;
std::uint64_t elapsed(Clock::time_point start){
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count());
}
std::string utc(){
  const auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());std::tm value{};
#ifdef _WIN32
  if(gmtime_s(&value,&now)!=0)return "unavailable";
#else
  if(!gmtime_r(&now,&value))return "unavailable";
#endif
  std::ostringstream out;out<<std::put_time(&value,"%Y-%m-%dT%H:%M:%SZ");return out.str();
}
std::string utf8(const std::filesystem::path& path){
  const auto bytes=path.generic_u8string();return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};
}
void violation(MixedReport& report,std::string value){
  if(std::find(report.violations.begin(),report.violations.end(),value)==report.violations.end())report.violations.push_back(std::move(value));
}
void observe(MixedReport& report,const host::Status& status){
  report.cycles=status.completed_cycles;
  report.maximum_queued_commands=std::max(report.maximum_queued_commands,static_cast<std::uint64_t>(status.queued_commands));
  report.maximum_queued_tasks=std::max(report.maximum_queued_tasks,static_cast<std::uint64_t>(status.queued_tasks));
}
host::Status snapshot(host::NativeContinuousHost& instance,MixedReport& report){
  const auto start=Clock::now();auto result=instance.snapshot();const auto ns=elapsed(start);
  report.status.add(ns);report.status_limit.add(ns);observe(report,result);return result;
}
sdk::Result wait_command(host::NativeContinuousHost& instance,std::uint64_t ticket,Clock::time_point deadline,bool durable){
  auto view=instance.snapshot();
  for(;;){
    sdk::host::CommandResult result;
    if(auto r=instance.command_result(ticket,result);!r)return r;
    if(result.complete){
      if(!result.result)return result.result;
      return durable&&!result.durable?sdk::Result{sdk::Code::corrupt,"engine control completed without a durable receipt"}:sdk::Result{};
    }
    if(Clock::now()>=deadline)return {sdk::Code::deadline,"mixed command exceeded the declared run duration"};
    view=instance.snapshot(view.revision,1000);
    if(!view.result)return view.result;
  }
}
sdk::Result command(host::NativeContinuousHost& instance,host::Command value,MixedReport& report,Clock::time_point deadline,bool durable){
  const auto start=Clock::now();std::uint64_t ticket=0;
  if(auto r=instance.enqueue(std::move(value),ticket);!r)return r;
  const auto result=wait_command(instance,ticket,deadline,durable);
  if(durable){const auto ns=elapsed(start);report.control.add(ns);report.control_limit.add(ns);}
  return result;
}
sdk::Result through(host::NativeContinuousHost& instance,std::uint64_t target,MixedReport& report,Clock::time_point deadline){
  if(auto r=command(instance,{host::CommandKind::through,{},{},target},report,deadline,false);!r)return r;
  auto view=snapshot(instance,report);
  for(;;){
    if(!view.result)return view.result;
    if(view.completed_cycles==target&&!view.cycle_pending)return {};
    if(view.completed_cycles>target)return {sdk::Code::corrupt,"mixed host ran beyond its accepted cycle target"};
    if(Clock::now()>=deadline)return {sdk::Code::deadline,"mixed workload exceeded the declared run duration"};
    (void)instance.snapshot(view.revision,1000);view=snapshot(instance,report);
  }
}
struct Completion {
  std::mutex mutex;std::condition_variable ready;bool complete=false;sdk::Result result;
  void finish(sdk::Result value){
    {std::lock_guard lock(mutex);result=std::move(value);complete=true;}ready.notify_one();
  }
  sdk::Result wait(Clock::time_point deadline){
    std::unique_lock lock(mutex);
    if(!ready.wait_until(lock,deadline,[&]{return complete;}))return {sdk::Code::deadline,"mixed storage completion exceeded the declared run duration"};
    return result;
  }
};
struct Gate {
  std::mutex mutex;std::condition_variable changed;bool entered=false,released=false;
  void release(){ {std::lock_guard lock(mutex);released=true;}changed.notify_all(); }
};
struct Release {std::shared_ptr<Gate> gate;~Release(){gate->release();}};
sdk::Result pressure(host::NativeContinuousHost& instance,MixedReport& report,Clock::time_point deadline){
  auto gate=std::make_shared<Gate>();Release release{gate};
  auto first=std::make_shared<Completion>(),second=std::make_shared<Completion>();
  if(auto r=instance.submit([gate](ev::ICommitStore&){
      std::unique_lock lock(gate->mutex);gate->entered=true;gate->changed.notify_all();
      if(!gate->changed.wait_for(lock,std::chrono::seconds(15),[&]{return gate->released;}))
        return sdk::Result{sdk::Code::deadline,"mixed pressure controller did not release its bounded test gate"};
      return sdk::Result{};
    },[first](sdk::Result r){first->finish(std::move(r));});!r)return r;
  {
    std::unique_lock lock(gate->mutex);
    if(!gate->changed.wait_until(lock,deadline,[&]{return gate->entered;}))return {sdk::Code::deadline,"mixed storage pressure did not start"};
  }
  if(auto r=instance.submit([](ev::ICommitStore& store){
      ev::CommitBatch batch;batch.operation_id="mixed-pressure-commit";batch.request_digest=sdk::sha256("mixed-pressure/1");
      batch.records.push_back({"qualification.mixed","pressure","committed",{},false});
      ev::CommitReceipt receipt;return store.commit(batch,receipt);
    },[second](sdk::Result r){second->finish(std::move(r));});!r)return r;
  if(instance.submit([](ev::ICommitStore&){return sdk::Result{};},[](sdk::Result){}).code!=sdk::Code::capacity)
    return {sdk::Code::corrupt,"mixed pressure did not enforce the bounded storage queue"};
  ++report.task_rejections;
  std::uint64_t pause=0,rejected=0;
  if(auto r=instance.enqueue({host::CommandKind::pause,{},{},0},pause);!r)return r;
  if(instance.enqueue({host::CommandKind::pause,{},{},0},rejected).code!=sdk::Code::capacity)
    return {sdk::Code::corrupt,"mixed pressure did not enforce the bounded control queue"};
  ++report.command_rejections;
  const auto view=snapshot(instance,report);
  if(!view.busy||view.queued_commands!=1||view.queued_tasks!=1)
    return {sdk::Code::corrupt,"mixed pressure snapshot lost bounded outstanding work"};
  // Deliberately stall only the storage worker. The owner still serves cached
  // snapshots and bounded admission; this is queue pressure, not device I/O.
  {
    std::unique_lock lock(gate->mutex);
    gate->changed.wait_for(lock,std::chrono::milliseconds(report.profile.blocked_lane_ms),[]{return false;});
  }
  (void)snapshot(instance,report);gate->release();
  if(auto r=first->wait(deadline);!r)return r;
  if(auto r=second->wait(deadline);!r)return r;
  if(auto r=wait_command(instance,pause,deadline,false);!r)return r;
  auto verified=std::make_shared<Completion>();
  if(auto r=instance.submit([](ev::ICommitStore& store){
      ev::StoredRecord record;ev::CommitReceipt receipt;
      if(auto result=store.latest("qualification.mixed","pressure",record);!result)return result;
      if(auto result=store.lookup_operation("mixed-pressure-commit",receipt);!result)return result;
      return record.record.payload=="committed"&&record.generation==receipt.generation?sdk::Result{}:
        sdk::Result{sdk::Code::corrupt,"queued pressure commit and receipt do not match"};
    },[verified](sdk::Result r){verified->finish(std::move(r));});!r)return r;
  if(auto r=verified->wait(deadline);!r)return r;
  report.pressure_verified=true;return {};
}
sdk::Result probe(host::NativeContinuousHost& instance,const sdk::EngineIdentity& source,MixedReport& report,Clock::time_point deadline){
  struct State:Completion {std::uint64_t nodes=0;};
  auto state=std::make_shared<State>();const auto started=Clock::now();
  if(auto r=instance.submit([state,source](ev::ICommitStore& store){
      ev::reset_native_index_statistics(store);ev::ObservationClaim claim;
      if(auto result=store.lookup_observation(source.source_uid,source.epoch,1,claim);!result)return result;
      sdk::Bytes payload,receipt;
      if(auto result=store.get_blob(claim.payload_digest,payload);!result)return result;
      if(auto result=store.get_blob(claim.receipt_digest,receipt);!result)return result;
      state->nodes=ev::native_index_statistics(store).nodes_read;
      if(claim.source_uid!=source.source_uid||claim.epoch!=source.epoch||claim.sequence!=1||
         sdk::sha256(payload)!=claim.payload_digest||sdk::sha256(receipt)!=claim.receipt_digest)
        return sdk::Result{sdk::Code::corrupt,"mixed historical probe returned unverified evidence"};
      return sdk::Result{};
    },[state](sdk::Result r){state->finish(std::move(r));});!r)return r;
  if(auto r=state->wait(deadline);!r)return r;
  const auto ns=elapsed(started);report.probe.add(ns);report.probe_limit.add(ns);++report.probes;
  report.lookup_maximum_nodes=std::max(report.lookup_maximum_nodes,state->nodes);return {};
}
sdk::Result inventory(std::string_view directory,const host::Status& status,MixedReport& report){
  Resources resources;
  if(auto r=read_resources(resources);!r)return r;
  if(resources.peak_resident_bytes>report.profile.maximum_resident_bytes)violation(report,"mixed process peak resident bytes exceed declared budget");
  if(auto r=measure_disk(directory,report.profile.maximum_files,report.disk);!r)return r;
  const auto allocated=report.disk.allocated_bytes+
    (report.profile.version==2?report.disk.directory_allocated_bytes:0);
  report.sampled_peak_disk_bytes=std::max({report.sampled_peak_disk_bytes,report.disk.logical_bytes,allocated});
  report.sampled_peak_files=std::max(report.sampled_peak_files,report.disk.files);
  report.final_generation=status.store.generation;report.final_accounted_bytes=status.store.physical_bytes;
  if(report.sampled_peak_disk_bytes>report.profile.maximum_disk_bytes)violation(report,"mixed sampled store disk bytes exceed declared budget");
  return {};
}
sdk::Result quota_probe(std::string_view directory,MixedReport& report){
  ev::StoreOptions options;options.maximum_bytes=1048576;std::unique_ptr<ev::ICommitStore> store;
  if(auto r=ev::open_native_store(directory,options,store);!r)return r;
  const auto before=store->info();ev::CommitBatch rejected;
  rejected.operation_id="mixed-quota-overflow";rejected.request_digest=sdk::sha256("mixed-quota-overflow");
  sdk::Bytes bytes(2097152,'q');const auto digest=sdk::sha256(bytes);rejected.blobs.push_back({digest,std::move(bytes),{}});
  ev::CommitReceipt receipt;
  if(store->commit(rejected,receipt).code!=sdk::Code::capacity)return {sdk::Code::corrupt,"mixed quota probe did not reject over-budget admission"};
  const auto after=store->info();
  if(after.generation!=before.generation||after.root_digest!=before.root_digest||!after.writable)
    return {sdk::Code::corrupt,"mixed quota rejection changed canonical state or poisoned admission"};
  ev::CommitBatch valid;valid.operation_id="mixed-quota-recovery";valid.request_digest=sdk::sha256(valid.operation_id);
  valid.records.push_back({"qualification.mixed","quota","bounded",{},false});
  if(auto r=store->commit(valid,receipt);!r)return r;
  ev::AuditReport audit;if(auto r=store->audit(audit);!r)return r;
  report.quota_rejection_verified=true;return {};
}
std::map<std::string,sdk::Digest> state_digests(const sdk::SessionView& view){
  std::map<std::string,sdk::Digest> result;
  for(const auto& value:view.engines)result.emplace(value.identity.instance_id,sdk::state_digest(value.state));
  return result;
}
void configure_limits(MixedReport& report){
  const auto& p=report.profile;
  report.status_limit.limit_ns=p.maximum_status_p99_us*1000;
  report.control_limit.limit_ns=p.maximum_control_p99_us*1000;
  report.probe_limit.limit_ns=p.maximum_probe_p99_us*1000;
  report.dispatch_limit.limit_ns=p.maximum_dispatch_p99_us*1000;
  report.queue_limit.limit_ns=p.maximum_queue_p99_us*1000;
  report.step_commit_limit.limit_ns=p.maximum_step_commit_p99_us*1000;
  report.projection_limit.limit_ns=p.maximum_projection_p99_us*1000;
}
void TimingCollector::observe(const sdk::host::TimingObservation& value) noexcept {
  if(value.result!=sdk::Code::ok)return;
  switch(value.kind){
    case sdk::host::TimingKind::step_dispatch:report_.dispatch.add(value.elapsed_ns);report_.dispatch_limit.add(value.elapsed_ns);break;
    case sdk::host::TimingKind::step_queue_wait:report_.queue.add(value.elapsed_ns);report_.queue_limit.add(value.elapsed_ns);break;
    case sdk::host::TimingKind::step_commit_completion:report_.step_commit.add(value.elapsed_ns);report_.step_commit_limit.add(value.elapsed_ns);break;
    case sdk::host::TimingKind::step_projection:report_.projection.add(value.elapsed_ns);report_.projection_limit.add(value.elapsed_ns);break;
  }
}
sdk::Result final_engines(const sdk::SessionDefinition& definition,const sdk::SessionView& final,MixedReport& report){
  report.engines.clear();
  for(const auto& scheduled:definition.engines){
    const auto found=std::find_if(final.engines.begin(),final.engines.end(),[&](const auto& engine){return engine.identity==scheduled.identity;});
    if(found==final.engines.end())return {sdk::Code::corrupt,"mixed final view lost an admitted engine"};
    const auto& state=found->state;
    if(scheduled.identity.type_id==engines::vlm_plan){
      if(state.phase!=sdk::Phase::completed||state.tick==0||state.tick>report.profile.cycles)
        return {sdk::Code::corrupt,"mixed finite VLM plan did not complete within the declared workload"};
      report.finite_plan_completed=true;
    }else{
      const auto expected=scheduled.identity.type_id==engines::environment?report.profile.cycles-2:report.profile.cycles/scheduled.cadence;
      if(state.tick!=expected||state.phase!=sdk::Phase::running)
        return {sdk::Code::corrupt,"mixed engine clock did not match the completed cycle schedule: "+scheduled.identity.instance_id};
    }
    report.engines.push_back({scheduled.identity,sdk::sha256(sdk::encode_envelope(scheduled.configuration)),
      sdk::sha256(sdk::encode_envelope(scheduled.input)),sdk::state_digest(state),scheduled.cadence,state.tick,state.revision,state.phase});
  }
  return {};
}
sdk::Result finalize(sdk::Result result,Clock::time_point started,MixedReport& report){
  const auto& profile=report.profile;
  report.wall_ns=elapsed(started);report.finished_utc=utc();
  const auto resource_result=read_resources(report.after);
  if(!result)violation(report,std::string(sdk::code_name(result.code))+": "+result.message);
  if(!resource_result)violation(report,resource_result.message);
  if(report.after.peak_resident_bytes>profile.maximum_resident_bytes)violation(report,"mixed process peak resident bytes exceed declared budget");
  if(report.workload_ns<profile.minimum_run_ms*1000000)violation(report,"mixed active workload did not meet the declared minimum duration");
  if(report.wall_ns>profile.maximum_run_ms*1000000)violation(report,"mixed total run duration exceeds declared budget");
  if(profile.version==1){
    if(report.status.percentile_upper_ns(99)>profile.maximum_status_p99_us*1000)violation(report,"mixed cached snapshot p99 upper bound exceeds declared budget");
    if(report.control.percentile_upper_ns(99)>profile.maximum_control_p99_us*1000)violation(report,"mixed durable control p99 upper bound exceeds declared budget");
    if(report.probe.percentile_upper_ns(99)>profile.maximum_probe_p99_us*1000)violation(report,"mixed queued historical probe p99 upper bound exceeds declared budget");
  }else{
    const std::pair<const ThresholdCounter*,const char*> gates[]={
      {&report.status_limit,"snapshot"},{&report.control_limit,"control"},{&report.probe_limit,"probe"},
      {&report.dispatch_limit,"dispatch"},{&report.queue_limit,"queue"},{&report.step_commit_limit,"step commit"},{&report.projection_limit,"projection"}};
    for(const auto& [counter,name]:gates)if(!counter->passes(99))violation(report,std::string("mixed exact p99 threshold failed or has no samples: ")+name);
  }
  if(report.reopen_ns>profile.maximum_reopen_us*1000)violation(report,"mixed compatible reopen exceeds declared budget");
  if(report.lookup_maximum_nodes>profile.maximum_lookup_nodes)violation(report,"mixed historical evidence probe exceeds declared index-work budget");
  if(report.campaign_interruptions)violation(report,"campaign resumed after interruption; continuous qualification and complete timing coverage are not established");
  report.passed=result.ok()&&resource_result.ok()&&report.violations.empty();
  report.continuous_qualified=report.campaign&&report.campaign_complete&&report.passed&&report.campaign_attempt==1;
  if(!result)return result;
  if(!resource_result)return resource_result;
  return report.passed?sdk::Result{}:sdk::Result{sdk::Code::capacity,"mixed qualification exceeded its predeclared operating budget or continuity gate"};
}
}
sdk::Result run_mixed_profile(const MixedProfile& profile,const sdk::SessionDefinition& definition,
    std::string_view directory,MixedReport& report,std::function<void(std::uint64_t)> progress){
  using namespace mixed_detail;
  report.profile=profile;report.started_utc=utc();const auto started=Clock::now();
  configure_limits(report);TimingCollector timings(report);
  const auto deadline=started+std::chrono::milliseconds(profile.maximum_run_ms);
  const auto execute=[&]()->sdk::Result{
    if(auto r=validate_mixed_definition(definition,profile);!r)return r;
    if(profile.required_device_class=="dedicated"&&report.device.device_class!="dedicated")
      return {sdk::Code::invalid,"dedicated mixed profile requires campaign-create with a dedicated device declaration"};
    if(auto r=read_resources(report.before);!r)return r;
    if(auto r=read_environment(directory,report.environment);!r)return r;
    if(auto r=quota_probe(utf8(native_path(directory)/"quota-store"),report);!r)return r;
    const auto store_path=utf8(native_path(directory)/"store");
    {
      sdk::EngineModuleCatalog catalog;
      if(auto r=engines::register_builtin_engines(catalog);!r)return r;
      if(auto r=catalog.freeze();!r)return r;
      ev::StoreOptions options;options.maximum_bytes=profile.maximum_store_bytes;
      std::unique_ptr<ev::ICommitStore> store;if(auto r=ev::open_native_store(store_path,options,store);!r)return r;
      if(store->generation()!=0)return {sdk::Code::conflict,"mixed qualification requires an isolated fresh store"};
      report.store_id=store->info().store_id;
      ev::CommitBatch capacity;capacity.operation_id="mixed-capacity";
      const auto payload=sdk::pack({"store-budget/1",std::to_string(profile.maximum_store_bytes)});
      capacity.request_digest=sdk::sha256(payload);capacity.records.push_back({"operator.policy","capacity",payload,{},false});
      ev::CommitReceipt receipt;if(auto r=store->commit(capacity,receipt);!r)return r;
      sdk::ProjectionService projection;sdk::SessionService runtime(catalog,*store,projection);sdk::SessionRunner runner(runtime,*store);
      if(auto r=runner.initialize(definition);!r)return r;
    }
    const auto environment=std::find_if(definition.engines.begin(),definition.engines.end(),[](const auto& item){return item.identity.type_id==engines::environment;});
    const auto cells=std::find_if(definition.engines.begin(),definition.engines.end(),[](const auto& item){return item.identity.type_id==engines::cell_behavior;});
    const auto workload_started=Clock::now();
    std::map<std::string,sdk::Digest> prior_states;ev::StoreInfo prior_store;
    for(unsigned segment=0;segment<2;++segment){
      host::NativeSession session;const auto reopen=Clock::now();
      if(auto r=session.open(store_path);!r)return r;
      if(auto r=session.ensure_engines();!r)return r;
      if(segment==1){
        report.reopen_ns=elapsed(reopen);const auto recovered=session.store().info();
        if(recovered.store_id!=prior_store.store_id||recovered.root_digest!=prior_store.root_digest||recovered.generation!=prior_store.generation||
           session.runner().progress().cycle_pending||session.runner().progress().completed_cycles!=profile.restart_after_cycle||
           state_digests(session.runtime().view())!=prior_states)
          return {sdk::Code::corrupt,"mixed orderly restart changed the acknowledged cycle, canonical root or engine state"};
        report.restart_identity_verified=true;
      }
      host::Options options;options.period_ms=static_cast<std::uint32_t>(profile.period_ms);
      options.command_capacity=1;options.task_capacity=2;options.initially_running=false;
      options.timing_observer=&timings;
      host::NativeContinuousHost instance(session.runtime(),session.runner(),session.store(),options);
      if(auto r=instance.start();!r)return r;
      auto initial=snapshot(instance,report);
      while(initial.engines.size()!=definition.engines.size()){
        if(!initial.result)return initial.result;
        if(Clock::now()>=deadline)return {sdk::Code::deadline,"mixed host did not publish its initial recovered view"};
        (void)instance.snapshot(initial.revision,1000);initial=snapshot(instance,report);
      }
      if(initial.completed_cycles!=(segment==0?0:profile.restart_after_cycle)||initial.cycle_pending)
        return {sdk::Code::corrupt,"mixed host initial view differs from the quiescent admitted state"};
      if(segment==0){
        const auto id=environment->identity.instance_id;
        if(auto r=command(instance,{host::CommandKind::engine_pause,id,"mixed-environment-pause",0},report,deadline,true);!r)return r;
        auto paused=snapshot(instance,report);
        for(;;){
          const auto found=std::find_if(paused.engines.begin(),paused.engines.end(),[&](const auto& engine){return engine.identity.instance_id==id;});
          if(found!=paused.engines.end()&&found->phase==sdk::Phase::paused)break;
          if(Clock::now()>=deadline)return {sdk::Code::deadline,"mixed pause did not reach the cached view"};
          (void)instance.snapshot(paused.revision,1000);paused=snapshot(instance,report);
        }
        if(auto r=command(instance,{host::CommandKind::engine_pause,id,"mixed-environment-pause",0},report,deadline,true);!r)return r;
        // A completed ticket can precede cached-view publication. Cross the
        // actual serialized storage lane before asserting that the retry made
        // no canonical transition; a stale snapshot cannot establish this.
        auto retry_barrier=std::make_shared<Completion>();
        if(auto r=instance.submit([root=paused.store.root_digest,generation=paused.store.generation](ev::ICommitStore& store){
            const auto current=store.info();
            return current.root_digest==root&&current.generation==generation?sdk::Result{}:
              sdk::Result{sdk::Code::corrupt,"mixed repeated control changed the canonical root or generation"};
          },[retry_barrier](sdk::Result r){retry_barrier->finish(std::move(r));});!r)return r;
        if(auto r=retry_barrier->wait(deadline);!r)return r;
        const auto retried=snapshot(instance,report);
        const auto state_of=[&](const host::Status& value){
          const auto found=std::find_if(value.engines.begin(),value.engines.end(),[&](const auto& engine){return engine.identity.instance_id==id;});
          return found==value.engines.end()?sdk::Digest{}:found->state_sha256;
        };
        if(state_of(paused).empty()||state_of(paused)!=state_of(retried)||paused.store.root_digest!=retried.store.root_digest)
          return {sdk::Code::corrupt,"mixed repeated engine-control identity committed twice"};
        report.control_retry_verified=true;
        if(auto r=through(instance,2,report,deadline);!r)return r;
        if(auto r=pressure(instance,report,deadline);!r)return r;
        if(auto r=command(instance,{host::CommandKind::engine_resume,id,"mixed-environment-resume",0},report,deadline,true);!r)return r;
      }
      const auto target=segment==0?profile.restart_after_cycle:profile.cycles;
      auto completed=snapshot(instance,report).completed_cycles;
      while(completed<target){
        const auto next=std::min(target,completed+profile.checkpoint_cycles);
        if(auto r=through(instance,next,report,deadline);!r)return r;
        // The absolute target is now idle. Every historical read still goes
        // through the same serialized lane as normal mesh/storage adapters.
        if(auto r=probe(instance,cells->identity,report,deadline);!r)return r;
        const auto view=snapshot(instance,report);
        if(auto r=inventory(store_path,view,report);!r)return r;
        completed=next;if(progress)progress(completed);
      }
      instance.shutdown();const auto stopped=snapshot(instance,report);
      if(!stopped.stopped||stopped.cycle_pending||stopped.completed_cycles!=target||!stopped.result)
        return {sdk::Code::corrupt,"mixed host did not drain its complete acknowledged target"};
      report.owner_wakeups+=stopped.owner_wakeups;report.completed_tasks+=stopped.completed_tasks;
      if(segment==0){prior_store=session.store().info();prior_states=state_digests(session.runtime().view());}
      else{
        if(auto r=final_engines(definition,session.runtime().view(),report);!r)return r;
        report.workload_ns=elapsed(workload_started);
        const auto audit_started=Clock::now();ev::AuditReport audit;const auto audited=session.store().audit(audit);
        report.audit_ns=elapsed(audit_started);report.audit_passed=audited.ok();if(!audited)return audited;
        if(auto r=inventory(store_path,stopped,report);!r)return r;
      }
    }
    return {};
  };
  sdk::Result result;
  try{result=execute();}
  catch(const std::exception& error){result={sdk::Code::io_error,error.what()};}
  return finalize(result,started,report);
}
}
