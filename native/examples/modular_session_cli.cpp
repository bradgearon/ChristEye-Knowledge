// Composition and operator presentation for the compiled modular runtime.
#include "anaiero/sdk/engine_files.hpp"
#include "anaiero/sdk/evidence.hpp"
#include "anaiero/sdk/experiments.hpp"
#include "anaiero/sdk/experiment_history.hpp"
#include "anaiero/sdk/lifecycle.hpp"
#include "anaiero/sdk/session_runner.hpp"
#include "anaiero/sdk/storage_tools.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace s=anaiero::sdk;
namespace e=anaiero::engines;
namespace ev=anaiero::sdk::evidence;
namespace life=anaiero::sdk::lifecycle;
namespace fs=std::filesystem;
namespace {
s::Result invalid(std::string message){return {s::Code::invalid,std::move(message)};
}
std::string json(std::string_view text){
  std::ostringstream out;
  out<<'"';
  for(const unsigned char c:text){
    if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);
    else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;
    else out<<static_cast<char>(c);
  }
  out<<'"';
  return out.str();
}
bool number(std::string_view text,std::uint64_t& value){
  const auto r=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&r.ec==std::errc{}&&r.ptr==text.data()+text.size();
}
bool read_file(const fs::path& path,std::string& bytes,std::size_t maximum=8U*1024U*1024U){
  std::error_code error;
  if(fs::is_symlink(path,error)||error)return false;
  const auto size=fs::file_size(path,error);
  if(error||size>maximum)return false;
  std::ifstream in(path,std::ios::binary);
  if(!in)return false;
  bytes.assign(std::istreambuf_iterator<char>(in),{});
  return !in.bad()&&bytes.size()==size;
}
s::Result stored_budget(ev::ICommitStore& store,std::uint64_t& maximum,bool& exists){
  ev::StoredRecord record;
  const auto found=store.latest("operator.policy","capacity",record);
  exists=false;
  if(found.code==s::Code::not_found)return {};
  if(!found)return found;
  std::vector<std::string> fields;
  if(record.record.tombstone||!s::unpack(record.record.payload,fields,2,1024)||fields.size()!=2||fields[0]!="store-budget/1"||
     !number(fields[1],maximum)||std::to_string(maximum)!=fields[1]||maximum<65536)return {s::Code::corrupt,"persisted store budget is invalid"};
  exists=true;return {};
}
bool canonical_root_exists(const fs::path& directory){
  std::error_code error;
  const auto status=fs::symlink_status(directory/"ROOT",error);
  return !error&&fs::is_regular_file(status);
}
std::uint64_t now(){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
  std::chrono::system_clock::now().time_since_epoch()).count());
  }
