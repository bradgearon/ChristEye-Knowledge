// Digital-only route search demo. No physical or network actuation.
#include "anaiero/services/adaptive_optimization_service.hpp"
#include "anaiero/services/synthetic_trial_service.hpp"
#include "anaiero/services/optimization_trigger_service.hpp"
#include "anaiero/infrastructure/in_memory_route_store.hpp"
#include "anaiero/services/runtime_service.hpp"
#include <array>
#include <iostream>
#include <string_view>
namespace opt=anaiero::optimization;
namespace rt=anaiero::runtime;
struct Routes final:opt::ICandidateSource {
  std::array<opt::Candidate,3> routes{{
    {101,1,17,20,opt::RouteKind::digital_signal,3,3},
    {102,1,17,21,opt::RouteKind::digital_learning,4,1},
    {103,1,17,22,opt::RouteKind::digital_learning,7,7}
  }};
  std::size_t count()const noexcept override{return routes.size();}
  bool at(std::size_t n,opt::Candidate& result)const noexcept override {
    if(n>=routes.size())return false;
    result=routes[n];return true;
  }
};
int main(int argc,char** argv){
  if(argc!=2||std::string_view(argv[1])!="demo"){
    std::cerr<<"usage: anaiero optimize demo (synthetic and in-memory only)\n";
    return 2;
  }
  rt::RuntimeService runtime;
  if(runtime.enroll({30,1,rt::Origin::simulated,true,true,false})!=rt::Status::ok)return 1;
  opt::OptimizationTriggerService trigger;
  runtime.set_observer(&trigger);
  rt::Observation event{};event.colony=1;event.animon=17;
  event.source=30;event.clock_domain=1;event.metric=rt::Metric::digital_learning;
  event.lane=rt::Lane::evidence;event.origin=rt::Origin::simulated;
  event.value=50;event.sequence=1;event.acquired_ns=1000000;
  if(runtime.post(event)!=rt::Status::ok)return 1;
  runtime.drain();
  if(!trigger.take_pending_revision())return 1;
  const auto snapshot=runtime.latest(1000000,1000000);
  opt::WeightedGoalPolicy policy;
  opt::AdaptiveOptimizationService service(policy);
  opt::SyntheticTrialService trials;
  if(!trials.add({101,9,2})||!trials.add({102,18,3})||
     !trials.add({103,25,600}))return 1;
  Routes candidates;
  opt::Goal goal{1,17,30,rt::Metric::digital_learning,75,3,2,0};
  opt::Decision chosen{};
  if(service.analyze(snapshot,goal,candidates,trials,chosen)!=opt::Status::ok)return 1;
  opt::InMemoryDigitalRouteStore store(snapshot.revision);
  if(service.commit(snapshot,chosen,store)!=opt::Status::ok)return 1;
  auto followup=snapshot;followup.revision++;followup.values[0].value=68;
  if(service.review(followup,chosen,store)!=opt::Status::ok)return 1;
  std::cout<<"{\"schema\":\"anaiero.optimization.demo/1\","
           <<"\"domain\":\"SYNTHETIC_ONLY\",\"baseline\":50,\"ideal\":75,"
           <<"\"winner_route\":"<<chosen.selected.id
           <<",\"predicted\":"<<chosen.predicted
           <<",\"synthetic_followup\":68,\"score\":"<<chosen.score
           <<",\"active\":"<<(store.active(chosen.selected.id)?"true":"false")
           <<"}\n";
}
