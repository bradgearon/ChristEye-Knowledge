#include "counter_engine.hpp"
#include <anaiero/sdk/catalog.hpp>
#include <anaiero/sdk/evidence.hpp>
#include <anaiero/sdk/experiments.hpp>
#include <anaiero/sdk/module_host.hpp>
#include <anaiero/sdk/runtime.hpp>
#include <anaiero/sdk/observer.hpp>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>

using namespace anaiero::sdk;
namespace {
#define CHECK(condition) do {if(!(condition)){std::cerr<<"check failed line "<<__LINE__<<": " #condition "\n";return false;}}while(false)
bool success(const Result& result) {
  if(result.ok())return true;
  std::cerr<<code_name(result.code)<<": "<<result.message<<'\n';return false;
}
Digest file_digest(const std::string& path) {
  std::ifstream input(path,std::ios::binary);Bytes bytes((std::istreambuf_iterator<char>(input)),{});return sha256(bytes);
}
StepRequest step(std::string id,const IEngine& engine,std::string value="1") {
  return {std::move(id),engine.state().revision,{"counter.add/1",std::move(value)}};
}
struct HighestCount final:IScoreStrategy {
  ScoreStrategyIdentity identity()const override {
    return {"independent.highest-count/1",sha256("highest-count-implementation/1"),{"score.metric/1","count"}};
  }
  Result score(const std::vector<MetricComparison>& metrics,Score& output)const override {
    if(metrics.size()!=1||metrics[0].candidate.metric!="count")return {Code::unsupported,"count metric required"};
    output={metrics[0].candidate.value,true,"larger count"};return {};
  }
};
struct FakeClock final:IObserverClock {mutable std::uint64_t value=0;std::uint64_t now_ns()const noexcept override{return value;}};
struct ThrowingObserver final:IObserver {Result observe(const ObserverEvent&)override{throw 1;}};
struct SlowObserver final:IObserver {
  FakeClock& clock;explicit SlowObserver(FakeClock& value):clock(value){}
  Result observe(const ObserverEvent&)override{clock.value+=1000;return {};}
};
bool observers_and_selection() {
  const auto descriptor=independent::descriptor();const auto config=independent::configuration();
  const auto identity=independent::identity(descriptor,config,"selection");
  independent::Factory factory;std::unique_ptr<IEngine> source;CHECK(success(factory.create(identity,config,nullptr,source)));
  const auto original=source->state();FakeClock clock;ObserverService observers(2,2);
  CHECK(success(observers.add("throws",std::make_shared<ThrowingObserver>())));
  CHECK(success(observers.add("slow",std::make_shared<SlowObserver>(clock))));
  HighestCount strategy;ExperimentService experiments;experiments.set_observer_sink(&observers);
  ExperimentRequest request{"selection",original.revision,state_digest(original),{{"counter.add/1","2"}},
    {{"lower",{"counter.set/1","0"}},{"higher",{"counter.set/1","10"}},{"equal-higher",{"counter.set/1","10"}}}};
  independent::fork_calls.store(0);ExperimentResult selected;
  CHECK(success(experiments.select(*source,request,strategy,selected)));
  CHECK(independent::fork_calls.load()==4); // one baseline, three independent candidates
  CHECK(selected.chosen_candidate=="equal-higher"&&source->state()==original);
  CHECK(independent::count(selected.baseline.final_state)==2&&selected.candidates.size()==3);
  CHECK(observers.statistics().accepted==2&&observers.statistics().dropped==2);
  CHECK(observers.statistics().delivered==0); // Never runs callbacks inside experiment ownership.
  CHECK(success(observers.dispatch(8,100,clock)));
  CHECK(observers.statistics().callback_faults==1&&observers.statistics().budget_overruns==1);
  CHECK(observers.statistics().enabled_observers==0&&source->state()==original);
  const auto& chosen=selected.candidates[2].trial;
  OutcomeEvidence evidence{identity,chosen.final_state,chosen.observations,chosen.receipt_tip};OutcomeReview review;
  CHECK(success(experiments.compare_outcome(selected,evidence,strategy,review)));
  CHECK(review.meets_selected_score&&review.exact_selected_state);
  evidence.observations.back().unit="mV";
  CHECK(experiments.compare_outcome(selected,evidence,strategy,review).code==Code::conflict);
  auto stale=request;++stale.expected_revision;CHECK(experiments.select(*source,stale,strategy,selected).code==Code::conflict);
  return true;
}
bool codecs() {
  CHECK(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  const auto bytes=pack({"a",std::string("b\0c",3)});std::vector<std::string> fields;
  CHECK(success(unpack(bytes,fields)));CHECK(fields.size()==2&&fields[1].size()==3);
  CHECK(unpack(bytes+"x",fields).code==Code::corrupt);
  CHECK(unpack(bytes,fields,1).code==Code::corrupt);
  EngineDescriptor descriptor;CHECK(success(decode_descriptor(encode_descriptor(independent::descriptor()),descriptor)));
  CHECK(descriptor.type_id=="independent.counter"&&descriptor.isolated_trials);
  return true;
}
bool runtime(const std::string& directory) {
  std::filesystem::remove_all(directory);
  std::unique_ptr<evidence::ICommitStore> store;
  CHECK(success(evidence::open_native_store(directory,{},store)));
  EngineModuleCatalog catalog;const auto descriptor=independent::descriptor();
  CHECK(success(catalog.add(descriptor,std::make_shared<independent::Factory>())));
  CHECK(catalog.add(descriptor,std::make_shared<independent::Factory>()).code==Code::conflict);
  CHECK(success(catalog.freeze()));CHECK(catalog.add(descriptor,std::make_shared<independent::Factory>()).code==Code::conflict);
  ProjectionService projection(4);SessionService session(catalog,*store,projection);
  const auto configuration=independent::configuration(4,40000);
  const auto identity=independent::identity(descriptor,configuration);
  CHECK(success(session.add_engine(identity,configuration)));auto* engine=session.find("counter");CHECK(engine);
  const auto original=engine->state();
  ExperimentService experiments;TrialResult trial;
  CHECK(success(experiments.evaluate(*engine,{"isolated",{{"counter.add/1","3"}},{"counter.set/1","9"},true},trial)));
  CHECK(independent::count(trial.final_state)==12&&engine->state()==original&&projection.size()==0);
  CHECK(trial.final_state.tick==1&&trial.final_state.revision==2);
  const auto request=step("once",*engine,"2");
  {
    evidence::CommitWorker worker(*store,2);std::mutex mutex;std::condition_variable wake;std::atomic<bool> notified=false;
    worker.set_wakeup([&]{notified.store(true);wake.notify_one();});session.set_commit_queue(&worker);
    CHECK(success(session.begin_step("counter",request)));CHECK(session.busy());
    CHECK(engine->state()==original&&projection.size()==0);
    CHECK(session.begin_step("counter",step("competing",*engine)).code==Code::busy);
    {
      std::unique_lock lock(mutex);CHECK(wake.wait_for(lock,std::chrono::seconds(5),[&]{return notified.load();}));
    }
    CHECK(success(session.pump()));CHECK(!session.busy());CHECK(independent::count(engine->state())==2);
    worker.stop();session.set_commit_queue(nullptr);
  }
  CHECK(success(session.step("counter",request)));CHECK(engine->state().revision==1);
  auto changed=request;changed.input.payload="3";CHECK(session.step("counter",changed).code==Code::conflict);
  CHECK(success(session.control("counter","pause-once",Phase::paused)));const auto paused=engine->state();
  CHECK(success(session.control("counter","pause-once",Phase::paused)));CHECK(engine->state()==paused);
  CHECK(session.step("counter",step("while-paused",*engine)).code==Code::conflict);
  CHECK(success(session.step("counter",{"change-while-paused",engine->state().revision,{"counter.set/1","3"}},true)));
  CHECK(engine->state().phase==Phase::paused&&engine->state().tick==paused.tick&&independent::count(engine->state())==3);
  CHECK(success(session.control("counter","resume-once",Phase::running)));
  StepRequest change{"set-ten",engine->state().revision,{"counter.set/1","10"}};
  CHECK(success(session.step("counter",change,true)));CHECK(independent::count(engine->state())==10&&engine->state().tick==1);
  const auto before_fault=engine->state();
  PreparedTransition outside;CHECK(success(engine->prepare_step(step("outside-owner",*engine),outside)));
  CHECK(success(engine->install(outside.target)));
  CHECK(session.view().engines[0].state==before_fault);
  CHECK(session.step("counter",step("reject-outside-state",*engine)).code==Code::corrupt);
  CHECK(engine->state()==before_fault);
  CHECK(session.step("counter",step("faulty-module",*engine,"mutate")).code==Code::corrupt);
  CHECK(engine->state()==before_fault);
  const auto retry=step("commit-before-projection",*engine);
  evidence::set_native_store_fault(*store,evidence::FaultPoint::after_transaction);
  CHECK(!session.step("counter",retry).committed());CHECK(engine->state()==before_fault);
  evidence::set_native_store_fault(*store,evidence::FaultPoint::none);
  CHECK(success(session.recover()));engine=session.find("counter");CHECK(engine&&engine->state()==before_fault);
  CHECK(success(session.step("counter",retry)));CHECK(engine->state().tick==2);
  const auto recoverable=step("root-before-index",*engine);
  evidence::set_native_store_fault(*store,evidence::FaultPoint::after_root);
  CHECK(session.step("counter",recoverable).code==Code::committed_degraded);
  CHECK(engine->state().tick==3&&session.view().degraded);
  CHECK(session.step("counter",step("blocked-degraded",*engine)).code==Code::busy);
  evidence::set_native_store_fault(*store,evidence::FaultPoint::none);CHECK(success(session.recover()));
  engine=session.find("counter");CHECK(engine&&engine->state().tick==3);
  CHECK(success(session.step("counter",recoverable)));CHECK(engine->state().tick==3);
  CHECK(success(session.step("counter",step("finite-final",*engine))));CHECK(engine->state().phase==Phase::completed);
  CHECK(session.step("counter",step("after-final",*engine)).code==Code::completed);
  CHECK(session.control("counter","restart-completed",Phase::running).code==Code::conflict);
  CHECK(projection.snapshot().size()==1&&projection.snapshot()[0].observation.quantity=="dimensionless_count");
  const auto final=engine->state();ProjectionService restored_projection;SessionService restored(catalog,*store,restored_projection);
  CHECK(success(restored.recover()));CHECK(restored.find("counter")->state()==final);
  CHECK(restored_projection.snapshot()[0].observation==projection.snapshot()[0].observation);
  evidence::AuditReport audit;CHECK(success(store->audit(audit)));return true;
}
struct RetirementPolicy final:ICommitPolicy {
  EngineIdentity identity;bool retired=false;
  SessionService* owner=nullptr;Code reentry=Code::ok;
  Result enrich(evidence::CommitBatch&,const PreparedTransition&)override {
    if(owner)reentry=owner->recover().code;
    return {};
  }
  Result restore_admission(const EngineIdentity&)const override{return retired?Result{Code::expired,"retired"}:Result{};}
  Result recovery_sources(std::vector<EngineIdentity>& output)const override {output.clear();if(!retired)output.push_back(identity);return {};}
};
bool detach(const std::string& directory) {
  std::filesystem::remove_all(directory);std::unique_ptr<evidence::ICommitStore> store;
  CHECK(success(evidence::open_native_store(directory,{},store)));
  EngineModuleCatalog catalog;auto descriptor=independent::descriptor();CHECK(success(catalog.add(descriptor,std::make_shared<independent::Factory>())));
  CHECK(success(catalog.freeze()));RetirementPolicy policy;const auto config=independent::configuration();policy.identity=independent::identity(descriptor,config);
  {
    ProjectionService projection;SessionService session(catalog,*store,projection,&policy);
    policy.owner=&session;
    CHECK(success(session.add_engine(policy.identity,config)));CHECK(success(session.step("counter",step("retire-last",*session.find("counter")))));
    CHECK(policy.reentry==Code::busy);policy.owner=nullptr;
  }
  policy.retired=true;ProjectionService projection;SessionService recovered(catalog,*store,projection,&policy);
  CHECK(success(recovered.recover()));CHECK(recovered.view().engines.empty());
  CHECK(success(recovered.remove_retired("counter")));CHECK(success(recovered.remove_retired("counter")));
  evidence::StoredRecord record;CHECK(success(store->latest("sdk.engine","counter",record)));
  CHECK(record.record.tombstone&&record.record.references.empty());return true;
}
bool extension(const std::string& path,bool process,const std::string& directory) {
  ModuleAdmission admission;admission.artifact_path=path;admission.artifact_sha256=file_digest(path);admission.trusted_native=!process;
  EngineDescriptor descriptor;std::shared_ptr<IEngineFactory> factory;
  auto load=[&](const ModuleAdmission& value)->Result {
    return process?load_process_module(value,descriptor,factory):load_native_module(value,descriptor,factory);
  };
  auto wrong=admission;wrong.artifact_sha256=sha256("wrong");CHECK(load(wrong).code==Code::conflict);
  if(!process){auto untrusted=admission;untrusted.trusted_native=false;CHECK(load(untrusted).code==Code::unauthorized);}
  CHECK(success(load(admission)));CHECK(descriptor.module_sha256==admission.artifact_sha256);
  const auto configuration=independent::configuration(4,40000);const auto identity=independent::identity(descriptor,configuration,process?"process":"native");
  std::unique_ptr<IEngine> engine;
  {
    EngineModuleCatalog catalog;CHECK(success(catalog.add(descriptor,factory)));CHECK(success(catalog.freeze()));
    std::filesystem::remove_all(directory);std::unique_ptr<evidence::ICommitStore> store;
    CHECK(success(evidence::open_native_store(directory,{},store)));ProjectionService projection;
    SessionService session(catalog,*store,projection);
    CHECK(success(session.add_engine(identity,configuration)));
    const auto durable_request=step("durable-extension",*session.find(identity.instance_id));
    CHECK(success(session.step(identity.instance_id,durable_request)));
    CHECK(success(session.recover()));CHECK(independent::count(session.find(identity.instance_id)->state())==1);
    CHECK(projection.snapshot().size()==1&&projection.snapshot()[0].observation.unit=="count");
    CHECK(success(session.step(identity.instance_id,durable_request)));CHECK(session.find(identity.instance_id)->state().tick==1);
    CHECK(success(catalog.create(identity,configuration,nullptr,engine)));
  }
  factory.reset(); // Engines and their forks keep native code/process ownership alive.
  const auto initial=engine->state();PreparedTransition prepared;CHECK(success(engine->prepare_step(step("extension-one",*engine,"7"),prepared)));
  CHECK(engine->state()==initial);CHECK(success(engine->install(prepared.target)));CHECK(independent::count(engine->state())==7);
  ExperimentService experiments;TrialResult result;
  CHECK(success(experiments.evaluate(*engine,{"extension-trial",{{"counter.add/1","2"}},{"counter.set/1","11"},true},result)));
  CHECK(independent::count(result.final_state)==13&&independent::count(engine->state())==7);
  return true;
}
bool process_deadline(const std::string& path) {
  ModuleAdmission admission;admission.artifact_path=path;admission.artifact_sha256=file_digest(path);admission.timeout_ms=100;
  EngineDescriptor descriptor;std::shared_ptr<IEngineFactory> factory;
  CHECK(success(load_process_module(admission,descriptor,factory)));
  const auto config=independent::configuration();const auto identity=independent::identity(descriptor,config,"deadline");
  std::unique_ptr<IEngine> engine;CHECK(success(factory->create(identity,config,nullptr,engine)));const auto before=engine->state();
  PreparedTransition transition;const auto started=std::chrono::steady_clock::now();
  CHECK(engine->prepare_step(step("timeout",*engine,"delay"),transition).code==Code::deadline);
  CHECK(std::chrono::steady_clock::now()-started<std::chrono::seconds(2));CHECK(engine->state()==before);
  CHECK(success(factory->create(identity,config,&before,engine)));CHECK(engine->state()==before);return true;
}
}
int main(int argc,char** argv) {
  if(argc!=5){std::cerr<<"expected native module, process module, invalid module, store directory\n";return 2;}
  if(!codecs()||!observers_and_selection()||!runtime(std::string(argv[4])+"-runtime")||!detach(std::string(argv[4])+"-detach")||
      !extension(argv[1],false,std::string(argv[4])+"-native")||
      !extension(argv[2],true,std::string(argv[4])+"-process")||!process_deadline(argv[2]))return 1;
  ModuleAdmission invalid;invalid.artifact_path=argv[3];invalid.artifact_sha256=file_digest(argv[3]);invalid.trusted_native=true;
  EngineDescriptor descriptor;std::shared_ptr<IEngineFactory> factory;
  if(load_native_module(invalid,descriptor,factory).code!=Code::unsupported){std::cerr<<"wrong ABI admitted\n";return 1;}
  std::cout<<"installed SDK / independent engine / native ABI / process / canonical runtime conformance passed\n";return 0;
}
