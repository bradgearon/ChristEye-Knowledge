// Native SW5 digital forks through the generic scoring service, durable route
// commit and authorization policy: never an actuator or wet-lab protocol.
#include "anaiero/services/engine_optimization_service.hpp"
#include "anaiero/services/sw5_scenario_evaluator_service.hpp"
#include "anaiero/services/session_authorization_service.hpp"
#include "anaiero/application/sw5_engine_change_adapter.hpp"
#include "anaiero/services/durable_digital_route_service.hpp"
#include "anaiero/infrastructure/durable_route_ledger.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/infrastructure/cell_checkpoint.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#define CHECK(x) do {if(!(x)){std::cerr<<"FAILED " #x <<" at "<<__LINE__<<"\n";std::abort();}} while(false)
namespace eng=anaiero::engine;
namespace opt=anaiero::optimization;
namespace cells=anaiero::cells;
namespace sec=anaiero::security;
namespace fs=std::filesystem;
struct FakeSession final:sec::IVerifiedSession {
  bool verified=false,ended=false;
  std::uint32_t source=0;
  std::uint64_t epoch=0;
  bool reads=false,writes=false,changes=false;
  bool cryptographically_verified()const noexcept override{return verified;}
  bool expired()const noexcept override{return ended;}
  std::uint32_t bound_source()const noexcept override{return source;}
  std::uint64_t bound_epoch()const noexcept override{return epoch;}
  bool allows(sec::Capability capability)const noexcept override {
    switch(capability) {
      case sec::Capability::subscribe:return reads;
      case sec::Capability::publish_observation:return writes;
      case sec::Capability::propose_digital_change:return changes;
    }
    return false;
  }
};
static void security() {
  sec::SessionAuthorizationService service{};
  FakeSession p{};
  p.source=31;p.epoch=2;p.reads=true;p.writes=true;p.changes=true;
  anaiero::runtime::Observation e{};
  e.source=31;e.source_epoch=2;e.colony=1;e.clock_domain=3;e.sequence=1;
  e.metric=anaiero::runtime::Metric::activity;e.origin=anaiero::runtime::Origin::simulated;
  CHECK(!service.may_publish(p,e));
  CHECK(!service.may_subscribe(p));
  CHECK(!service.may_change_digital_policy(p));
  // This fake represents a *hypothetically* authenticated transport session,
  // not evidence that Anaiero already has a cryptographic remote mesh.
  p.verified=true;
  CHECK(service.may_publish(p,e));
  CHECK(service.may_subscribe(p));
  CHECK(service.may_change_digital_policy(p));
  p.writes=false;CHECK(!service.may_publish(p,e));p.writes=true;
  e.source_epoch=1;CHECK(!service.may_publish(p,e));e.source_epoch=2;
  e.source=32;CHECK(!service.may_publish(p,e));e.source=31;
  e.origin=anaiero::runtime::Origin::external_unverified_readonly;
  CHECK(!service.may_publish(p,e));
  e.origin=anaiero::runtime::Origin::simulated;
  p.ended=true;CHECK(!service.may_publish(p,e));
}
struct MixedForecasts final:eng::IScenarioEvaluator {
  unsigned fault=0;
  bool evaluate(const eng::OptimizationPlan& plan,const eng::Candidate& candidate,
                eng::Forecast& forecast) noexcept override {
    forecast={};forecast.baseline.id=401;forecast.baseline.source_epoch=2;
    forecast.baseline.tick=plan.horizon;forecast.baseline.revision=7;
    forecast.source_revision=7;forecast.source_state_sha256=std::string(64,'a');
    if(candidate.id==2){
      if(fault==1)++forecast.baseline.id;
      if(fault==2)++forecast.baseline.source_epoch;
      if(fault==3){++forecast.baseline.revision;++forecast.source_revision;}
      if(fault==4)forecast.source_state_sha256=std::string(64,'b');
      if(fault==5)++forecast.baseline.tick;
      if(fault==6)++forecast.baseline.energy;
    }
    forecast.trial=forecast.baseline;forecast.trial.connections=candidate.id;
    forecast.valid=true;return true;
  }
};
static void shared_context_regressions(){
  const std::array<eng::Candidate,2> candidates{{{1,1,0,0,0},{2,2,0,0,0}}};
  eng::OptimizationPlan plan{};plan.horizon=18;plan.goal_count=1;
  plan.objectives[0]={eng::ObjectiveMetric::connections,10,1,false};
  MixedForecasts evaluator;eng::EngineOptimizationService optimizer(evaluator);eng::Selection selected;
  CHECK(optimizer.analyze(plan,candidates,selected)&&selected.candidate.id==2);
  for(unsigned fault=1;fault<=6;++fault){
    evaluator.fault=fault;
    CHECK(!optimizer.analyze(plan,candidates,selected));
    CHECK(!selected.approved&&selected.candidate.id==0); // no partially valid result leaks
  }
  evaluator.fault=0;auto duplicates=candidates;duplicates[1].id=duplicates[0].id;
  CHECK(!optimizer.analyze(plan,duplicates,selected)&&!selected.approved);
}
static cells::World seed(const cells::Atlas& atlas,cells::ISpatialPort& spatial){
  cells::CellBehaviorService domain(atlas,spatial);
  cells::Cell a{};a.id=3;a.kind=cells::Kind::central_neuron;
  a.position={20,17,2};a.energy=100;
  cells::Cell b=a;b.id=4;b.position={23,17,2};
  CHECK(domain.add(a)==cells::Status::ok);
  CHECK(domain.add(b)==cells::Status::ok);
  return domain.world();
}
int main(int argc,char**argv){
  CHECK(argc==3);
  security();shared_context_regressions();
  cells::Atlas atlas{};
  CHECK(cells::files::load_checked_atlas(argv[1],atlas)==cells::Status::ok);
  cells::BoundedPositionPort spatial{};
  const auto initial=seed(atlas,spatial);
  const auto source_sha=christeye::living::plans::sha256_hex(
    cells::files::serialize_world(initial,atlas));
  eng::Sw5ScenarioEvaluatorService actual(
    atlas,initial,spatial,{75,0,64,true},401,31,2,7,source_sha);
  const std::array<eng::Candidate,3> candidates{{
    {101,101,2,1,1},
    {102,102,3,1,2},
    {103,103,300,100,100}
  }};
  const std::array<opt::ForkOption,3> routes{{
    {{101,1,3,4,opt::RouteKind::digital_signal,2,1,400},400,1},
    {{102,1,3,4,opt::RouteKind::digital_signal,3,1,700},700,2},
    {{103,1,3,4,opt::RouteKind::digital_signal,300,100,900},900,100}
  }};
  for(std::size_t i=0;i<candidates.size();++i)
    CHECK(actual.add(candidates[i],routes[i]));
  CHECK(!actual.add(candidates[0],routes[0])); // duplicate
  eng::OptimizationPlan plan{};
  plan.horizon=18;plan.goal_count=3;plan.minimum_score=1;
  plan.objectives[0]={eng::ObjectiveMetric::connections,1,6,true};
  plan.objectives[1]={eng::ObjectiveMetric::participants,2,2,true};
  plan.objectives[2]={eng::ObjectiveMetric::learning_proxy,40,1,false};
  eng::EngineOptimizationService optimizer(actual);
  eng::Selection selected{};
  CHECK(optimizer.analyze(plan,candidates,selected));
  CHECK(selected.approved&&selected.candidate.id==102&&selected.score>0);
  CHECK(selected.forecast.source_state_sha256==source_sha);
  CHECK(selected.forecast.source_revision==7);
  CHECK(selected.forecast.baseline.tick==18&&
        selected.forecast.trial.tick==18);
  CHECK(selected.forecast.baseline.connections==0&&
        selected.forecast.trial.connections==1);
  CHECK(initial.link_count==0&&initial.intent_count==0);
  auto wrong=selected;
  wrong.forecast.source_state_sha256=std::string(64,'0');
  const auto root=fs::path(argv[2]);fs::remove_all(root);
  opt::DurableRouteLedger ledger{};
  CHECK(ledger.open(root,atlas)==opt::StoreResult::ok);
  opt::DurableDigitalRouteService durable(ledger,atlas,spatial);
  opt::DigitalRouteLedger initial_ledger{};
  CHECK(durable.initialize(initial,7,initial_ledger)==opt::StoreResult::ok);
  anaiero::application::Sw5EngineChangeAdapter executor(durable);
  for(std::size_t i=0;i<candidates.size();++i)
    CHECK(executor.add(candidates[i],routes[i]));
  std::uint64_t id=0;
  CHECK(!executor.commit(wrong,id));
  CHECK(id==0);
  CHECK(optimizer.commit(selected,executor));
  CHECK(selected.commit_id==1);
  opt::DigitalRouteLedger record{};
  CHECK(durable.read(record)==opt::StoreResult::ok);
  CHECK(record.active&&record.selected.id==102&&
        record.applied_guidance==700&&!record.event_acked);
  // Rollback is not permitted before the durable delivery ACK.
  CHECK(!executor.revert(selected.commit_id));
  CHECK(!optimizer.commit(selected,executor));
  std::cout<<"engine-neutral SW5 forks, pinned durable route and trust policy PASS\n";
}
