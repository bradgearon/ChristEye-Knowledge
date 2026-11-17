// Real SW5/SIM3 trials, immutable plan assets, durable interrupted progress,
// cancellation and restart, source identities, and later outcome decisions.
#include "anaiero/application/session_experiment_runtime.hpp"
#include "anaiero/application/sw5_simulation_adapter.hpp"
#include "anaiero/application/sim3_simulation_adapter.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x <<" line "<<__LINE__<<'\n';std::abort();}}while(false)
namespace ex=anaiero::experiments;
namespace files=ex::files;
namespace app=anaiero::application;
namespace eng=anaiero::engine;
namespace cells=anaiero::cells;
namespace sim3=christeye::living::sim3;
namespace db=christeye::living::durable;
namespace pl=christeye::living::plans;
namespace fs=std::filesystem;
static void write(const fs::path& file,std::string_view bytes){
  fs::create_directories(file.parent_path());std::ofstream stream(file,std::ios::binary);stream<<bytes;CHECK(stream.good());
}
static cells::World seed(const cells::Atlas& atlas,cells::ISpatialPort& spatial){
  cells::CellBehaviorService service(atlas,spatial);cells::Cell a{};
  a.id=3;a.kind=cells::Kind::central_neuron;a.position={20,17,2};a.energy=100;
  auto b=a;b.id=4;b.position={23,17,2};
  CHECK(service.add(a)==cells::Status::ok&&service.add(b)==cells::Status::ok);return service.world();
}
static ex::Definition definition(eng::Descriptor descriptor){
  ex::Definition d{};d.name="guidance-comparison";d.engine_id=descriptor.id;d.kind=descriptor.kind;
  d.model_sha256=descriptor.model_digest;d.behavior_id=descriptor.capabilities.behavior_id;
  d.metric_schema=descriptor.capabilities.metric_schema;d.clock_domain=descriptor.capabilities.clock_domain;d.tick_ns=descriptor.capabilities.tick_ns;
  d.optimization.horizon=18;d.optimization.goal_count=3;d.optimization.minimum_score=1;
  d.optimization.objectives[0]={eng::ObjectiveMetric::connections,1,6,true};
  d.optimization.objectives[1]={eng::ObjectiveMetric::participants,2,2,true};
  d.optimization.objectives[2]={eng::ObjectiveMetric::learning_proxy,40,1,false};
  d.stage_count=2;d.stages[0]={"mature",9,{75,0,64,true}};d.stages[1]={"observe",9,{75,0,64,true}};
  d.candidate_count=3;
  d.candidates[0]={{101,101,2,1,1},ex::PolicyKind::sw5_guidance,"3","4",400,0};
  d.candidates[1]={{102,102,3,1,2},ex::PolicyKind::sw5_guidance,"3","4",700,0};
  d.candidates[2]={{103,103,300,100,100},ex::PolicyKind::sw5_guidance,"3","4",900,0};
  d.outcome_ticks=18;CHECK(files::seal_definition(d));
  if(d.kind==eng::Kind::sw5)CHECK(ex::ExperimentService::valid_definition(d));
  return d;
}
struct Cancellation final:ex::ICancellation {
  mutable unsigned calls=0;unsigned after=0;
  bool cancelled()const noexcept override{return ++calls>=after;}
};
struct MemoryStore final:ex::IRecordStore {
  ex::Record value{};bool exists=false;unsigned fault=0;
  bool accepts_definition(const ex::Definition& d)const noexcept override{return files::accepts_definition(d);}
  bool load(std::string_view id,ex::Record& out,bool& found)noexcept override {
    found=exists&&value.operation_id==id;if(found)out=value;return true;
  }
  bool save(ex::Record& in)noexcept override {
    if(fault==1){fault=0;return false;}
    if((exists&&in.revision!=value.revision)||(!exists&&in.revision))return false;
    value=in;++value.revision;exists=true;
    if(fault==2){fault=0;return false;}
    in.revision=value.revision;return true;
  }
};
static void codecs_and_assets(const fs::path& repository,const fs::path& root,const ex::Definition& definition){
  auto d=definition;std::string error;ex::Definition parsed{};
  const auto bytes=files::serialize_definition(d);
  CHECK(files::parse_definition(bytes,parsed,error)&&parsed.sha256==d.sha256);
  std::string crlf;for(char c:bytes){if(c=='\n')crlf+='\r';crlf+=c;}
  CHECK(files::parse_definition(crlf,parsed,error)&&parsed.sha256==d.sha256);
  auto corrupt=bytes;corrupt.replace(corrupt.find("candidates 3"),12,"candidates 9");
  CHECK(!files::parse_definition(corrupt,parsed,error));
  corrupt=bytes;corrupt.replace(corrupt.find("stage mature 9"),14,"stage mature 8");
  CHECK(!files::parse_definition(corrupt,parsed,error));
  d.stages[0].inputs.support_proxy=74;CHECK(!files::accepts_definition(d));d=definition;
  // Original CEVPLAN grammar and process/base binding validation are reused,
  // including a commented, unsealed source base accepted by its original parser.
  std::string plan_bytes,base_bytes;
  CHECK(pl::read_bounded(repository/"examples/vlm/bread-first.cevplan",pl::kMaxPlanBytes,plan_bytes));
  CHECK(pl::read_bounded(repository/"examples/vlm/plan-garden.avlm",pl::kMaxArtifactBytes,base_bytes));
  pl::Plan plan;CHECK(pl::Plan::parse(plan_bytes,plan).error==pl::Error::ok);
  for(const auto& artifact:plan.artifacts()){
    std::string data;CHECK(pl::read_bounded(repository/artifact.relative_path,pl::kMaxArtifactBytes,data));
    write(root/artifact.relative_path,data);
  }
  write(root/"linked.cevplan",plan_bytes);write(root/"base.avlm",base_bytes);
  d.learning_plan_path="linked.cevplan";d.learning_plan_sha256=pl::sha256_hex(plan_bytes);
  d.learning_base_path="base.avlm";d.learning_base_sha256=pl::sha256_hex(base_bytes);
  const auto& artifact=plan.artifacts().front();d.artifact_count=1;
  d.artifacts[0]={artifact.id,artifact.role,artifact.relative_path,artifact.sha256};
  CHECK(files::seal_definition(d));write(root/"linked.cevexperiment",files::serialize_definition(d));
  CHECK(files::load_definition(root/"linked.cevexperiment",parsed,error));
  db::Registry registry;CHECK(registry.open(root/"store",db::StorageOptions::sustained(8))==db::Code::ok);
  CHECK(files::register_definition(registry,root/"linked.cevexperiment",parsed,error));
  CHECK(files::lookup_definition(registry,d.name,d.sha256,parsed));
  const auto old_hash=d.sha256;
  d.name=definition.name;d.outcome_tolerance=1;CHECK(files::seal_definition(d));
  write(root/"linked.cevexperiment",files::serialize_definition(d));
  CHECK(files::register_definition(registry,root/"linked.cevexperiment",parsed,error));
  CHECK(files::lookup_definition(registry,d.name,old_hash,parsed)&&parsed.sha256==old_hash);
  write(root/"base.avlm",base_bytes+"# changed file\n");
  CHECK(!files::load_definition(root/"linked.cevexperiment",parsed,error));
  write(root/"base.avlm",base_bytes);
  d.artifacts[0].relative_path="../escape.txt";CHECK(!files::seal_definition(d));
  d=definition;d.artifact_count=1;d.artifacts[0]={"wrong-role","digital_learning","wrong.txt",pl::sha256_hex("CERNA/1\n")};
  write(root/"wrong.txt","CERNA/1\n");CHECK(files::seal_definition(d));write(root/"wrong.cevexperiment",files::serialize_definition(d));
  CHECK(!files::load_definition(root/"wrong.cevexperiment",parsed,error));
}
static void domain_and_recovery(const cells::Atlas& atlas,const cells::World& initial,eng::Descriptor descriptor){
  cells::BoundedPositionPort spatial;auto d=definition(descriptor);
  app::Sw5ExperimentAdapter evaluator(descriptor,atlas,initial,spatial);
  MemoryStore store;ex::ExperimentService service(store,evaluator);
  auto wrong=d;wrong.model_sha256=std::string(64,'a');CHECK(files::seal_definition(wrong));
  CHECK(service.start("wrong-model",wrong,std::string(evaluator.source_checkpoint()))==ex::Code::identity_mismatch);
  CHECK(service.start("wrong-source",d,"wrong checkpoint")==ex::Code::identity_mismatch);
  wrong=d;wrong.seed=1;CHECK(files::seal_definition(wrong));ex::EvaluationContext context;
  CHECK(evaluator.context(wrong,context)==ex::Code::unsupported);
  CHECK(service.start("memory-run",d,std::string(evaluator.source_checkpoint()))==ex::Code::ok);
  CHECK(service.step()==ex::Code::ok&&service.record().next_candidate==1&&evaluator.baseline_simulations()==1);
  const auto first=service.record().trials[0].receipt_sha256;
  Cancellation cancel;cancel.after=6;
  CHECK(service.step(&cancel)==ex::Code::cancelled&&service.record().state==ex::RunState::paused);
  CHECK(service.record().next_candidate==1&&service.record().trials[0].receipt_sha256==first);
  ex::ExperimentService recovered(store,evaluator);CHECK(recovered.recover("memory-run")==ex::Code::ok);
  CHECK(recovered.resume()==ex::Code::ok);
  CHECK(recovered.step()==ex::Code::ok&&recovered.step()==ex::Code::ok);
  CHECK(recovered.record().state==ex::RunState::completed&&recovered.record().selection.candidate.id==102);
  CHECK(evaluator.baseline_simulations()==1); // one unchanged baseline for all candidates and retries
  CHECK(initial.intent_count==0&&initial.link_count==0);
  CHECK(recovered.mark_applied(7,initial.tick+1)==ex::Code::invalid_state);
  CHECK(recovered.mark_applied(7,initial.tick)==ex::Code::ok);
  CHECK(recovered.mark_applied(7,initial.tick)==ex::Code::ok);
  auto observed=recovered.record().selection.forecast.trial;
  observed.tick=initial.tick+18;observed.revision=initial.generation+18;
  observed.participants=0;observed.energy=0;observed.state_sha256.fill('e');observed.state_sha256[64]='\0';
  auto bad=observed;bad.tick++;
  CHECK(recovered.observe(bad)==ex::Code::identity_mismatch&&recovered.record().outcome.decision==ex::OutcomeDecision::pending);
  CHECK(recovered.observe(observed)==ex::Code::ok&&recovered.record().outcome.decision==ex::OutcomeDecision::revert);
  CHECK(recovered.observe(observed)==ex::Code::ok);
  auto conflicting=observed;++conflicting.energy;
  CHECK(recovered.observe(conflicting)==ex::Code::identity_mismatch); // same digest cannot authorize changed summaries
  ex::ExperimentService outcome_reopen(store,evaluator);CHECK(outcome_reopen.recover("memory-run")==ex::Code::ok);
  auto valid=store.value;store.value.outcome.residuals[0]++;
  ex::ExperimentService corrupt(store,evaluator);CHECK(corrupt.recover("memory-run")==ex::Code::corrupt);store.value=valid;
  store.value.trials[0].candidate.cost++;
  store.value.trials[0].receipt_sha256=ex::ExperimentService::receipt_hash(d,store.value.context,store.value.trials[0]);
  ex::ExperimentService substituted(store,evaluator);CHECK(substituted.recover("memory-run")==ex::Code::corrupt);store.value=valid;
  // A storage failure poisons this service until reconstruction; no skipped or
  // duplicate trial progress can be driven from an ambiguous in-memory state.
  for(unsigned fault:{1U,2U}){
    MemoryStore uncertain;ex::ExperimentService failing(uncertain,evaluator);
    CHECK(failing.start("fault",d,std::string(evaluator.source_checkpoint()))==ex::Code::ok);
    CHECK(failing.pause()==ex::Code::cancelled&&failing.resume()==ex::Code::ok);
    uncertain.fault=fault;CHECK(failing.step()==ex::Code::persistence_failed&&!failing.busy());
    CHECK(failing.step()==ex::Code::persistence_failed);
    ex::ExperimentService retry(uncertain,evaluator);CHECK(retry.recover("fault")==ex::Code::ok);
    CHECK(retry.record().next_candidate==(fault==2?1:0));
    while(retry.busy())CHECK(retry.step()==ex::Code::ok);
    CHECK(retry.record().selection.candidate.id==102);
  }
  // Updating an existing guidance intent must predict the same generation and
  // complete checkpoint as the live route policy, not only the same summaries.
  cells::CellBehaviorService current(atlas,spatial);CHECK(current.restore(initial)==cells::Status::ok);
  CHECK(current.encourage(3,4,400)==cells::Status::ok);const auto with_intent=current.world();
  app::Sw5ExperimentAdapter updating(descriptor,atlas,with_intent,spatial);eng::Forecast predicted{};
  CHECK(updating.context(d,context)==ex::Code::ok&&updating.evaluate(d,context,1,predicted,nullptr)==ex::Code::ok);
  anaiero::optimization::DigitalRoutePolicyService live_policy(atlas,spatial);cells::World changed{};
  bool existed=false;std::uint16_t old_weight=0;
  const anaiero::optimization::ForkOption option{{102,descriptor.id,3,4,anaiero::optimization::RouteKind::digital_signal,3,1,700},700,2};
  CHECK(live_policy.apply(with_intent,option,changed,existed,old_weight)&&existed&&old_weight==400);
  CHECK(current.restore(changed)==cells::Status::ok);
  for(std::size_t stage=0;stage<d.stage_count;++stage){const auto& in=d.stages[stage].inputs;
    for(std::uint16_t tick=0;tick<d.stages[stage].ticks;++tick)
      CHECK(current.step({in.support_proxy,in.stress_proxy,in.digital_signal,in.lesson})==cells::Status::ok);
  }
  CHECK(pl::sha256_hex(cells::files::serialize_world(current.world(),atlas))==files::digest_view(predicted.trial));
}
static void durable_faults(const fs::path& root,const cells::Atlas& atlas,const cells::World& initial,eng::Descriptor descriptor){
  cells::BoundedPositionPort spatial;const auto d=definition(descriptor);
  const std::array<db::FaultSite,3> faults{{db::FaultSite::after_journal_sync,db::FaultSite::after_head_temp_sync,db::FaultSite::after_head_commit}};
  for(std::size_t i=0;i<faults.size();++i){
    const auto path=root/std::to_string(i);
    {
      db::Registry registry;CHECK(registry.open(path,db::StorageOptions::sustained(8))==db::Code::ok);
      files::WindStoreExperimentStore store(registry);app::Sw5ExperimentAdapter evaluator(descriptor,atlas,initial,spatial);
      ex::ExperimentService service(store,evaluator);
      CHECK(service.start("durable",d,std::string(evaluator.source_checkpoint()))==ex::Code::ok);
      CHECK(service.pause()==ex::Code::cancelled&&service.resume()==ex::Code::ok);
      registry.inject_next_publish(faults[i]);CHECK(service.step()==ex::Code::persistence_failed);
    }
    {
      db::Registry registry;CHECK(registry.open(path,db::StorageOptions::sustained(8))==db::Code::ok);
      files::WindStoreExperimentStore store(registry);ex::Record saved;bool exists=false;
      CHECK(store.load("durable",saved,exists)&&exists&&saved.next_candidate==(i==2?1:0));
      cells::World source{};CHECK(cells::files::deserialize_world(saved.source_checkpoint,atlas,source)==cells::Status::ok);
      app::Sw5ExperimentAdapter evaluator(descriptor,atlas,source,spatial);ex::ExperimentService service(store,evaluator);
      CHECK(service.recover("durable")==ex::Code::ok);
      while(service.busy())CHECK(service.step()==ex::Code::ok);
      CHECK(service.record().selection.candidate.id==102);
      const auto raw=files::serialize_record(service.record());ex::Record decoded;
      CHECK(files::deserialize_record(raw,decoded)&&decoded.revision==service.record().revision);
      auto corrupt=raw;corrupt[corrupt.find("context ")+8]='0';CHECK(!files::deserialize_record(corrupt,decoded));
      CHECK(!store.save(saved)); // stale CAS writer cannot replace newer receipts
      CHECK(service.mark_applied(9,initial.tick)==ex::Code::ok);
      auto observed=service.record().selection.forecast.trial;observed.revision=initial.generation+18;
      CHECK(service.observe(observed)==ex::Code::ok);
      ex::ExperimentService next_run(store,evaluator);
      CHECK(next_run.start("next-durable",d,std::string(evaluator.source_checkpoint()))==ex::Code::ok);
      CHECK(registry.event_count()>registry.events().size()); // a rotated history remains directly recoverable
    }
    {
      db::Registry registry;CHECK(registry.open(path,db::StorageOptions::sustained(8))==db::Code::ok);
      files::WindStoreExperimentStore store(registry);app::Sw5ExperimentAdapter evaluator(descriptor,atlas,initial,spatial);
      ex::ExperimentService service(store,evaluator);CHECK(service.recover("durable")==ex::Code::ok);
      CHECK(service.record().outcome.decision==ex::OutcomeDecision::retain&&service.record().selection.commit_id==9);
    }
  }
}
static void sim3_trials(const fs::path& model,const fs::path& root){
  sim3::Runner runner;CHECK(runner.open(model,root)==sim3::Result::ok);
  app::Sim3SimulationAdapter live(runner,402,32,3);CHECK(live.initialize()==eng::Status::ok);
  auto d=definition(live.describe());d.name="sim3-teaching-comparison";d.outcome_ticks=0;d.optimization.goal_count=2;
  d.optimization.objectives[0]={eng::ObjectiveMetric::learning_proxy,100,4,true};
  d.optimization.objectives[1]={eng::ObjectiveMetric::participants,4,2,true};
  d.stage_count=1;d.stages[0]={"observe",18,{}};
  d.candidates[0]={{201,201,0,0,0},ex::PolicyKind::sim3_teach,"blood","3",10,0};
  d.candidates[1]={{202,202,0,0,0},ex::PolicyKind::sim3_teach,"blood","3",40,0};
  d.candidates[2]={{203,203,0,0,0},ex::PolicyKind::sim3_link_units,"crest","blood",0,2};
  CHECK(files::seal_definition(d));
  app::Sim3ExperimentAdapter evaluator(live.describe(),runner.configuration(),runner.state());
  const auto source=std::string(evaluator.source_checkpoint());MemoryStore store;ex::ExperimentService service(store,evaluator);
  CHECK(service.start("sim3-run",d,source)==ex::Code::ok);
  while(service.busy())CHECK(service.step()==ex::Code::ok);
  CHECK(service.record().selection.approved&&service.record().selection.candidate.id==202&&evaluator.baseline_simulations()==1);
  CHECK(sim3::encode_snapshot(runner.state())==source); // every trial was isolated
  const auto baseline=service.record().trials[0].forecast.baseline;
  for(unsigned tick=0;tick<18;++tick)CHECK(runner.step(false)==sim3::Result::ok);
  CHECK(runner.snapshot_sha256()==files::digest_view(baseline)); // identical live and isolated transition rules
  eng::Snapshot actual{};CHECK(live.latest(actual)==eng::Status::ok&&actual.participants==baseline.participants);
  auto wrong=d;wrong.candidates[0].to="999";CHECK(files::seal_definition(wrong));
  MemoryStore failed;app::Sim3ExperimentAdapter invalid(live.describe(),runner.configuration(),runner.state());
  ex::ExperimentService rejected(failed,invalid);
  CHECK(rejected.start("missing-cell",wrong,std::string(invalid.source_checkpoint()))==ex::Code::ok);
  CHECK(rejected.step()==ex::Code::evaluation_failed&&rejected.record().trials[0].state==ex::TrialState::failed);
  ex::ExperimentService failure_reopen(failed,invalid);CHECK(failure_reopen.recover("missing-cell")==ex::Code::ok);
  wrong=d;wrong.automatic_apply=true;CHECK(files::seal_definition(wrong));
  CHECK(!ex::ExperimentService::valid_definition(wrong));
}
struct LocalSw5Port final:anaiero::session::ISw5Port {
  app::Sw5SimulationAdapter& engine;cells::Atlas model;cells::ISpatialPort& movement;
  LocalSw5Port(app::Sw5SimulationAdapter& e,cells::Atlas a,cells::ISpatialPort& p):engine(e),model(std::move(a)),movement(p){}
  const cells::Atlas& atlas()const noexcept override{return model;}
  cells::ISpatialPort& spatial()noexcept override{return movement;}
  bool world(cells::World& out)const noexcept override {
    anaiero::infrastructure::MemoryCheckpoint bytes;
    return engine.checkpoint(bytes)==eng::Status::ok&&cells::files::deserialize_world(bytes.bytes(),model,out)==cells::Status::ok;
  }
  bool encode(const cells::World& w,std::string& out)const noexcept override{out=cells::files::serialize_world(w,model);return true;}
  bool replace(std::string_view value)noexcept override {
    anaiero::infrastructure::MemoryCheckpoint bytes;return bytes.write(value)&&engine.restore(bytes)==eng::Status::ok;
  }
};
static void runtime_recovery(const fs::path& root,const cells::Atlas& atlas,const cells::World& initial){
  cells::BoundedPositionPort spatial;app::Sw5SimulationAdapter live(401,31,2,atlas,initial,spatial);
  CHECK(live.initialize()==eng::Status::ok);LocalSw5Port port(live,atlas,spatial);
  const auto d=definition(live.describe());const auto plan=root/"plan.cevexperiment";
  write(plan,files::serialize_definition(d));
  {
    db::Registry registry;CHECK(registry.open(root/"store",db::StorageOptions::sustained(8))==db::Code::ok);
    app::SessionExperimentRuntime runtime(registry);CHECK(runtime.bind_sw5(live,port));
    CHECK(runtime.start(41,plan.string())&&runtime.busy()&&runtime.pump()&&runtime.cancel());
    CHECK(runtime.record()->next_candidate==1&&runtime.record()->state==ex::RunState::paused);
  }
  fs::remove(plan);eng::Snapshot changed{};CHECK(live.advance({0,{75,0,64,true}},changed)==eng::Status::ok);
  {
    db::Registry registry;CHECK(registry.open(root/"store",db::StorageOptions::sustained(8))==db::Code::ok);
    app::SessionExperimentRuntime runtime(registry);CHECK(runtime.bind_sw5(live,port));
    CHECK(runtime.start(41,plan.string())); // durable source survives file removal and live advancement
    CHECK(runtime.record()->context.start_tick==0&&!runtime.busy()&&runtime.resume());
    const auto revision=runtime.record()->revision;
    CHECK(runtime.resume()&&runtime.record()->revision==revision); // idempotent enclosing-operation replay
  }
  {
    // Simulate restart after the durable resume but before an enclosing session
    // has acknowledged it. Repeating resume must continue the same trial prefix.
    db::Registry registry;CHECK(registry.open(root/"store",db::StorageOptions::sustained(8))==db::Code::ok);
    app::SessionExperimentRuntime runtime(registry);CHECK(runtime.bind_sw5(live,port));
    CHECK(runtime.start(41,plan.string())&&runtime.record()->next_candidate==1&&runtime.busy());
    const auto revision=runtime.record()->revision;CHECK(runtime.resume()&&runtime.record()->revision==revision);
    while(runtime.busy())CHECK(runtime.pump());
    CHECK(runtime.record()->selection.candidate.id==102);
    CHECK(runtime.start(42,d.name+"@"+d.sha256));
    CHECK(runtime.record()->context.start_tick==1);
    while(runtime.busy())CHECK(runtime.pump());
    CHECK(!runtime.failed());
  }
}
int main(int argc,char** argv){
  CHECK(argc==4);const auto root=fs::path(argv[3]);fs::remove_all(root);fs::create_directories(root);
  cells::Atlas atlas{};CHECK(cells::files::load_checked_atlas(argv[1],atlas)==cells::Status::ok);
  cells::BoundedPositionPort spatial;const auto initial=seed(atlas,spatial);
  app::Sw5SimulationAdapter live(401,31,2,atlas,initial,spatial);CHECK(live.initialize()==eng::Status::ok);
  codecs_and_assets(fs::path(argv[2]).parent_path().parent_path(),root/"assets",definition(live.describe()));
  domain_and_recovery(atlas,initial,live.describe());
  durable_faults(root/"faults",atlas,initial,live.describe());
  sim3_trials(argv[2],root/"sim3");runtime_recovery(root/"runtime",atlas,initial);
  std::cout<<"bounded immutable experiments, actual SW5/SIM3 forks, CAS recovery and outcomes PASS\n";
}