fs::path executable(const char* argv0){
#ifdef _WIN32
  (void)argv0;
  std::wstring path(32768,L'\0');
  const auto n=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
  if(!n||n>=path.size())return {};
  path.resize(n);
  return fs::path(path);
#else
  std::string path(8192,'\0');
  const auto n=::readlink("/proc/self/exe",path.data(),path.size());
  if(n>0&&static_cast<std::size_t>(n)<path.size()){path.resize(static_cast<std::size_t>(n));
  return fs::path(path);
  }
  return fs::absolute(argv0);
#endif
}
class LifecyclePolicy final:public s::ICommitPolicy {
  life::Service& lifecycle_;
public:
  explicit LifecyclePolicy(life::Service& lifecycle):lifecycle_(lifecycle){}
  s::Result scheduling_admission(const s::EngineIdentity& identity,bool& eligible)const override{
    life::Source source;
    if(auto r=lifecycle_.source({identity.source_uid,identity.epoch},source);!r)return r;
    if(source.identity!=identity)return {s::Code::conflict,"scheduled source identity changed"};
    eligible=source.phase==life::SourcePhase::active;
    return {};
  }
  s::Result enrich(ev::CommitBatch& batch,const s::PreparedTransition& transition)override{
    return lifecycle_.stage_commit(batch,transition);
  }
  s::Result restore_admission(const s::EngineIdentity& identity)const override{
    life::Snapshot snapshot;
    if(auto r=lifecycle_.snapshot(snapshot);!r)return r;
    if(snapshot.quarantined)return {s::Code::unauthorized,"restored store is quarantined"};
    life::Source source;
    if(auto r=lifecycle_.source({identity.source_uid,identity.epoch},source);!r)return r;
    if(source.identity!=identity)return {s::Code::conflict,"source identity differs from engine"};
    if(source.phase==life::SourcePhase::retired)return {s::Code::expired,"source is retired"};
    return {};
  }
  s::Result recovery_sources(std::vector<s::EngineIdentity>& output)const override{
    life::Snapshot snapshot;
    if(auto r=lifecycle_.snapshot(snapshot);!r)return r;
    if(snapshot.quarantined)return {s::Code::unauthorized,"restored store is quarantined"};
    output.clear();
    for(const auto& source:snapshot.sources)
      if(source.phase!=life::SourcePhase::retired&&!source.state_sha256.empty())output.push_back(source.identity);
    return {};
  }
};
struct Application {
  s::EngineModuleCatalog catalog;
  std::unique_ptr<ev::ICommitStore> store;
  std::unique_ptr<life::IArchiveStore> archive;
  std::unique_ptr<life::Service> lifecycle;
  s::ProjectionService projection;
  std::unique_ptr<LifecyclePolicy> policy;
  std::unique_ptr<s::SessionService> runtime;
  std::unique_ptr<s::SessionRunner> runner;
  s::Result open(std::string_view directory,bool create,std::string_view archive_path={},
                  std::uint64_t maximum_bytes=0){
    if(auto r=e::register_builtin_engines(catalog);!r)return r;
    if(auto r=catalog.freeze();!r)return r;
    const fs::path directory_path(directory);
    if(!create&&!canonical_root_exists(directory_path))return {s::Code::not_found,"canonical store ROOT does not exist"};
    if(create&&!canonical_root_exists(directory_path)){
      std::error_code error;
      if(fs::exists(directory_path,error)&&(!fs::is_directory(directory_path,error)||!fs::is_empty(directory_path,error)))
        return {s::Code::conflict,"initialization requires an empty directory or an existing canonical store"};
      if(error)return {s::Code::io_error,"cannot inspect initialization directory"};
    }
    ev::StoreOptions options;
    if(maximum_bytes)options.maximum_bytes=maximum_bytes;
    if(auto r=ev::open_native_store(directory,options,store);!r)return r;
    std::uint64_t persisted=0;bool policy_exists=false;
    const auto store_id=store->info().store_id;
    for(unsigned attempt=0;attempt<4;++attempt){
      if(auto r=stored_budget(*store,persisted,policy_exists);!r)return r;
      if(!policy_exists||options.maximum_bytes==persisted)break;
      if(maximum_bytes)return {s::Code::conflict,"store already has a different byte budget; use the capacity command"};
      if(attempt==3)return {s::Code::busy,"store byte policy changed repeatedly while reopening"};
      store.reset();options.maximum_bytes=persisted;
      if(!canonical_root_exists(directory_path))return {s::Code::conflict,"store root disappeared while reopening its byte policy"};
      if(auto r=ev::open_native_store(directory,options,store);!r)return r;
      if(store->info().store_id!=store_id)return {s::Code::conflict,"store identity changed while reopening its byte policy"};
    }
    if(!policy_exists&&create){
      ev::CommitBatch batch;batch.operation_id="session-storage-policy";
      const auto payload=s::pack({"store-budget/1",std::to_string(options.maximum_bytes)});
      batch.request_digest=s::sha256(payload);batch.guards.push_back({"operator.policy","capacity",0,{}});
      batch.records.push_back({"operator.policy","capacity",payload,{},false});
      ev::CommitReceipt receipt;
      if(auto r=store->commit(batch,receipt);!r)return r;
    }
    if(!create){
      ev::StoredRecord definition;
      const auto found=store->latest("session-definition","main",definition);
      if(found.code==s::Code::not_found)return {s::Code::not_found,"canonical store has no modular session; use audit or explicit init"};
      if(!found)return found;
      if(definition.record.tombstone)return {s::Code::expired,"modular session definition is retired"};
    }
    if(!archive_path.empty())if(auto r=life::open_native_archive(archive_path,{},archive);!r)return r;
    lifecycle=std::make_unique<life::Service>(*store,life::Options{},archive.get());
    if(auto r=lifecycle->open();!r)return r;
    policy=std::make_unique<LifecyclePolicy>(*lifecycle);
    runtime=std::make_unique<s::SessionService>(catalog,*store,projection,policy.get());
    life::Snapshot snapshot;
    if(auto r=lifecycle->snapshot(snapshot);!r)return r;
    if(!snapshot.quarantined)if(auto r=runtime->recover();!r)return r;
    runner=std::make_unique<s::SessionRunner>(*runtime,*store);
    return {};
  }
  s::Result ensure_engines(){
    for(const auto& scheduled:runner->definition().engines){
      const auto& id=scheduled.identity;
      life::Source source;
      auto result=lifecycle->source({id.source_uid,id.epoch},source);
      if(result.code==s::Code::not_found){
        if(auto r=lifecycle->register_engine(id,"local-operator","enroll-"+s::sha256(s::encode_identity(id)));!r)return r;
        if(auto r=lifecycle->source({id.source_uid,id.epoch},source);!r)return r;
      }else if(!result)return result;
      if(source.identity!=id)return {s::Code::conflict,"configured source has another identity"};
      if(source.phase==life::SourcePhase::registered)
        if(auto r=lifecycle->activate(source.id,"activate-"+s::sha256(s::encode_identity(id)));!r)return r;
      if(source.phase==life::SourcePhase::retired||source.phase==life::SourcePhase::sealed||source.phase==life::SourcePhase::draining)continue;
      if(!runtime->find(id.instance_id))if(auto r=runtime->add_engine(id,scheduled.configuration);!r)return r;
    }
    return {};
  }
  s::Result retain_executable(const fs::path& path){
    std::string bytes;
    if(!read_file(path,bytes))return invalid("cannot retain the current executable within its 8 MiB artifact limit");
    const auto digest=s::sha256(bytes);
    const auto module=catalog.describe().front().module_sha256;
    ev::StoredRecord existing;
    const auto found=store->latest("sdk.module-installation",module,existing);
    if(found){
      if(existing.record.payload!=digest)return {s::Code::conflict,"installed executable differs from the retained module package"};
      return {};
    }
    if(found.code!=s::Code::not_found)return found;
    ev::CommitBatch batch;
    batch.operation_id="module-package-"+module;
    batch.request_digest=s::sha256(module+digest);
    batch.guards.push_back({"sdk.module-installation",module,0,{}});
    batch.blobs.push_back({digest,std::move(bytes),{}});
    batch.records.push_back({"sdk.module-installation",module,digest,{digest},false});
    ev::CommitReceipt receipt;
    auto r=store->commit(batch,receipt);
    if(!r.committed())return r;
    life::Pin pin;
    pin.id="module-"+module;
    pin.owner="local-operator";
    pin.kind=life::PinKind::module;
    pin.generation=1;
    pin.objects={digest};
    return lifecycle->put_pin(pin,0,now(),"pin-"+module);
  }
  s::Result run(std::uint64_t through){
    if(auto r=runner->open();!r)return r;
    if(auto r=ensure_engines();!r)return r;
    if(through<runner->progress().completed_cycles)return {s::Code::conflict,"requested cycle is before committed progress"};
    if(through-runner->progress().completed_cycles>10000)return {s::Code::capacity,"one invocation admits at most 10000 cycles"};
    std::mutex mutex;
    std::condition_variable changed;
    std::uint64_t completions=0;
    ev::CommitWorker writer(*store,8);
    writer.set_wakeup([&]{std::lock_guard lock(mutex);++completions;changed.notify_one();});
    runtime->set_commit_queue(&writer);
    auto finish=[&](s::Result result){writer.stop();
    runtime->set_commit_queue(nullptr);
    return result;
    };
    while(runner->progress().completed_cycles<through){
      if(auto r=runner->begin_cycle();!r)return finish(r);
      for(;;){
        std::string instance;
        s::StepRequest request;
        bool pending=false;
        if(auto r=runner->next_step(instance,request,pending);!r)return finish(r);
        if(!pending)break;
        std::uint64_t seen=0;
        {std::lock_guard lock(mutex);
        seen=completions;
        }
        if(auto r=runtime->begin_step(instance,request);!r)return finish(r);
        while(runtime->busy()){
          {std::unique_lock lock(mutex);
          changed.wait(lock,[&]{return completions!=seen;});
          seen=completions;
          }
          if(auto r=runtime->pump();!r&&r.code!=s::Code::busy)return finish(r);
        }
      }
      if(auto r=runner->finish_cycle();!r)return finish(r);
    }
    return finish({});
  }
  s::Result status(){
    const auto info=store->info();
    life::Snapshot snapshot;
    const auto life_result=lifecycle->snapshot(snapshot);
    if(!life_result)return life_result;
    const auto opened=runner->open();
    if(!opened&&opened.code!=s::Code::not_found)return opened;
    const auto progress=opened?runner->progress():s::SessionProgress{};
    std::cout<<"{\"schema\":\"anaiero.session-status/1\",\"evidence_class\":\"SIMULATED_UNCALIBRATED\",\"store_id\":"<<json(info.store_id)
      <<",\"generation\":"<<info.generation<<",\"index_generation\":"<<info.index_generation
      <<",\"index_backend\":"<<json(info.index_backend)<<",\"accounted_bytes\":"<<info.physical_bytes
      <<",\"quota_basis\":\"accounted_serialized_bytes\",\"maintenance_reserve_bytes\":"<<info.maintenance_reserve_bytes
      <<",\"ordinary_limit_bytes\":"<<info.ordinary_limit_bytes
      <<",\"maximum_bytes\":"<<info.maximum_bytes<<",\"completed_cycles\":"<<progress.completed_cycles
      <<",\"cycle_pending\":"<<(progress.cycle_pending?"true":"false")
      <<",\"quarantined\":"<<(snapshot.quarantined?"true":"false")<<",\"incarnation\":"<<json(snapshot.incarnation)
      <<",\"lifetime_observations\":"<<snapshot.lifetime_observations<<",\"active_sources\":"<<snapshot.sources.size()
      <<",\"engines\":[";
    bool first=true;
    for(const auto& engine:runtime->view().engines){
      life::Source source;
      if(auto r=lifecycle->source({engine.identity.source_uid,engine.identity.epoch},source);!r)return r;
      if(!first)std::cout<<',';
      first=false;
      std::cout<<"{\"id\":"<<json(engine.identity.instance_id)<<",\"type\":"<<json(engine.identity.type_id)
        <<",\"source\":"<<json(engine.identity.source_uid)<<",\"epoch\":"<<engine.identity.epoch
        <<",\"source_phase\":"<<json(life::phase_name(source.phase))
        <<",\"phase\":"<<json(s::phase_name(engine.state.phase))<<",\"tick\":"<<engine.state.tick
        <<",\"revision\":"<<engine.state.revision<<",\"state_sha256\":"<<json(s::state_digest(engine.state))<<'}';
    }
    std::cout<<"],\"metrics\":[";
    first=true;
    for(const auto& metric:projection.snapshot()){
      if(!first)std::cout<<',';
      first=false;
      std::cout<<"{\"engine\":"<<json(metric.identity.instance_id)<<",\"metric\":"<<json(metric.observation.metric)
        <<",\"quantity\":"<<json(metric.observation.quantity)<<",\"unit\":"<<json(metric.observation.unit)
        <<",\"value\":"<<metric.observation.value<<",\"tick\":"<<metric.observation.tick<<'}';
    }
    std::cout<<"],\"lifecycle_status\":"<<json(s::code_name(life_result.code))<<"}\n";
    return {};
  }
};
s::Result detach_identity(Application& app,const s::EngineIdentity& identity){
  const auto* current=app.runtime->find(identity.instance_id);
  if(current&&current->identity()!=identity)return {};
  ev::StoredRecord stored;
  const auto found=app.store->latest("sdk.engine",identity.instance_id,stored);
  if(found.code==s::Code::not_found)return {};
  if(!found)return found;
  std::vector<std::string> fields;s::EngineIdentity recorded;
  if(!s::unpack(stored.record.payload,fields,4)||fields.size()!=4||!s::decode_identity(fields[1],recorded))
    return {s::Code::corrupt,"retired engine cleanup identity is invalid"};
  if(recorded!=identity)return {};
  return app.runtime->remove_retired(identity.instance_id);
}
s::Result durable_change(Application& app,std::string_view instance,std::string_view operation,const fs::path& file){
  if(auto r=app.runner->open();!r)return r;
  if(app.runner->progress().cycle_pending)return {s::Code::busy,"finish the pending controller cycle before changing an engine"};
  auto* engine=app.runtime->find(instance);
  if(!engine)return {s::Code::not_found,"active engine not found"};
  const auto* descriptor=app.catalog.find(engine->identity().type_id);
  if(!descriptor||descriptor->change_schemas.size()!=1)return {s::Code::unsupported,"engine has no operator change schema"};
  std::string bytes;
  if(!read_file(file,bytes,4U*1024U*1024U))return invalid("cannot read bounded change file");
  s::StepRequest request{std::string(operation),engine->state().revision,{descriptor->change_schemas.front(),bytes}};
  if(!s::valid_token(operation))return invalid("invalid operation ID");
  ev::StoredRecord existing;
  const auto found=app.store->latest("sdk.operator-intent",operation,existing);
  if(found){
    std::vector<std::string> fields;
    if(!s::unpack(existing.record.payload,fields,4)||fields.size()!=4||fields[0]!=s::encode_identity(engine->identity())||
      fields[2]!=s::encode_envelope(request.input)||fields[3]!="change"||!number(fields[1],request.expected_revision))
      return {s::Code::conflict,"operation ID was already bound to a different engine or change"};
  }else{
    if(found.code!=s::Code::not_found)return found;
    const auto payload=s::pack({s::encode_identity(engine->identity()),std::to_string(request.expected_revision),s::encode_envelope(request.input),"change"});
    ev::CommitBatch intent;
    intent.operation_id="intent-"+s::sha256(operation);
    intent.request_digest=s::sha256(payload);
    intent.guards.push_back({"sdk.operator-intent",std::string(operation),0,{}});
    intent.records.push_back({"sdk.operator-intent",std::string(operation),payload,{},false});
    ev::CommitReceipt receipt;
    if(auto r=app.store->commit(intent,receipt);!r.committed())return r;
  }
  return app.runtime->step(instance,request,true);
}
class MaximumMetric final:public s::IScoreStrategy {
  s::ScoreStrategyIdentity identity_;
public:
  MaximumMetric(std::string metric,std::string implementation):identity_{"anaiero.maximum-metric/1",std::move(implementation),
    {"anaiero.metric-score/1",std::move(metric)}}{}
  s::ScoreStrategyIdentity identity()const override{return identity_;}
  s::Result score(const std::vector<s::MetricComparison>& metrics,s::Score& output)const override{
    for(const auto& metric:metrics)if(metric.candidate.metric==identity_.configuration.payload){
      output={metric.candidate.value,true,"Maximum final "+metric.candidate.metric+" in "+metric.candidate.unit};
      return {};
    }
    return {s::Code::unsupported,"requested metric is absent from this engine"};
  }
};
void selection_json(const s::RecordedExperiment& value){
  std::cout<<"{\"experiment\":"<<json(value.specification.id)<<",\"completed\":"<<(value.completed?"true":"false")
    <<",\"source\":"<<json(value.source.source_uid)<<",\"source_revision\":"<<value.source_state.revision
    <<",\"source_state_sha256\":"<<json(s::state_digest(value.source_state))
    <<",\"chosen_candidate\":"<<json(value.result.chosen_candidate)<<",\"program_sha256\":"<<json(value.result.program_sha256)
    <<",\"baseline_score\":"<<value.result.baseline_score.value<<",\"candidates\":[";
  bool first=true;
  for(const auto& candidate:value.result.candidates){
    if(!first)std::cout<<',';
    first=false;
    std::cout<<"{\"id\":"<<json(candidate.id)<<",\"status\":"<<json(s::code_name(candidate.status.code))
      <<",\"score\":"<<candidate.score.value<<",\"eligible\":"<<(candidate.status&&candidate.score.eligible?"true":"false")<<'}';
  }
  std::cout<<"]}\n";
}
s::Result admin(Application& app,const fs::path& file){
  std::string text;
  if(!read_file(file,text,16384))return invalid("cannot read administration command");
  std::istringstream in(text);
  std::string family,verb,operation,extra;
  in>>family>>verb;
  auto end=[&]{return !(in>>extra);
  };
  if(family=="source"){
    if(verb!="inspect"){
      const auto opened=app.runner->open();
      if(!opened&&opened.code!=s::Code::not_found)return opened;
      if(opened&&app.runner->progress().cycle_pending)return {s::Code::busy,"finish the pending cycle before changing a source lifecycle"};
    }
    life::SourceRef ref;
    if(!(in>>ref.uid>>ref.epoch))return invalid("source requires UID and epoch");
    if(verb=="drain"){
      if(!(in>>operation)||!end())return invalid("source drain syntax");
      return app.lifecycle->begin_drain(ref,operation);
    }
    if(verb=="seal"){
      life::Cursor tip;
      if(!(in>>tip.sequence>>tip.payload_sha256>>operation)||!end())return invalid("source seal syntax");
      if(tip.payload_sha256=="-")tip.payload_sha256.clear();
      return app.lifecycle->seal(ref,tip,operation);
    }
    if(verb=="retire"){
      if(!(in>>operation)||!end())return invalid("source retire syntax");
      life::Source source;
      if(auto r=app.lifecycle->source(ref,source);!r)return r;
      if(auto r=app.lifecycle->retire(ref,operation);!r)return r;
      return detach_identity(app,source.identity);
    }
    if(verb=="inspect"){
      if(!end())return invalid("source inspect syntax");
      life::Source source;
      if(auto r=app.lifecycle->source(ref,source);!r)return r;
      std::cout<<"{\"source\":"<<json(ref.uid)<<",\"epoch\":"<<ref.epoch<<",\"phase\":"<<json(life::phase_name(source.phase))
        <<",\"sequence\":"<<source.last_sequence<<",\"payload_sha256\":"<<json(source.last_payload_sha256)<<"}\n";
        return {};
    }
  }else if(family=="peer"&&verb=="expire"){
    if(!(in>>operation)||!end())return invalid("peer expire syntax");
    return app.lifecycle->expire_peer_leases(now(),operation);
  }else if(family=="peer"&&verb=="upsert"){
    life::Peer peer;
    std::uint64_t expected=0;
    std::string phase;
    if(!(in>>peer.principal>>expected>>peer.generation>>peer.certificate_sha256>>peer.expires_unix_s>>phase>>operation)||!end())return invalid("peer upsert syntax");
    if(phase=="active")peer.phase=life::PeerPhase::active;
    else if(phase=="suspended")peer.phase=life::PeerPhase::suspended;
    else if(phase=="revoked")peer.phase=life::PeerPhase::revoked;
    else return invalid("unknown peer phase");
    return app.lifecycle->upsert_peer(peer,expected,now(),operation);
  }else if(family=="subscription"){
    if(verb=="open"){
      life::Subscription value;
      if(!(in>>value.id>>value.principal>>value.scope.uid>>value.scope.epoch>>value.generation>>value.lease_until_unix_s>>operation)||!end())return invalid("subscription open syntax");
      life::Subscription previous;
      const auto found=app.lifecycle->subscription(value.id,previous);
      if(found)value.acknowledged=previous.acknowledged;
      else if(found.code!=s::Code::not_found)return found;
      return app.lifecycle->open_subscription(value,now(),operation);
    }
    if(verb=="offer"||verb=="ack"){
      std::string id;
      std::uint64_t generation=0;
      life::Cursor cursor;
      if(!(in>>id>>generation>>cursor.sequence>>cursor.payload_sha256>>operation)||!end())return invalid("subscription cursor syntax");
      if(cursor.payload_sha256=="-")cursor.payload_sha256.clear();
      return verb=="offer"?app.lifecycle->offer(id,generation,cursor,now(),operation):app.lifecycle->acknowledge(id,generation,cursor,now(),operation);
    }
    if(verb=="close"){
      std::string id;
      std::uint64_t generation=0;
      if(!(in>>id>>generation>>operation)||!end())return invalid("subscription close syntax");
      return app.lifecycle->close_subscription(id,generation,operation);
    }
    if(verb=="expire"){
      if(!(in>>operation)||!end())return invalid("subscription expire syntax");
      return app.lifecycle->expire_subscriptions(now(),operation);
    }
  }else if(family=="bootstrap"&&verb=="accept"){
    life::Bootstrap value;
    std::string snapshot_file;
    if(!(in>>value.subscription_id>>value.principal>>value.scope.uid>>value.scope.epoch>>value.generation
       >>value.lease_until_unix_s>>value.tip.sequence>>value.tip.payload_sha256>>std::quoted(snapshot_file)>>operation)||!end())
      return invalid("bootstrap accept syntax");
    if(value.tip.payload_sha256=="-")value.tip.payload_sha256.clear();
    if(!read_file(snapshot_file,value.snapshot))return invalid("cannot read bounded bootstrap snapshot");
    value.snapshot_sha256=s::sha256(value.snapshot);
    return app.lifecycle->accept_bootstrap(value,now(),operation);
  }else if(family=="pin"){
    if(verb=="put"){
      life::Pin pin;
      std::uint64_t expected=0;
      unsigned kind=0;
      std::size_t count=0;
      if(!(in>>pin.id>>pin.owner>>expected>>pin.generation>>kind>>pin.expires_unix_s>>count)||kind>6||count>64)return invalid("pin put syntax");
      pin.kind=static_cast<life::PinKind>(kind);
      for(std::size_t i=0;i<count;++i){std::string digest;
      if(!(in>>digest))return invalid("missing pin digest");
      pin.objects.push_back(std::move(digest));
      }
      if(!(in>>operation)||!end())return invalid("pin operation missing");
      return app.lifecycle->put_pin(pin,expected,now(),operation);
    }
    if(verb=="release"){
      std::string id;
      std::uint64_t generation=0;
      if(!(in>>id>>generation>>operation)||!end())return invalid("pin release syntax");
      return app.lifecycle->release_pin(id,generation,operation);
    }
  }else if(family=="maintenance"){
    if(verb=="review"){
      life::MaintenanceRequest request;
      std::string disposition;
      std::size_t count=0;
      if(!(in>>request.id>>request.source.uid>>request.source.epoch>>request.expire_through>>disposition>>std::quoted(request.archive_directory)>>count)||count>64)
        return invalid("maintenance review syntax");
      if(disposition=="expire")request.disposition=life::HistoryAvailability::expired;
      else if(disposition=="archive")request.disposition=life::HistoryAvailability::archived;
      else return invalid("maintenance requires archive or explicit expiry");
      if(request.disposition==life::HistoryAvailability::archived){
        if(!app.archive)return invalid("archive maintenance requires the configured archive directory argument");
        const auto archive_native_path=[](std::string_view encoded_path){
          return fs::path(std::u8string(reinterpret_cast<const char8_t*>(encoded_path.data()),encoded_path.size()));
        };
        std::error_code path_error;
        if(!fs::equivalent(archive_native_path(request.archive_directory),archive_native_path(app.archive->identifier()),path_error)||path_error)
          return invalid("maintenance archive differs from the configured archive directory");
        request.archive_directory=app.archive->identifier();
      }
      for(std::size_t i=0;i<count;++i){std::string digest;
      if(!(in>>digest))return invalid("missing maintenance object");
      request.objects.push_back(std::move(digest));
      }
      if(!(in>>operation)||!end())return invalid("maintenance operation missing");
      request.now_unix_s=now();
      life::MaintenancePlan plan;
      auto result=app.lifecycle->review_maintenance(request,operation,plan);
      if(result)std::cout<<"{\"maintenance\":"<<json(plan.request.id)<<",\"expected_bytes\":"<<plan.expected_bytes<<",\"new_floor\":"<<plan.new_floor<<"}\n";
      return result;
    }
    if(verb=="activate"||verb=="execute"){
      std::string id;
      if(!(in>>id>>operation)||!end())return invalid("maintenance action syntax");
      return verb=="activate"?app.lifecycle->activate_maintenance(id,now(),operation):app.lifecycle->execute_maintenance(id,operation);
    }
  }else if(family=="history"&&verb=="status"){
    life::SourceRef source;
    std::uint64_t after=0;
    if(!(in>>source.uid>>source.epoch>>after)||!end())return invalid("history status syntax");
    return app.lifecycle->replay_status(source,after);
  }else if(family=="restore"&&verb=="reauthorize"){
    std::string policy_file;
    if(!(in>>std::quoted(policy_file)>>operation)||!end())return invalid("restore reauthorize syntax");
    std::string declaration;
    if(!read_file(fs::path(policy_file),declaration,16384)||declaration.empty())return invalid("a current operator policy declaration is required");
    if(auto r=app.lifecycle->reauthorize_restore(s::sha256(declaration),operation);!r)return r;
    std::vector<s::EngineIdentity> cleanup;
    if(auto r=app.lifecycle->reauthorization_sources(operation,cleanup);!r)return r;
    for(const auto& identity:cleanup){
      const auto detached=detach_identity(app,identity);
      if(!detached&&detached.code!=s::Code::not_found)return detached;
    }
    return {};
  }
  return invalid("unknown administration command; see the modular session guide");
}
void help(){
  std::cout<<"Anaiero modular sessions\n"
    "  catalog\n  init CONFIG STORE [MAXIMUM_BYTES]\n  configure CONFIG STORE OPERATION\n  run STORE THROUGH_CYCLE\n  status STORE\n"
    "  change STORE INSTANCE OPERATION CHANGE_FILE\n  pause|resume|cancel STORE INSTANCE OPERATION\n"
    "  trial STORE INSTANCE TICKS [CHANGE_FILE]\n  export-plan STORE INSTANCE IMAGE RECEIPTS\n"
    "  select STORE INSTANCE EXPERIMENT_ID TICKS METRIC CHANGE_FILE [CHANGE_FILE...]\n"
    "  selection|resume-experiment STORE EXPERIMENT_ID\n  apply-selected|outcome|release-experiment STORE EXPERIMENT_ID OPERATION\n"
    "  audit|repair-index STORE [WORKING_BUDGET_BYTES]\n  capacity STORE MAXIMUM_BYTES OPERATION\n  admin STORE COMMAND_FILE [ARCHIVE_DIRECTORY]\n"
    "  migrate-evidence LEGACY_STORE NEW_STORE\n  restore SOURCE_STORE NEW_STORE OPERATION\n"
    "  module-packages STORE [AFTER_MODULE]\n  export-artifact STORE SHA256 OUTPUT\n"
    "  export-state STORE INSTANCE FILE\n"
    "Run uses an absolute committed cycle target. Restart and repeat the same target to finish interrupted work.\n"
    "Changes and controls require stable caller operation IDs. Plan and model artifacts are retained at init.\n";
}
s::Result execute(int argc,char** argv){
  if(argc<2||std::string_view(argv[1])=="help"){help();
  return {};
  }
  const std::string command=argv[1];
  if(command=="catalog"){
    s::EngineModuleCatalog catalog;
    if(auto r=e::register_builtin_engines(catalog);!r)return r;
    for(const auto& d:catalog.describe())std::cout<<"{\"type\":"<<json(d.type_id)<<",\"name\":"<<json(d.display_name)
      <<",\"module_sha256\":"<<json(d.module_sha256)<<",\"finite\":"<<(d.finite?"true":"false")
      <<",\"isolated_trials\":"<<(d.isolated_trials?"true":"false")<<",\"changes\":"<<(d.change_preparation?"true":"false")<<"}\n";
    return {};
  }
  if(argc<3)return invalid("store path is required");
  if((command=="module-packages"&&(argc==3||argc==4))||(command=="export-artifact"&&argc==5)){
    if(!canonical_root_exists(argv[2]))return {s::Code::not_found,"canonical store ROOT does not exist"};
    std::unique_ptr<ev::ICommitStore> store;
    if(auto r=ev::open_native_store(argv[2],{},store);!r)return r;
    if(command=="export-artifact"){
      if(!s::valid_digest(argv[3]))return invalid("artifact SHA-256 is invalid");
      std::string bytes;if(auto r=store->get_blob(argv[3],bytes);!r)return r;
      return e::write_new_artifact(argv[4],bytes);
    }
    std::vector<ev::StoredRecord> records;bool more=false;
    if(auto r=store->scan_latest("sdk.module-installation",{},argc==4?argv[3]:"",32,records,more);!r)return r;
    std::cout<<"{\"packages\":[";bool first=true;
    for(const auto& record:records){
      if(record.record.tombstone)continue;
      if(!s::valid_digest(record.record.key)||!s::valid_digest(record.record.payload))return {s::Code::corrupt,"retained module package identity is invalid"};
      if(!first)std::cout<<',';
      first=false;
      std::cout<<"{\"module_sha256\":"<<json(record.record.key)<<",\"artifact_sha256\":"<<json(record.record.payload)<<'}';
    }
    std::cout<<"],\"more\":"<<(more?"true":"false")<<",\"after\":"<<json(records.empty()?"":records.back().record.key)<<"}\n";
    return {};
  }
  if(command=="capacity"&&argc==5){
    std::uint64_t maximum=0;
    if(!number(argv[3],maximum)||maximum<65536||!s::valid_token(argv[4]))return invalid("capacity requires a byte budget and stable operation ID");
    if(!canonical_root_exists(argv[2]))return {s::Code::not_found,"canonical store ROOT does not exist"};
    ev::StoreOptions options;options.maximum_bytes=maximum;
    std::unique_ptr<ev::ICommitStore> store;
    if(auto r=ev::open_native_store(argv[2],options,store);!r)return r;
    ev::StoredRecord prior;
    const auto found=store->latest("operator.policy","capacity",prior);
    if(!found&&found.code!=s::Code::not_found)return found;
    const auto payload=s::pack({"store-budget/1",std::to_string(maximum)});
    ev::CommitBatch batch;batch.operation_id=argv[4];batch.request_digest=s::sha256(payload);
    batch.commit_class=ev::CommitClass::maintenance;
    batch.guards.push_back({"operator.policy","capacity",prior.revision,prior.digest});
    batch.records.push_back({"operator.policy","capacity",payload,{},false});
    ev::CommitReceipt receipt;if(auto r=store->commit(batch,receipt);!r)return r;
    std::cout<<"{\"capacity\":\"committed\",\"maximum_bytes\":"<<maximum<<",\"generation\":"<<receipt.generation<<"}\n";
    return {};
  }
  if((command=="audit"||command=="repair-index")&&(argc==3||argc==4)){
    if(!canonical_root_exists(argv[2]))return {s::Code::not_found,"canonical store ROOT does not exist"};
    ev::StoreOptions options;
    if(argc==4&&(!number(argv[3],options.maximum_bytes)||options.maximum_bytes<65536))return invalid("invalid inspection working budget");
    std::unique_ptr<ev::ICommitStore> store;
    if(auto r=ev::open_native_store(argv[2],options,store);!r)return r;
    if(command=="repair-index")if(auto r=store->repair_index();!r)return r;
    ev::AuditReport report;
    if(auto r=store->audit(report);!r)return r;
    std::cout<<"{\"audit\":\"verified\",\"generations\":"<<report.generations<<",\"records\":"<<report.records
      <<",\"blobs\":"<<report.blobs<<",\"expired_blobs\":"<<report.expired_blobs
      <<",\"canonical_records_read\":"<<report.canonical_records_read<<",\"index_nodes_read\":"<<report.index_nodes_read<<"}\n";
    return {};
  }
  if(command=="migrate-evidence"){
    if(argc!=4)return invalid("migrate-evidence requires legacy source and new destination");
    ev::MigrationReport report;
    if(auto r=ev::migrate_legacy_evidence(argv[2],argv[3],{},report);!r)return r;
    std::cout<<"{\"migration\":\"evidence-only\",\"source_fingerprint\":"<<json(report.source_fingerprint)
      <<",\"destination_root\":"<<json(report.destination_root)<<",\"objects\":"<<report.objects<<",\"events\":"<<report.events
      <<",\"source_unchanged\":"<<(report.source_unchanged?"true":"false")<<",\"activated\":"<<(report.activated?"true":"false")
      <<",\"backup_directory\":"<<json(report.backup_directory)<<",\"retained_backup_bytes\":"<<report.retained_backup_bytes<<"}\n";
    return {};
  }
  if(command=="restore"){
    if(argc!=5)return invalid("restore requires source, new destination and stable operation ID");
    life::RestoreReport report;
    if(auto r=life::restore_quarantined_store(argv[2],argv[3],argv[4],report);!r)return r;
    std::cout<<"{\"restore\":\"quarantined\",\"source_generation\":"<<report.source_generation
      <<",\"source_root\":"<<json(report.source_root)<<",\"incarnation\":"<<json(report.incarnation)
      <<",\"copied_files\":"<<report.copied_files<<",\"copied_bytes\":"<<report.copied_bytes
      <<",\"published\":"<<(report.published?"true":"false")<<"}\n";
    return {};
  }
  Application app;
  if(command=="configure"){
    if(argc!=5)return invalid("configure requires configuration, store and operation ID");
    if(auto r=app.open(argv[3],false);!r)return r;
    s::SessionDefinition definition;
    if(auto r=e::read_session_definition(argv[2],app.catalog,definition);!r)return r;
    for(const auto& entry:definition.engines)if(!app.runtime->find(entry.identity.instance_id)){
      std::unique_ptr<s::IEngine> engine;
      if(auto r=app.catalog.create(entry.identity,entry.configuration,nullptr,engine);!r)return r;
    }
    if(auto r=app.retain_executable(executable(argv[0]));!r)return r;
    if(auto r=app.runner->reconfigure(definition,argv[4]);!r)return r;
    if(auto r=app.ensure_engines();!r)return r;
    return app.status();
  }
  if(command=="init"){
    if(argc<4||argc>5)return invalid("init requires configuration and store");
    std::uint64_t maximum=0;
    if(argc==5&&(!number(argv[4],maximum)||maximum<65536))return invalid("invalid store byte quota");
    if(auto r=app.open(argv[3],true,{},maximum);!r)return r;
    s::SessionDefinition definition;
    if(auto r=e::read_session_definition(argv[2],app.catalog,definition);!r)return r;
    // Validate every module before writing an initialized session intent.
    for(const auto& entry:definition.engines){std::unique_ptr<s::IEngine> engine;
      if(auto r=app.catalog.create(entry.identity,entry.configuration,nullptr,engine);!r)return r;
      }
    if(auto r=app.runner->initialize(definition);!r)return r;
    if(auto r=app.retain_executable(executable(argv[0]));!r)return r;
    if(auto r=app.ensure_engines();!r)return r;
    return app.status();
  }
  const std::string_view archive_path=command=="admin"&&argc==5?argv[4]:"";
  if(auto r=app.open(argv[2],false,archive_path);!r)return r;
  if(command=="status"&&argc==3){return app.status();
  }
  if(command=="run"&&argc==4){std::uint64_t through=0;
  if(!number(argv[3],through))return invalid("invalid absolute cycle target");
    if(auto r=app.run(through);!r)return r;
    return app.status();
    }
  if(command=="admin"&&(argc==4||argc==5))return admin(app,argv[3]);
  if(command=="change"&&argc==6)return durable_change(app,argv[3],argv[4],argv[5]);
  if(command=="select"&&argc>=8&&argc<=15){
    if(auto r=app.runner->open();!r)return r;
    if(app.runner->progress().cycle_pending)return {s::Code::busy,"finish the pending cycle before freezing an experiment"};
    auto* engine=app.runtime->find(argv[3]);
    if(!engine)return {s::Code::not_found,"engine not found"};
    const auto* descriptor=app.catalog.find(engine->identity().type_id);
    if(!descriptor||!descriptor->isolated_trials||descriptor->change_schemas.size()!=1)
      return {s::Code::unsupported,"engine has no registered isolated change trials"};
    std::uint64_t ticks=0;
    if(!number(argv[5],ticks)||ticks==0||ticks>128||!s::valid_token(argv[6]))return invalid("selection requires 1..128 ticks and a metric name");
    const auto& schedules=app.runner->definition().engines;
    const auto scheduled=std::find_if(schedules.begin(),schedules.end(),[&](const auto& item){return item.identity==engine->identity();});
    if(scheduled==schedules.end())return {s::Code::not_found,"engine input schedule not found"};
    s::ExperimentSpecification specification;
    specification.id=argv[4];specification.inputs.assign(static_cast<std::size_t>(ticks),scheduled->input);
    for(int i=7;i<argc;++i){
      std::string bytes;if(!read_file(argv[i],bytes,4U*1024U*1024U))return invalid("cannot read bounded candidate change");
      specification.candidates.push_back({"candidate-"+std::to_string(i-6),{descriptor->change_schemas.front(),std::move(bytes)}});
    }
    MaximumMetric strategy(argv[6],descriptor->module_sha256);
    s::RecordedExperiment result;
    if(auto r=s::ExperimentHistoryService(app.catalog,*app.store).select(*engine,specification,strategy,result);!r)return r;
    selection_json(result);return {};
  }
  if(command=="selection"&&argc==4){
    s::RecordedExperiment result;
    if(auto r=s::ExperimentHistoryService(app.catalog,*app.store).load(argv[3],result);!r)return r;
    selection_json(result);return {};
  }
  if(command=="resume-experiment"&&argc==4){
    s::ExperimentHistoryService history(app.catalog,*app.store);
    s::RecordedExperiment frozen;
    if(auto r=history.load(argv[3],frozen);!r)return r;
    const auto* descriptor=app.catalog.find(frozen.source.type_id);
    if(!descriptor||descriptor->module_sha256!=frozen.strategy.implementation_sha256||
       frozen.strategy.id!="anaiero.maximum-metric/1"||frozen.strategy.configuration.schema!="anaiero.metric-score/1")
      return {s::Code::conflict,"retained experiment needs its pinned scorer implementation"};
    std::unique_ptr<s::IEngine> source;
    if(auto r=app.catalog.create(frozen.source,frozen.source_state.configuration,&frozen.source_state,source);!r)return r;
    MaximumMetric strategy(frozen.strategy.configuration.payload,descriptor->module_sha256);
    s::RecordedExperiment result;
    if(auto r=history.select(*source,frozen.specification,strategy,result);!r)return r;
    selection_json(result);return {};
  }
  if((command=="apply-selected"||command=="outcome"||command=="release-experiment")&&argc==5){
    s::ExperimentHistoryService history(app.catalog,*app.store);
    if(command=="release-experiment")return history.release(argv[3],argv[4]);
    if(command=="apply-selected"){
      if(auto r=app.runner->open();!r)return r;
      if(app.runner->progress().cycle_pending)return {s::Code::busy,"finish the pending cycle before applying a selection"};
      const auto result=history.apply_selected(argv[3],argv[4],*app.runtime);
      if(result.code==s::Code::completed){std::cout<<"{\"application\":\"baseline-preserved\"}\n";return {};}
      return result;
    }
    s::RecordedExperiment frozen;
    if(auto r=history.load(argv[3],frozen);!r)return r;
    if(frozen.strategy.id!="anaiero.maximum-metric/1"||frozen.strategy.configuration.schema!="anaiero.metric-score/1")
      return {s::Code::unsupported,"operator scorer does not implement the frozen strategy"};
    const auto* descriptor=app.catalog.find(frozen.source.type_id);
    if(!descriptor||descriptor->module_sha256!=frozen.strategy.implementation_sha256)
      return {s::Code::conflict,"frozen scorer implementation does not match this executable"};
    MaximumMetric strategy(frozen.strategy.configuration.payload,descriptor->module_sha256);
    s::OutcomeReview result;
    if(auto r=history.review(argv[3],argv[4],strategy,result);!r)return r;
    std::cout<<"{\"outcome\":\"committed\",\"measured_score\":"<<result.measured_score.value
      <<",\"meets_selected_score\":"<<(result.meets_selected_score?"true":"false")
      <<",\"exact_selected_state\":"<<(result.exact_selected_state?"true":"false")
      <<",\"observed_receipt_tip\":"<<json(result.observed_receipt_tip)<<"}\n";
    return {};
  }
  if((command=="pause"||command=="resume"||command=="cancel")&&argc==5){
    if(auto r=app.runner->open();!r)return r;
    if(app.runner->progress().cycle_pending)return {s::Code::busy,"finish the pending cycle before lifecycle control"};
    return app.runtime->control(argv[3],argv[4],command=="pause"?s::Phase::paused:command=="resume"?s::Phase::running:s::Phase::cancelled);
  }
  if(command=="export-plan"&&argc==6){
    auto* engine=app.runtime->find(argv[3]);
    if(!engine)return {s::Code::not_found,"engine not found"};
    std::string image,receipts;
    if(auto r=e::plan_output(engine->state(),image,receipts);!r)return r;
    if(auto r=e::write_new_artifact(argv[4],image);!r)return r;
    if(auto r=e::write_new_artifact(argv[5],receipts);!r)return r;
    return {};
  }
  if(command=="export-state"&&argc==5){
    auto* engine=app.runtime->find(argv[3]);
    if(!engine)return {s::Code::not_found,"engine not found"};
    if(auto r=e::write_new_artifact(argv[4],s::encode_state(engine->state()));!r)return r;
    return {};
  }
  if(command=="trial"&&(argc==5||argc==6)){
    if(auto r=app.runner->open();!r)return r;
    auto* engine=app.runtime->find(argv[3]);
    if(!engine)return {s::Code::not_found,"engine not found"};
    std::uint64_t ticks=0;
    if(!number(argv[4],ticks)||ticks>128)return invalid("trial horizon must be 0..128");
    const s::EngineSchedule* scheduled=nullptr;
    for(const auto& value:app.runner->definition().engines)if(value.identity.instance_id==argv[3])scheduled=&value;
    if(!scheduled)return {s::Code::not_found,"scheduled input program not found"};
    s::TrialProgram program;
    program.id="operator-trial";
    program.inputs.assign(static_cast<std::size_t>(ticks),scheduled->input);
    if(argc==6){const auto* d=app.catalog.find(engine->identity().type_id);
    std::string bytes;
      if(!d||d->change_schemas.size()!=1||!read_file(argv[5],bytes,4U*1024U*1024U))return invalid("trial change file or capability is invalid");
      program.has_change=true;
      program.change={d->change_schemas.front(),std::move(bytes)};
      }
    s::TrialResult result;
    if(auto r=s::ExperimentService{}.evaluate(*engine,program,result);!r)return r;
    std::cout<<"{\"trial\":\"isolated\",\"source_state_sha256\":"<<json(s::state_digest(engine->state()))
      <<",\"final_state_sha256\":"<<json(s::state_digest(result.final_state))<<",\"tick\":"<<result.final_state.tick
      <<",\"receipt_tip\":"<<json(result.receipt_tip)<<"}\n";
      return {};
  }
  return invalid("unknown command or argument count; use help");
}
}
int main(int argc,char** argv){
  try {const auto result=execute(argc,argv);
  if(!result){std::cerr<<"{\"status\":"<<json(s::code_name(result.code))<<",\"message\":"<<json(result.message)<<"}\n";
      return result.code==s::Code::committed_degraded?3:2;
      }return 0;
  }catch(const std::exception& error){std::cerr<<"{\"status\":\"io_error\",\"message\":"<<json(error.what())<<"}\n";
  return 2;
  }
}
