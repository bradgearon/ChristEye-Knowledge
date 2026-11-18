#include "anaiero/sdk/engine_files.hpp"
#include "anaiero/sdk/experiment_history.hpp"
#include "anaiero/sdk/lifecycle.hpp"
#include "anaiero/sdk/runtime.hpp"
#include "anaiero/sdk/session_runner.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <source_location>

namespace s=anaiero::sdk;
namespace e=anaiero::engines;
namespace ev=s::evidence;
namespace life=s::lifecycle;
namespace fs=std::filesystem;
#define CHECK(condition) do {if(!(condition)){std::cerr<<"FAILED line "<<__LINE__<<": " #condition "\n";std::exit(1);}}while(false)
static void require(const s::Result& result,std::source_location location=std::source_location::current()) {
  if(!result){std::cerr<<"FAILED line "<<location.line()<<": "<<s::code_name(result.code)<<": "<<result.message<<'\n';std::exit(1);}
}
class LifecyclePolicy final:public s::ICommitPolicy {
  life::Service& service_;
public:
  explicit LifecyclePolicy(life::Service& service):service_(service){}
  s::Result enrich(ev::CommitBatch& batch,const s::PreparedTransition& transition)override {
    return service_.stage_commit(batch,transition);
  }
  s::Result restore_admission(const s::EngineIdentity& identity)const override {
    life::Snapshot snapshot;auto result=service_.snapshot(snapshot);if(!result)return result;
    if(snapshot.quarantined)return {s::Code::unauthorized,"quarantined store"};
    life::Source source;result=service_.source({identity.source_uid,identity.epoch},source);if(!result)return result;
    if(source.identity!=identity)return {s::Code::conflict,"lifecycle engine identity mismatch"};
    if(source.phase==life::SourcePhase::retired)return {s::Code::expired,"retired source"};
    return {};
  }
  s::Result recovery_sources(std::vector<s::EngineIdentity>& output)const override {
    life::Snapshot snapshot;auto result=service_.snapshot(snapshot);if(!result)return result;
    if(snapshot.quarantined)return {s::Code::unauthorized,"quarantined store"};
    output.clear();for(const auto& source:snapshot.sources)
      if(source.phase!=life::SourcePhase::retired&&!source.state_sha256.empty())output.push_back(source.identity);
    return {};
  }
  s::Result scheduling_admission(const s::EngineIdentity& identity,bool& eligible)const override {
    eligible=false;auto result=restore_admission(identity);if(result.code==s::Code::expired)return {};
    if(!result)return result;
    life::Source source;result=service_.source({identity.source_uid,identity.epoch},source);if(!result)return result;
    eligible=source.phase==life::SourcePhase::active;return {};
  }
};
struct Fixture {
  struct Clock:ev::IAdmissionClock {std::uint64_t unix_seconds()const noexcept override{return 100;}} clock;
  fs::path path;
  s::EngineModuleCatalog catalog;
  std::unique_ptr<ev::ICommitStore> store;
  std::unique_ptr<life::Service> lifecycle;
  s::ProjectionService projection{64};
  std::unique_ptr<LifecyclePolicy> policy;
  std::unique_ptr<s::SessionService> runtime;
  std::unique_ptr<s::SessionRunner> runner;
  explicit Fixture(fs::path directory):path(std::move(directory)) {
    require(e::register_builtin_engines(catalog));require(catalog.freeze());reopen();
  }
  void reopen() {
    runner.reset();runtime.reset();policy.reset();lifecycle.reset();store.reset();projection.clear();
    ev::StoreOptions storage;storage.admission_clock=&clock;
    require(ev::open_native_store(path.string(),storage,store));
    if(!store->info().index_ready||!store->info().writable)require(store->repair_index());
    life::Options options;options.max_active_sources=5;options.max_metrics=64;
    lifecycle=std::make_unique<life::Service>(*store,options);require(lifecycle->open());
    policy=std::make_unique<LifecyclePolicy>(*lifecycle);
    runtime=std::make_unique<s::SessionService>(catalog,*store,projection,policy.get(),5);
    require(runtime->recover());runner=std::make_unique<s::SessionRunner>(*runtime,*store);
    auto opened=runner->open();CHECK(opened.ok()||opened.code==s::Code::not_found);
  }
  void add(const s::EngineSchedule& schedule,std::string prefix) {
    require(lifecycle->register_engine(schedule.identity,"test-operator",prefix+"-register"));
    require(lifecycle->activate({schedule.identity.source_uid,schedule.identity.epoch},prefix+"-activate"));
    require(runtime->add_engine(schedule.identity,schedule.configuration));
  }
  s::EngineState state(std::string_view id)const {
    for(const auto& value:runtime->view().engines)if(value.identity.instance_id==id)return value.state;
    std::cerr<<"active state absent: "<<id<<'\n';std::exit(1);
  }
  void finish_pending() {
    require(runner->begin_cycle());
    for(;;) {
      std::string id;s::StepRequest request;bool pending=false;require(runner->next_step(id,request,pending));
      if(!pending)break;
      require(runtime->step(id,request));
    }
    require(runner->finish_cycle());
  }
  void through(std::uint64_t cycle) {
    while(runner->progress().completed_cycles<cycle)finish_pending();
  }
};
static void integrated(const fs::path& repository,const fs::path& directory) {
  fs::remove_all(directory);Fixture fixture(directory);s::SessionDefinition definition;
  require(e::read_session_definition((repository/"examples/runtime/modular-native.cevsession").string(),fixture.catalog,definition));
  CHECK(definition.engines.size()==5);require(fixture.runner->initialize(definition));
  for(const auto& schedule:definition.engines)fixture.add(schedule,"initial-"+schedule.identity.instance_id);
  const auto initial_environment=fixture.state("environment");
  const auto old_environment=definition.engines.back();
  const auto initial_object=s::checkpoint_digest(old_environment.identity,initial_environment);
  s::EngineIdentity overflow;require(e::identify(fixture.catalog,e::environment,"overflow","overflow",1,
    old_environment.configuration,overflow));
  CHECK(fixture.lifecycle->register_engine(overflow,"test-operator","at-capacity").code==s::Code::capacity);

  // A durable cycle intent survives a process boundary after only one engine.
  require(fixture.runner->begin_cycle());std::string first_id;s::StepRequest first;bool pending=false;
  require(fixture.runner->next_step(first_id,first,pending));CHECK(pending&&first_id=="cells");
  require(fixture.runtime->step(first_id,first));CHECK(fixture.state("cells").tick==1);
  fixture.reopen();CHECK(fixture.runner->progress().cycle_pending&&fixture.runner->progress().completed_cycles==0);
  require(fixture.runtime->step(first_id,first));CHECK(fixture.state("cells").tick==1);
  std::string resumed_id;s::StepRequest resumed;require(fixture.runner->next_step(resumed_id,resumed,pending));
  CHECK(pending&&resumed_id!="cells");fixture.finish_pending();
  CHECK(fixture.state("program").tick==1&&fixture.state("colonies").tick==0);

  // The canonical root can survive a crash before its index and owner finish.
  require(fixture.runner->begin_cycle());require(fixture.runner->next_step(first_id,first,pending));CHECK(pending&&first_id=="cells");
  ev::set_native_store_fault(*fixture.store,ev::FaultPoint::after_root);
  CHECK(fixture.runtime->step(first_id,first).code==s::Code::committed_degraded);
  fixture.reopen();CHECK(fixture.state("cells").tick==2);
  require(fixture.runtime->step(first_id,first));CHECK(fixture.state("cells").tick==2);
  fixture.finish_pending();CHECK(fixture.state("colonies").tick==1);
  std::cout<<"partial-cycle and canonical-root recovery passed\n"<<std::flush;

  fixture.through(14);
  CHECK(fixture.state("program").tick==12&&fixture.state("program").phase==s::Phase::completed);
  CHECK(fixture.state("cells").tick==14&&fixture.state("colonies").tick==7);
  CHECK(fixture.state("voltage").tick==14&&fixture.state("environment").tick==14);
  std::cout<<"finite VLM and mixed-cadence continuation passed\n"<<std::flush;
  require(fixture.runtime->control("cells","pause-cell",s::Phase::paused));const auto paused=fixture.state("cells");
  require(fixture.runtime->control("cells","pause-cell",s::Phase::paused));CHECK(fixture.state("cells")==paused);
  fixture.reopen();require(fixture.runtime->control("cells","pause-cell",s::Phase::paused));CHECK(fixture.state("cells")==paused);
  fixture.through(16);CHECK(fixture.state("cells").tick==14&&fixture.state("environment").tick==16);
  require(fixture.runtime->control("cells","resume-cell",s::Phase::running));const auto resumed_state=fixture.state("cells");
  require(fixture.runtime->control("cells","resume-cell",s::Phase::running));CHECK(fixture.state("cells")==resumed_state);
  require(fixture.runtime->control("voltage","cancel-voltage",s::Phase::cancelled));const auto cancelled=fixture.state("voltage");
  require(fixture.runtime->control("voltage","cancel-voltage",s::Phase::cancelled));CHECK(fixture.state("voltage")==cancelled);
  fixture.through(18);CHECK(fixture.state("cells").tick==16&&fixture.state("colonies").tick==9);
  CHECK(fixture.state("program").tick==12&&fixture.state("voltage").tick==16);

  // SW5 changes its world; SIM3 changes a coherent configuration/checkpoint pair.
  const auto cell_before=fixture.state("cells"),colony_before=fixture.state("colonies");
  s::StepRequest cell_change{"cell-route",cell_before.revision,{"anaiero.cell-change/1","CECELLCHANGE/1 route 3 4 700\n"}};
  s::StepRequest colony_change{"colony-link",colony_before.revision,{"anaiero.colony-change/1","CECOLONYCHANGE/1 link crest blood food 0\n"}};
  require(fixture.runtime->step("cells",cell_change,true));require(fixture.runtime->step("colonies",colony_change,true));
  const auto cell_changed=fixture.state("cells"),colony_changed=fixture.state("colonies");
  CHECK(cell_changed.tick==cell_before.tick&&cell_changed.checkpoint!=cell_before.checkpoint);
  CHECK(colony_changed.tick==colony_before.tick&&colony_changed.configuration!=colony_before.configuration);
  CHECK(colony_changed.checkpoint!=colony_before.checkpoint);
  fixture.reopen();CHECK(fixture.state("cells")==cell_changed&&fixture.state("colonies")==colony_changed);
  require(fixture.runtime->step("cells",cell_change,true));require(fixture.runtime->step("colonies",colony_change,true));
  CHECK(fixture.state("cells")==cell_changed&&fixture.state("colonies")==colony_changed);
  fixture.through(20);CHECK(fixture.state("colonies").configuration==colony_changed.configuration);
  std::cout<<"phase retries and real SW5/SIM3 changes passed\n"<<std::flush;

  // Drain/seal leave the materialization inspectable but do not stop other work.
  const life::SourceRef old_scope{old_environment.identity.source_uid,old_environment.identity.epoch};
  require(fixture.lifecycle->begin_drain(old_scope,"environment-drain"));bool eligible=true;
  require(fixture.runtime->scheduling_admission("environment",eligible));CHECK(!eligible);
  fixture.through(22);CHECK(fixture.state("environment").tick==20&&fixture.state("cells").tick==20);
  life::Source source;require(fixture.lifecycle->source(old_scope,source));
  require(fixture.lifecycle->seal(old_scope,{source.last_sequence,source.last_payload_sha256},"environment-seal"));
  require(fixture.lifecycle->retire(old_scope,"environment-retire"));
  // Simulate a crash before the engine's separate detach cleanup.
  fixture.reopen();CHECK(!fixture.runtime->find("environment")&&fixture.runtime->view().engines.size()==4);
  require(fixture.runtime->scheduling_admission("environment",eligible));CHECK(!eligible);
  require(fixture.runtime->remove_retired("environment"));require(fixture.runtime->remove_retired("environment"));
  ev::StoredRecord retired;require(fixture.store->latest("sdk.engine","environment",retired));
  CHECK(retired.record.tombstone&&retired.record.references.empty());
  CHECK(fixture.lifecycle->register_engine(old_environment.identity,"test-operator","reject-retired-epoch").code==s::Code::conflict);
  require(fixture.runtime->scheduling_admission("environment",eligible));CHECK(!eligible);

  // Expire an old epoch's initial state, then admit the exact same logical state
  // in a new epoch. Its source-scoped object identity must be different.
  life::MaintenanceRequest maintenance{"expire-initial",old_scope,0,100,life::HistoryAvailability::expired,{}, {initial_object}};
  life::MaintenancePlan plan;require(fixture.lifecycle->review_maintenance(maintenance,"initial-review",plan));
  require(fixture.lifecycle->activate_maintenance(maintenance.id,100,"initial-activate"));
  require(fixture.lifecycle->execute_maintenance(maintenance.id,"initial-prune"));
  s::Bytes bytes;CHECK(fixture.store->get_blob(initial_object,bytes).code==s::Code::history_expired);
  s::EngineSchedule replacement=old_environment;
  require(e::identify(fixture.catalog,e::environment,"environment-next",old_environment.identity.source_uid,2,
    replacement.configuration,replacement.identity));
  auto updated=fixture.runner->definition();updated.engines.back()=replacement;
  require(fixture.runner->reconfigure(updated,"replace-retired-source"));fixture.add(replacement,"replacement");
  CHECK(fixture.state("environment-next")==initial_environment);
  CHECK(s::state_digest(fixture.state("environment-next"))==s::state_digest(initial_environment));
  const auto new_object=s::checkpoint_digest(replacement.identity,fixture.state("environment-next"));
  CHECK(new_object!=initial_object);require(fixture.store->get_blob(new_object,bytes));
  s::EngineIdentity checkpoint_identity;s::EngineState checkpoint_state;require(s::decode_checkpoint(bytes,checkpoint_identity,checkpoint_state));
  CHECK(checkpoint_identity==replacement.identity&&checkpoint_state==initial_environment);
  CHECK(fixture.policy->restore_admission(old_environment.identity).code==s::Code::expired);
  fixture.reopen();CHECK(fixture.runtime->view().engines.size()==5&&!fixture.runtime->find("environment"));
  fixture.through(24);CHECK(fixture.state("environment-next").tick==2&&fixture.state("colonies").tick==12);
  CHECK(fixture.state("program").phase==s::Phase::completed&&fixture.state("voltage").phase==s::Phase::cancelled);
  CHECK(fixture.runner->progress().completed_cycles==24&&!fixture.runner->progress().cycle_pending);
  life::Snapshot snapshot;require(fixture.lifecycle->snapshot(snapshot));CHECK(snapshot.sources.size()==5);
  require(fixture.lifecycle->source(old_scope,source));CHECK(source.phase==life::SourcePhase::retired);
  ev::AuditReport audit;require(fixture.store->audit(audit));CHECK(audit.generations>100);
  CHECK(!fs::exists(directory/"culture.state"));
  std::cout<<"retirement, scoped checkpoint expiry and active-slot reuse passed\n"<<std::flush;
}
struct MaximumLinks final:s::IScoreStrategy {
  s::ScoreStrategyIdentity identity()const override {
    return {"test.maximum-links/1",s::sha256("test-maximum-links-v1"),{"test.score-config/1","active_links"}};
  }
  s::Result score(const std::vector<s::MetricComparison>& metrics,s::Score& output)const override {
    for(const auto& value:metrics)if(value.candidate.metric=="active_links") {
      if(value.candidate.quantity!="synthetic_link_count"||value.candidate.unit!="count")
        return {s::Code::conflict,"link semantics changed"};
      output={value.candidate.value,true,"more connected synthetic links"};return {};
    }
    return {s::Code::unsupported,"active links absent"};
  }
};
static void durable_experiments(const fs::path& repository,const fs::path& directory) {
  fs::remove_all(directory);Fixture fixture(directory);s::SessionDefinition all;
  require(e::read_session_definition((repository/"examples/runtime/modular-native.cevsession").string(),fixture.catalog,all));
  s::SessionDefinition definition{"experiment-fixture",{all.engines.front()}};
  require(fixture.runner->initialize(definition));fixture.add(definition.engines.front(),"experiment-source");
  const auto initial=fixture.state("cells");const auto identity=definition.engines.front().identity;
  const life::SourceRef source{identity.source_uid,identity.epoch};
  const auto initial_object=s::checkpoint_digest(identity,initial);MaximumLinks strategy;
  // The real fixture starts at maturity zero and gains two per step. The
  // guided route can form only once both cells reach maturity twelve.
  s::ExperimentSpecification specification{"frozen",std::vector<s::Envelope>(6,definition.engines.front().input),
    {{"route",{"anaiero.cell-change/1","CECELLCHANGE/1 route 3 4 700\n"}}}};
  s::RecordedExperiment selected;
  {
    s::ExperimentHistoryService history(fixture.catalog,*fixture.store);
    ev::set_native_store_fault(*fixture.store,ev::FaultPoint::after_root);
    CHECK(history.select(*fixture.runtime->find("cells"),specification,strategy,selected).code==s::Code::committed_degraded);
    CHECK(fixture.state("cells")==initial);
  }
  fixture.reopen();
  {
    s::ExperimentHistoryService history(fixture.catalog,*fixture.store);
    require(history.select(*fixture.runtime->find("cells"),specification,strategy,selected));
    CHECK(selected.completed&&selected.source_state==initial&&selected.result.chosen_candidate=="route");
    const auto original_program=selected.result.program_sha256;
    fixture.through(1);const auto advanced=fixture.state("cells");CHECK(advanced.tick==1);
    s::RecordedExperiment retried;require(history.select(*fixture.runtime->find("cells"),specification,strategy,retried));
    CHECK(retried.source_state==initial&&retried.result.program_sha256==original_program);
    CHECK(fixture.state("cells")==advanced);
    CHECK(history.apply_selected("frozen","stale-apply",*fixture.runtime).code==s::Code::conflict);

    auto fresh=specification;fresh.id="fresh";
    require(history.select(*fixture.runtime->find("cells"),fresh,strategy,selected));
    CHECK(selected.source_state==advanced&&selected.result.chosen_candidate=="route");
    auto paused_measurement=specification;paused_measurement.id="paused-measurement";
    s::RecordedExperiment paused_selection;
    require(history.select(*fixture.runtime->find("cells"),paused_measurement,strategy,paused_selection));
    require(history.apply_selected("fresh","apply-selected",*fixture.runtime));const auto applied=fixture.state("cells");
    require(history.apply_selected("fresh","apply-selected",*fixture.runtime));CHECK(fixture.state("cells")==applied);
  }
  fixture.reopen();
  {
    s::ExperimentHistoryService history(fixture.catalog,*fixture.store);
    require(history.apply_selected("fresh","apply-selected",*fixture.runtime));
    fixture.through(7);s::OutcomeReview review;
    require(history.review("fresh","measure-selected",strategy,review));
    CHECK(review.meets_selected_score&&review.exact_selected_state);
    const auto measured=review.observed_state_sha256;
    require(fixture.runtime->control("cells","pause-at-horizon",s::Phase::paused));
    s::OutcomeReview paused_review;require(history.review("paused-measurement","measure-paused",strategy,paused_review));
    CHECK(paused_review.meets_selected_score&&!paused_review.exact_selected_state);
    require(fixture.runtime->control("cells","resume-after-measurement",s::Phase::running));fixture.through(8);
    require(history.review("fresh","measure-selected",strategy,review));CHECK(review.observed_state_sha256==measured);
    life::Source tip;require(fixture.lifecycle->begin_drain(source,"experiment-drain"));require(fixture.lifecycle->source(source,tip));
    require(fixture.lifecycle->seal(source,{tip.last_sequence,tip.last_payload_sha256},"experiment-seal"));
    require(fixture.lifecycle->retire(source,"experiment-retire"));require(fixture.runtime->remove_retired("cells"));
    require(history.apply_selected("fresh","apply-selected",*fixture.runtime)); // Exact retry survives source retirement.
    ev::StoredRecord retained_result;require(fixture.store->latest("sdk.experiment-result","fresh",retained_result));
    require(history.release("frozen","release-frozen"));require(history.release("fresh","release-fresh"));
    require(history.release("paused-measurement","release-paused-measurement"));
    require(history.release("fresh","release-fresh"));
    CHECK(history.load("fresh",selected).code==s::Code::history_expired);
    ev::BlobInfo released;require(fixture.store->blob_info(retained_result.record.payload,released));
    CHECK(released.expired_generation!=0&&!released.present);
  }
  life::MaintenanceRequest request{"release-experiment-source",source,0,100,life::HistoryAvailability::expired,{}, {initial_object}};
  life::MaintenancePlan maintenance;
  require(fixture.lifecycle->review_maintenance(request,"experiment-maintenance-review",maintenance));
  require(fixture.lifecycle->activate_maintenance(request.id,100,"experiment-maintenance-activate"));
  require(fixture.lifecycle->execute_maintenance(request.id,"experiment-maintenance-prune"));
  s::Bytes bytes;CHECK(fixture.store->get_blob(initial_object,bytes).code==s::Code::history_expired);
  ev::AuditReport audit;require(fixture.store->audit(audit));
}
int main(int argc,char** argv) {
  CHECK(argc==3||argc==4);const auto repository=fs::canonical(argv[1]);const auto directory=fs::absolute(argv[2]);
  const std::string_view mode=argc==4?argv[3]:"all";
  CHECK(mode=="all"||mode=="engines"||mode=="experiments");
  if(mode!="experiments")integrated(repository,directory);
  if(mode!="engines")durable_experiments(repository,fs::path(directory.string()+"-experiments"));
  std::cout<<"Five-engine durable cycles, finite VLM, changes, lifecycle fencing and scoped checkpoint reuse passed\n";
}
