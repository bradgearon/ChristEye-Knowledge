#include "mixed_internal.hpp"
#include "native_paths.hpp"
#include "anaiero/sdk/engine_files.hpp"
#include "anaiero/sdk/native_session.hpp"
#include "christeye/living/durable_io.hpp"
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <sstream>

namespace anaiero::qualification {
namespace {
namespace fs=std::filesystem;
namespace ev=sdk::evidence;
namespace host=sdk::host;
namespace os=christeye::living::durable::os;
using namespace mixed_detail;
constexpr std::string_view state_kind="qualification.campaign",attempt_kind="qualification.attempt";
constexpr std::uint64_t maximum_attempts=128,maximum_checkpoints=65536;
struct State {
  sdk::Digest profile,session,definition,build,device;
  std::uint64_t attempt=0,serial=0,confirmed_cycles=0,target_cycles=0;
  std::uint64_t active_ns=0,attempt_ns=0,recovered_cycles=0;
  bool active=false,complete=false,pressure=false,control_retry=false,quota=false,restart=false,qualified=false;
  ev::StoredRecord retained;
};
bool number(std::string_view bytes,std::uint64_t& value){
  const auto result=std::from_chars(bytes.data(),bytes.data()+bytes.size(),value);
  return !bytes.empty()&&result.ec==std::errc{}&&result.ptr==bytes.data()+bytes.size()&&std::to_string(value)==bytes;
}
sdk::Bytes encode(const State& s){
  return sdk::pack({"ANAIERO-MIXED-CAMPAIGN/1",s.profile,s.session,s.definition,s.build,
    std::to_string(s.attempt),std::to_string(s.serial),std::to_string(s.confirmed_cycles),std::to_string(s.target_cycles),
    std::to_string(s.active_ns),std::to_string(s.attempt_ns),std::to_string(s.recovered_cycles),
    s.active?"1":"0",s.complete?"1":"0",s.pressure?"1":"0",s.control_retry?"1":"0",s.quota?"1":"0",s.restart?"1":"0",s.qualified?"1":"0",s.device});
}
sdk::Result decode(const ev::StoredRecord& retained,State& state){
  std::vector<std::string> f;
  if(retained.record.tombstone||!sdk::unpack(retained.record.payload,f,20,16384)||f.size()!=20||f[0]!="ANAIERO-MIXED-CAMPAIGN/1")
    return {sdk::Code::corrupt,"campaign state is malformed"};
  State s;s.profile=f[1];s.session=f[2];s.definition=f[3];s.build=f[4];s.device=f[19];
  if(!sdk::valid_digest(s.profile)||!sdk::valid_digest(s.session)||!sdk::valid_digest(s.definition)||!sdk::valid_digest(s.build)||(!s.device.empty()&&!sdk::valid_digest(s.device)))
    return {sdk::Code::corrupt,"campaign identity is malformed"};
  std::uint64_t* values[]={&s.attempt,&s.serial,&s.confirmed_cycles,&s.target_cycles,&s.active_ns,&s.attempt_ns,&s.recovered_cycles};
  for(std::size_t i=0;i<7;++i)if(!number(f[i+5],*values[i]))return {sdk::Code::corrupt,"campaign integer is noncanonical"};
  bool* flags[]={&s.active,&s.complete,&s.pressure,&s.control_retry,&s.quota,&s.restart,&s.qualified};
  for(std::size_t i=0;i<7;++i){if(f[i+12]!="0"&&f[i+12]!="1")return {sdk::Code::corrupt,"campaign flag is malformed"};*flags[i]=f[i+12]=="1";}
  if(s.attempt>maximum_attempts||s.serial>maximum_checkpoints||s.confirmed_cycles>s.target_cycles||s.target_cycles>10000||
     s.attempt_ns>s.active_ns||(s.complete&&s.active)||(s.qualified&&(!s.complete||s.attempt!=1))||s.active_ns>maximum_attempts*14ULL*24*60*60*1000000000ULL)
    return {sdk::Code::corrupt,"campaign state exceeds declared bounds"};
  s.retained=retained;state=std::move(s);return {};
}
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
void restore_flags(const State& state,MixedReport& report){
  report.campaign=true;report.campaign_attempt=state.attempt;report.campaign_interruptions=state.attempt?state.attempt-1:0;
  report.recovered_cycles=state.recovered_cycles;report.active_ns_lower_bound=state.active_ns;
  report.pressure_verified=state.pressure;report.control_retry_verified=state.control_retry;
  report.quota_rejection_verified=state.quota;report.restart_identity_verified=state.restart;
  report.campaign_complete=state.complete;
}
std::string status_json(const State& state,const MixedReport& report,std::string_view status,const ev::CommitReceipt& receipt){
  // Every token below is internal or a validated digest. The nested report has
  // its own complete JSON escaping. This cache cannot authorize resume.
  std::ostringstream out;
  out<<"{\"schema\":\"anaiero.mixed-campaign-status/1\",\"cached\":true,\"liveness\":\"unknown\",\"status\":\""<<status
    <<"\",\"attempt\":"<<state.attempt<<",\"checkpoint\":"<<state.serial<<",\"confirmed_cycles\":"<<state.confirmed_cycles
    <<",\"target_cycles\":"<<state.target_cycles<<",\"generation\":"<<receipt.generation<<",\"root_sha256\":\""<<receipt.root_digest
    <<"\",\"active_ns_lower_bound\":"<<state.active_ns<<",\"report\":"<<mixed_report_json(report)<<"}\n";return out.str();
}
sdk::Result publish_cache(const fs::path& directory,std::string_view bytes){
  std::error_code error;const auto path=directory/"status.json";const auto status=fs::symlink_status(path,error);
  if(error&&error!=std::errc::no_such_file_or_directory)return {sdk::Code::io_error,"cannot inspect campaign status cache"};
  if(fs::exists(status)&&!fs::is_regular_file(status))return {sdk::Code::conflict,"campaign status cache is not a regular file"};
  return os::atomic_write(path,bytes)?sdk::Result{}:sdk::Result{sdk::Code::io_error,"cannot publish campaign status cache; canonical progress remains retained"};
}
sdk::Result save(ev::ICommitStore& store,const fs::path& directory,State& state,MixedReport& report,
    std::string_view status,std::uint64_t attempt_elapsed_ns=0){
  if(state.serial>=maximum_checkpoints)return {sdk::Code::capacity,"campaign checkpoint bound exhausted"};
  if(attempt_elapsed_ns<state.attempt_ns)return {sdk::Code::corrupt,"campaign monotonic attempt time moved backwards"};
  State next=state;++next.serial;next.active_ns+=attempt_elapsed_ns-next.attempt_ns;next.attempt_ns=attempt_elapsed_ns;
  next.pressure=report.pressure_verified;next.control_retry=report.control_retry_verified;next.quota=report.quota_rejection_verified;
  next.restart=report.restart_identity_verified;next.active=status=="running";next.complete=report.campaign_complete;
  next.qualified=report.continuous_qualified;
  restore_flags(next,report);const auto wire=encode(next),json=mixed_report_json(report);
  if(json.size()>128U*1024U)return {sdk::Code::capacity,"campaign checkpoint report exceeds 128 KiB"};
  ev::CommitBatch batch;batch.operation_id="mixed-campaign-"+std::to_string(next.serial);
  batch.request_digest=sdk::sha256(sdk::pack({wire,json,std::string(status)}));
  batch.guards.push_back({std::string(state_kind),"main",state.retained.revision,state.retained.digest});
  batch.records.push_back({std::string(state_kind),"main",wire,{},false});
  if(state.retained.revision){
    State previous;if(auto r=decode(state.retained,previous);!r)return r;
    if(previous.attempt&&next.attempt>previous.attempt){
      ev::StoredRecord old;if(auto r=store.latest(attempt_kind,std::to_string(previous.attempt),old);!r)return r;
      std::vector<std::string> fields;
      if(!sdk::unpack(old.record.payload,fields,3,128U*1024U+4096)||fields.size()!=3||fields[0]!="ANAIERO-MIXED-ATTEMPT/1")
        return {sdk::Code::corrupt,"prior campaign attempt is malformed"};
      fields[1]=previous.active?"interrupted":"failed";
      batch.guards.push_back({std::string(attempt_kind),old.record.key,old.revision,old.digest});
      batch.records.push_back({std::string(attempt_kind),old.record.key,sdk::pack(fields),{},false});
    }
  }
  if(next.attempt)batch.records.push_back({std::string(attempt_kind),std::to_string(next.attempt),
    sdk::pack({"ANAIERO-MIXED-ATTEMPT/1",std::string(status),json}),{},false});
  ev::CommitReceipt receipt;if(auto r=store.commit(batch,receipt);!r)return r;
  if(auto r=store.latest(state_kind,"main",next.retained);!r)return r;
  state=std::move(next);
  return publish_cache(directory,status_json(state,report,status,receipt));
}
sdk::Result read_inputs(const fs::path& directory,MixedReport& report,sdk::SessionDefinition& definition){
  sdk::Bytes profile,session,wire;
  if(auto r=read_file(directory/"profile.cevqualification",16384,profile);!r)return r;
  if(auto r=read_file(directory/"session-input.cevsession",16384,session);!r)return r;
  if(auto r=read_file(directory/"session.definition",8U*1024U*1024U,wire);!r)return r;
  if(auto r=parse_mixed_profile(profile,report.profile);!r)return r;
  if(auto r=sdk::decode_session_definition(wire,definition);!r)return r;
  if(sdk::encode_session_definition(definition)!=wire)return {sdk::Code::corrupt,"campaign session definition is noncanonical"};
  if(auto r=validate_mixed_definition(definition,report.profile);!r)return r;
  report.profile_sha256=sdk::sha256(profile);report.session_text_sha256=sdk::sha256(session);report.session_definition_sha256=sdk::sha256(wire);
  std::error_code error;const auto device_file=directory/"device.cevdevice";const auto status=fs::symlink_status(device_file,error);
  if(error&&error!=std::errc::no_such_file_or_directory)return {sdk::Code::io_error,"cannot inspect campaign device declaration"};
  if(fs::exists(status)){
    sdk::Bytes bytes;if(auto r=read_file(device_file,16384,bytes);!r)return r;
    if(auto r=parse_device_declaration(bytes,report.device);!r)return r;
    report.device_sha256=sdk::sha256(bytes);
  }
  return read_environment(utf8(directory),report.environment);
}
sdk::Result preserve_attempt(ev::ICommitStore& store,const fs::path& directory,const State& state){
  if(!state.attempt)return {};
  ev::StoredRecord record;if(auto r=store.latest(attempt_kind,std::to_string(state.attempt),record);!r)return r;
  std::vector<std::string> fields;
  if(record.record.tombstone||!sdk::unpack(record.record.payload,fields,3,128U*1024U+4096)||fields.size()!=3||fields[0]!="ANAIERO-MIXED-ATTEMPT/1")
    return {sdk::Code::corrupt,"prior campaign attempt is malformed"};
  const auto file="attempt-"+std::to_string(state.attempt)+"-"+(state.active?"interrupted":"failed")+".json";
  return engines::write_new_artifact(utf8(directory/file),fields[2]);
}
sdk::Result serialized(host::NativeContinuousHost& instance,host::StoreTask task){
  auto completion=std::make_shared<std::promise<sdk::Result>>();auto future=completion->get_future();
  if(auto r=instance.submit(std::move(task),[completion](sdk::Result result){completion->set_value(std::move(result));});!r)return r;
  return future.get();
}
}

sdk::Result create_mixed_campaign(std::string_view profile_bytes,std::string_view session_text,
    const sdk::SessionDefinition& definition,std::string_view requested,std::string_view device_bytes){
  MixedReport report;
  if(profile_bytes.size()>16384||session_text.size()>16384)return {sdk::Code::capacity,"campaign text inputs exceed 16 KiB"};
  if(auto r=parse_mixed_profile(profile_bytes,report.profile);!r)return r;
  if(auto r=validate_mixed_definition(definition,report.profile);!r)return r;
  if(!device_bytes.empty()){
    if(auto r=parse_device_declaration(device_bytes,report.device);!r)return r;
    report.device_sha256=sdk::sha256(device_bytes);
  }else if(report.profile.minimum_run_ms)return {sdk::Code::invalid,"elapsed campaign requires an exact device declaration"};
  if(report.profile.required_device_class=="dedicated"&&report.device.device_class!="dedicated")
    return {sdk::Code::invalid,"this campaign requires a declared dedicated device"};
  const auto definition_bytes=sdk::encode_session_definition(definition);
  if(definition_bytes.empty()||definition_bytes.size()>8U*1024U*1024U)return {sdk::Code::capacity,"campaign definition exceeds 8 MiB"};
  const auto directory=native_path(requested);std::error_code error;
  if(!fs::create_directory(directory,error)||error)return {sdk::Code::conflict,"campaign output already exists or cannot be created"};
  os::File lock;if(auto r=lease(directory,lock);!r)return r;
  if(auto r=engines::write_new_artifact(utf8(directory/"profile.cevqualification"),profile_bytes);!r)return r;
  if(auto r=engines::write_new_artifact(utf8(directory/"session-input.cevsession"),session_text);!r)return r;
  if(auto r=engines::write_new_artifact(utf8(directory/"session.definition"),definition_bytes);!r)return r;
  if(!device_bytes.empty())if(auto r=engines::write_new_artifact(utf8(directory/"device.cevdevice"),device_bytes);!r)return r;
  if(auto r=read_environment(requested,report.environment);!r)return r;
  report.profile_sha256=sdk::sha256(profile_bytes);report.session_text_sha256=sdk::sha256(session_text);
  report.session_definition_sha256=sdk::sha256(definition_bytes);report.campaign=true;
  State state;state.profile=report.profile_sha256;state.session=report.session_text_sha256;
  state.definition=report.session_definition_sha256;state.build=report.environment.build_identity;state.device=report.device_sha256;
  sdk::EngineModuleCatalog catalog;if(auto r=engines::register_builtin_engines(catalog);!r)return r;
  if(auto r=catalog.freeze();!r)return r;
  ev::StoreOptions options;options.maximum_bytes=report.profile.maximum_store_bytes;
  std::unique_ptr<ev::ICommitStore> store;if(auto r=ev::open_native_store(utf8(directory/"store"),options,store);!r)return r;
  if(store->generation()!=0)return {sdk::Code::conflict,"campaign requires a fresh canonical store"};
  report.store_id=store->info().store_id;ev::CommitBatch capacity;capacity.operation_id="mixed-capacity";
  const auto payload=sdk::pack({"store-budget/1",std::to_string(report.profile.maximum_store_bytes)});
  capacity.request_digest=sdk::sha256(payload);capacity.records.push_back({"operator.policy","capacity",payload,{},false});
  ev::CommitReceipt receipt;if(auto r=store->commit(capacity,receipt);!r)return r;
  sdk::ProjectionService projection;sdk::SessionService runtime(catalog,*store,projection);sdk::SessionRunner runner(runtime,*store);
  if(auto r=runner.initialize(definition);!r)return r;
  return save(*store,directory,state,report,"ready");
}

sdk::Result mixed_campaign_status(std::string_view requested,std::string& json){
  return read_file(native_path(requested)/"status.json",256U*1024U,json);
}

sdk::Result run_mixed_campaign(std::string_view requested,MixedReport& report,
    const CampaignRunOptions& options,std::function<void(std::uint64_t)> progress){
  report={};const auto started=Clock::now();const auto directory=native_path(requested),store_path=directory/"store";
  os::File lock;if(auto r=lease(directory,lock);!r)return r;
  sdk::SessionDefinition definition;if(auto r=read_inputs(directory,report,definition);!r)return r;
  configure_limits(report);TimingCollector timings(report);State state;
  auto session=std::make_unique<host::NativeSession>();
  if(auto r=session->open(utf8(store_path));!r)return r;
  ev::StoredRecord retained;if(auto r=session->store().latest(state_kind,"main",retained);!r)return r;
  if(auto r=decode(retained,state);!r)return r;
  if(state.profile!=report.profile_sha256||state.session!=report.session_text_sha256||state.definition!=report.session_definition_sha256||
     state.build!=report.environment.build_identity||state.device!=report.device_sha256||sdk::sha256(sdk::encode_session_definition(session->runner().definition()))!=state.definition)
    return {sdk::Code::conflict,"campaign exact profile, session definition or compiled build differs; preserve this attempt and create a new campaign"};
  if(state.complete){
    // Final report publication can itself be interrupted after its canonical
    // checkpoint. Recreate those exact bytes without reexecuting any workload.
    ev::StoredRecord final;if(auto r=session->store().latest(attempt_kind,std::to_string(state.attempt),final);!r)return r;
    std::vector<std::string> fields;
    if(final.record.tombstone||!sdk::unpack(final.record.payload,fields,3,128U*1024U+4096)||fields.size()!=3||
       fields[0]!="ANAIERO-MIXED-ATTEMPT/1"||fields[1]!="completed")return {sdk::Code::corrupt,"completed campaign report is missing"};
    restore_flags(state,report);report.passed=state.qualified;report.continuous_qualified=state.qualified;report.retained_report_json=fields[2];
    ev::CommitReceipt receipt;if(auto r=session->store().lookup_operation("mixed-campaign-"+std::to_string(state.serial),receipt);!r)return r;
    if(receipt.generation!=state.retained.generation)return {sdk::Code::corrupt,"campaign completion receipt differs from its retained checkpoint"};
    if(auto r=publish_cache(directory,status_json(state,report,"completed",receipt));!r)return r;
    const auto name="attempt-"+std::to_string(state.attempt)+"-completed-"+sdk::sha256(fields[2])+".json";
    if(auto r=engines::write_new_artifact(utf8(directory/name),fields[2]);!r)return r;
    if(auto r=engines::write_new_artifact(utf8(directory/"report.json"),fields[2]);!r)return r;
    return state.qualified?sdk::Result{}:sdk::Result{sdk::Code::capacity,"completed campaign retained its original failed qualification"};
  }
  if(state.attempt==maximum_attempts)return {sdk::Code::capacity,"campaign attempt bound exhausted"};
  if(auto r=preserve_attempt(session->store(),directory,state);!r)return r;
  const auto recovered=session->runner().progress();
  if(recovered.completed_cycles<state.confirmed_cycles||recovered.completed_cycles>state.target_cycles||
     (recovered.cycle_pending&&recovered.completed_cycles>=state.target_cycles))
    return {sdk::Code::corrupt,"session progress is outside the campaign's durably admitted target"};
  state.recovered_cycles+=recovered.completed_cycles-state.confirmed_cycles;
  ++state.attempt;state.attempt_ns=0;state.active=true;restore_flags(state,report);
  report.started_utc=utc();report.store_id=session->store().info().store_id;
  if(auto r=read_resources(report.before);!r)return r;
  if(auto r=save(session->store(),directory,state,report,"running");!r)return r;
  if(options.observe)options.observe(CampaignEvent::attempt_started,recovered.completed_cycles);
  const auto workload_started=Clock::now(),deadline=started+std::chrono::milliseconds(report.profile.maximum_run_ms);
  std::optional<std::uint64_t> completed_workload_ns;
  bool session_available=true;
  const auto checkpoint=[&](std::string_view status)->sdk::Result{
    if(!session_available)return {sdk::Code::io_error,"campaign session unavailable for checkpoint; original canonical intent retained"};
    report.cycles=session->runner().progress().completed_cycles;state.confirmed_cycles=report.cycles;
    report.workload_ns=completed_workload_ns.value_or(elapsed(workload_started));
    if(auto r=save(session->store(),directory,state,report,status,report.workload_ns);!r)return r;
    if(options.observe)options.observe(CampaignEvent::checkpoint_committed,report.cycles);
    return {};
  };
  const auto execute=[&]()->sdk::Result{
    if(auto r=quota_probe(utf8(directory/"quota-store"),report);!r)return r;
    const auto environment=std::find_if(definition.engines.begin(),definition.engines.end(),[](const auto& e){return e.identity.type_id==engines::environment;});
    const auto cells=std::find_if(definition.engines.begin(),definition.engines.end(),[](const auto& e){return e.identity.type_id==engines::cell_behavior;});
    for(unsigned segment=0;segment<2;++segment){
      if(auto r=session->ensure_engines();!r)return r;
      if(options.engine_observer)session->runtime().set_observer_sink(options.engine_observer);
      host::Options host_options;host_options.period_ms=static_cast<std::uint32_t>(report.profile.period_ms);
      host_options.command_capacity=1;host_options.task_capacity=2;host_options.initially_running=false;host_options.timing_observer=&timings;
      auto owned_instance=std::make_unique<host::NativeContinuousHost>(session->runtime(),session->runner(),session->store(),host_options);
      auto& instance=*owned_instance;
      if(auto r=instance.start();!r)return r;
      auto initial=snapshot(instance,report);
      while(initial.engines.size()!=definition.engines.size()||initial.cycle_pending){
        if(!initial.result)return initial.result;
        if(Clock::now()>=deadline)return {sdk::Code::deadline,"campaign recovery exceeded declared run duration"};
        (void)instance.snapshot(initial.revision,1000);initial=snapshot(instance,report);
      }
      if(initial.completed_cycles>state.target_cycles)return {sdk::Code::corrupt,"campaign recovery advanced beyond its durable target"};
      const auto save_idle=[&](std::uint64_t target)->sdk::Result{
        // The host is at an absolute idle target; this barrier crosses the same
        // serialized lane as engine writes before taking a campaign checkpoint.
        auto completed=std::make_shared<std::promise<sdk::Result>>();auto future=completed->get_future();
        if(auto r=instance.submit([&](ev::ICommitStore& store){
          state.target_cycles=target;state.confirmed_cycles=report.cycles;report.workload_ns=elapsed(workload_started);
          return save(store,directory,state,report,"running",report.workload_ns);
        },[completed](sdk::Result result){completed->set_value(std::move(result));});!r)return r;
        // A task owns stack captures until its completion. The enclosing host
        // drains on exit; wait to completion even if it has crossed the budget.
        const auto result=future.get();if(!result)return result;
        if(Clock::now()>=deadline)return {sdk::Code::deadline,"campaign checkpoint exceeded declared run duration"};
        if(options.observe)options.observe(CampaignEvent::target_committed,target);
        return {};
      };
      const auto advance=[&](std::uint64_t target)->sdk::Result{
        report.cycles=snapshot(instance,report).completed_cycles;
        if(auto r=save_idle(target);!r)return r;
        if(auto r=through(instance,target,report,deadline);!r)return r;
        if(options.observe)options.observe(CampaignEvent::work_completed,target);
        if(progress)progress(target);
        if(auto r=probe(instance,cells->identity,report,deadline);!r)return r;
        if(auto r=inventory(utf8(store_path),snapshot(instance,report),report);!r)return r;
        report.cycles=target;
        return save_idle(target);
      };
      if(initial.completed_cycles<=2){
        // Stable operation identities reconcile a crash before or after any
        // phase control. A resumed running phase is never reset by an old retry.
        if(auto r=command(instance,{host::CommandKind::engine_pause,environment->identity.instance_id,"mixed-environment-pause",0},report,deadline,true);!r)return r;
        auto before=std::make_shared<ev::StoreInfo>();
        if(auto r=serialized(instance,[before](ev::ICommitStore& store){*before=store.info();return sdk::Result{};});!r)return r;
        if(auto r=command(instance,{host::CommandKind::engine_pause,environment->identity.instance_id,"mixed-environment-pause",0},report,deadline,true);!r)return r;
        if(auto r=serialized(instance,[before](ev::ICommitStore& store){
          const auto after=store.info();return before->generation==after.generation&&before->root_digest==after.root_digest?sdk::Result{}:
            sdk::Result{sdk::Code::corrupt,"campaign repeated phase control changed canonical progress"};
        });!r)return r;
        report.control_retry_verified=true;
        if(initial.completed_cycles<2)if(auto r=advance(2);!r)return r;
        if(!report.pressure_verified)if(auto r=pressure(instance,report,deadline);!r)return r;
        if(auto r=command(instance,{host::CommandKind::engine_resume,environment->identity.instance_id,"mixed-environment-resume",0},report,deadline,true);!r)return r;
      }
      const auto boundary=report.restart_identity_verified?report.profile.cycles:report.profile.restart_after_cycle;
      auto completed=snapshot(instance,report).completed_cycles;
      if(completed>boundary)return {sdk::Code::corrupt,"campaign crossed an unverified restart boundary"};
      while(completed<boundary){
        const auto next=std::min(boundary,completed+report.profile.checkpoint_cycles);
        if(auto r=advance(next);!r)return r;
        completed=next;
      }
      instance.shutdown();const auto stopped=snapshot(instance,report);
      if(!stopped.stopped||stopped.cycle_pending||stopped.completed_cycles!=boundary||!stopped.result)
        return {sdk::Code::corrupt,"campaign host failed to drain its admitted target"};
      report.owner_wakeups+=stopped.owner_wakeups;report.completed_tasks+=stopped.completed_tasks;
      if(!report.restart_identity_verified){
        const auto prior=session->store().info();const auto states=state_digests(session->runtime().view());
        // Release the drained host before replacing its borrowed session.
        session->runtime().set_observer_sink(nullptr);
        owned_instance.reset();
        const auto reopen_started=Clock::now();
        session_available=false;session.reset();session=std::make_unique<host::NativeSession>();
        if(auto r=session->open(utf8(store_path));!r)return r;
        session_available=true;
        if(auto r=session->ensure_engines();!r)return r;
        report.reopen_ns=elapsed(reopen_started);const auto actual=session->store().info();
        if(actual.store_id!=prior.store_id||actual.generation!=prior.generation||actual.root_digest!=prior.root_digest||
           session->runner().progress().completed_cycles!=boundary||session->runner().progress().cycle_pending||
           state_digests(session->runtime().view())!=states)
          return {sdk::Code::corrupt,"campaign orderly restart changed canonical progress or engine state"};
        report.restart_identity_verified=true;if(auto r=checkpoint("running");!r)return r;
      }else break;
    }
    report.cycles=session->runner().progress().completed_cycles;
    if(report.cycles!=report.profile.cycles||session->runner().progress().cycle_pending)return {sdk::Code::corrupt,"campaign workload did not finish"};
    completed_workload_ns=elapsed(workload_started);
    if(auto r=final_engines(definition,session->runtime().view(),report);!r)return r;
    ev::AuditReport audit;const auto auditing=Clock::now();const auto result=session->store().audit(audit);
    report.audit_ns=elapsed(auditing);report.audit_passed=result.ok();if(!result)return result;
    host::Status observed;observed.store=session->store().info();
    if(auto r=inventory(utf8(store_path),observed,report);!r)return r;
    report.campaign_complete=true;return {};
  };
  sdk::Result result;
  try{result=execute();}catch(const std::exception& error){result={sdk::Code::io_error,error.what()};}
  report.workload_ns=completed_workload_ns.value_or(elapsed(workload_started));
  result=finalize(result,started,report);
  const auto saved=checkpoint(report.campaign_complete?"completed":"failed");
  if(!saved){
    // The session interval is finished only after its canonical completion.
    // On an uncertain publication write diagnostic evidence, never a passing
    // report.json. A later resume reconciles a possibly committed checkpoint.
    const auto detail=std::string("campaign checkpoint finalization failed: ")+saved.message;
    violation(report,detail);report.passed=false;report.continuous_qualified=false;
    const auto failed=mixed_report_json(report);
    const auto file="attempt-"+std::to_string(state.attempt)+"-publication-failed-"+sdk::sha256(failed)+".json";
    if(auto r=engines::write_new_artifact(utf8(directory/file),failed);!r)return r;
    return result.ok()?saved:result;
  }
  if(saved&&report.campaign_complete&&options.observe)options.observe(CampaignEvent::completion_committed,report.cycles);
  // Keep the observed failed report even when canonical storage cannot accept
  // the checkpoint. Its content hash makes retries append-only and unambiguous.
  const auto json=mixed_report_json(report);
  const auto name="attempt-"+std::to_string(state.attempt)+"-"+(report.campaign_complete?"completed":"failed")+"-"+sdk::sha256(json)+".json";
  if(auto r=engines::write_new_artifact(utf8(directory/name),json);!r)return r;
  if(report.campaign_complete){
    if(auto r=engines::write_new_artifact(utf8(directory/"report.json"),json);!r)return r;
    if(options.observe)options.observe(CampaignEvent::completed,report.cycles);
  }
  return result;
}
}
