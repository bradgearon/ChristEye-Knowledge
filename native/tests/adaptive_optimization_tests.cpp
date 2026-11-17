#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/services/adaptive_optimization_service.hpp"
#include "anaiero/services/synthetic_trial_service.hpp"
#include "anaiero/services/service_extension_dispatcher.hpp"
#include "anaiero/services/optimization_trigger_service.hpp"
#include "anaiero/infrastructure/in_memory_route_store.hpp"
#include "anaiero/services/runtime_service.hpp"
#include "anaiero/services/cell_behavior_service.hpp"
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cstdlib>
// Assertions must execute in CMake Release builds on GCC, Clang, and MSVC.
#define assert(expr) do { if(!(expr)) std::abort(); } while(false)
#include <iostream>
namespace opt=anaiero::optimization;
namespace rt=anaiero::runtime;
namespace contracts=anaiero::contracts;
struct Routes final: opt::ICandidateSource {
  std::array<opt::Candidate,4> items{{
    {2,1,17,21,opt::RouteKind::digital_signal,4,3},
    {1,1,17,22,opt::RouteKind::digital_learning,2,1},
    {3,1,17,23,opt::RouteKind::digital_learning,500,100},
    {4,1,17,24,opt::RouteKind::digital_signal,1,1}
  }};
  std::size_t count()const noexcept override{return items.size();}
  bool at(std::size_t i,opt::Candidate& o)const noexcept override {
    if(i>=items.size())return false;
    o=items[i];return true;
  }
};
struct Hook final:contracts::IServiceExtension {
  int proposed=0,committed=0,reverted=0,projected=0,dispatched=0,cells=0;
  void on_service_event(const contracts::ServiceEvent& e) noexcept override {
    if(e.service==contracts::ServiceId::adaptive_optimization){
      if(e.phase==contracts::ServicePhase::proposed)++proposed;
      if(e.phase==contracts::ServicePhase::committed)++committed;
      if(e.phase==contracts::ServicePhase::reverted)++reverted;
    }
    if(e.service==contracts::ServiceId::colony_projector)++projected;
    if(e.service==contracts::ServiceId::runtime_dispatch)++dispatched;
    if(e.service==contracts::ServiceId::cell_behavior)++cells;
  }
};
static rt::ColonyState sample(int value,std::uint64_t rev=7,
  rt::Quality quality=rt::Quality::current,
  rt::Origin origin=rt::Origin::simulated) {
  rt::ColonyState state{};state.revision=rev;state.count=1;
  state.values[0]={1,17,30,rt::Metric::digital_learning,value,origin,quality,1,100,9};
  return state;
}
int main(){
  opt::WeightedGoalPolicy policy;
  opt::AdaptiveOptimizationService service(policy);
  Hook first,second;
  anaiero::services::ServiceExtensionDispatcher<2> hooks;
  assert(hooks.add(first)&&hooks.add(second)&&!hooks.add(first));
  service.set_extension(&hooks);
  opt::SyntheticTrialService trials;
  assert(trials.add({1,16,1})&&trials.add({2,19,2})&&
         trials.add({3,40,999})&&trials.add({4,-4,1}));
  assert(!trials.add({4,100,1}));
  Routes routes;
  opt::Goal goal{1,17,30,rt::Metric::digital_learning,75,3,2,0};
  opt::Decision chosen{};
  assert(service.analyze(sample(50),goal,routes,trials,chosen)==opt::Status::ok);
  assert(chosen.selected.id==2&&chosen.weighted_gain==19&&
         first.proposed==1&&second.proposed==1);
  opt::InMemoryDigitalRouteStore store(7);
  assert(service.commit(sample(50,8),chosen,store)==opt::Status::stale_revision);
  assert(service.commit(sample(50),chosen,store)==opt::Status::ok);
  assert(store.active(2)&&first.committed==1);
  assert(service.review(sample(50),chosen,store)==opt::Status::awaiting_observation);
  assert(service.review(sample(35,8),chosen,store)==opt::Status::reverted);
  assert(!store.active(2)&&first.reverted==1);
  assert(service.review(sample(35,9),chosen,store)==opt::Status::not_committed);
  assert(service.analyze(sample(50,7,rt::Quality::stale),goal,routes,trials,chosen)==opt::Status::unqualified);
  assert(service.analyze(sample(50,7,rt::Quality::current,
    rt::Origin::external_unverified_readonly),goal,routes,trials,chosen)==opt::Status::unqualified);
  struct TooMany:opt::ICandidateSource {
    std::size_t count()const noexcept override{return 17;}
    bool at(std::size_t,opt::Candidate&)const noexcept override{return false;}
  } too_many;
  assert(service.analyze(sample(50),goal,too_many,trials,chosen)==opt::Status::exhausted);
  assert(service.analyze(sample(50,9),goal,routes,trials,chosen)==opt::Status::ok);
  opt::InMemoryDigitalRouteStore store2(9);
  assert(service.commit(sample(50,9),chosen,store2)==opt::Status::ok);
  assert(service.review(sample(69,10),chosen,store2)==opt::Status::ok&&store2.active(2));
  rt::WeightedQueue<> runtime_queue;rt::RuntimeService runtime(runtime_queue);
  assert(runtime.enroll({30,1,rt::Origin::simulated,true,true,false})==rt::Status::ok);
  runtime.set_extension(&first);
  opt::OptimizationTriggerService trigger;runtime.set_observer(&trigger);
  rt::Observation obs{};obs.colony=1;obs.animon=17;obs.source=30;
  obs.clock_domain=1;obs.metric=rt::Metric::digital_learning;
  obs.origin=rt::Origin::simulated;obs.value=50;obs.sequence=1;obs.acquired_ns=100;
  assert(runtime.post(obs)==rt::Status::ok);
  runtime.drain();
  assert(first.dispatched==1&&trigger.take_pending_revision()==1);
  assert(trigger.take_pending_revision()==0);
  rt::ColonyProjector projector;projector.set_extension(&first);
  assert(projector.apply(obs)==rt::Status::ok&&first.projected==1);
  struct ReadingHook:contracts::IServiceExtension {
    rt::ColonyProjector* owner=nullptr;int calls=0;
    void on_service_event(const contracts::ServiceEvent&) noexcept override {
      if(owner->snapshot(100,1000).count==1)++calls;
    }
  } reader;
  reader.owner=&projector;projector.set_extension(&reader);
  obs.sequence=2;obs.acquired_ns=200;
  assert(projector.apply(obs)==rt::Status::ok&&reader.calls==1);
  struct Spatial:anaiero::cells::ISpatialPort {
    bool translate(const anaiero::cells::Vec3&,const anaiero::cells::Vec3& target,
                   anaiero::cells::Vec3& result)noexcept override{
      result=target;return true;
    }
  } spatial;
  anaiero::cells::Atlas atlas{};
  for(std::size_t i=0;i<anaiero::cells::kKinds;++i)
    atlas.by_kind[i].kind=static_cast<anaiero::cells::Kind>(i+1);
  anaiero::cells::CellBehaviorService cellService(atlas,spatial);
  cellService.set_extension(&first);
  assert(cellService.step({10,0,0,false})==anaiero::cells::Status::ok);
  assert(first.cells==1);
  std::cout<<"adaptive optimization tests pass\n";
}
