#pragma once
// Multi-goal engine-neutral ranking. Does not know SW5, SIM3 or NEURON details.
#include "anaiero/contracts/engine_optimization.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
namespace anaiero::engine {
class EngineOptimizationService final {
  IScenarioEvaluator& evaluator_;
  contracts::IServiceExtension* observer_=nullptr;
  static std::int64_t measure(const Snapshot& s,ObjectiveMetric m)noexcept {
    switch(m){
      case ObjectiveMetric::participants:return s.participants;
      case ObjectiveMetric::connections:return s.connections;
      case ObjectiveMetric::energy:return s.energy;
      case ObjectiveMetric::learning_proxy:return s.learning_proxy;
    }
    return -1;
  }
  static std::int64_t distance(std::int64_t a,std::int64_t b)noexcept {
    if(a>1000000000LL||a< -1000000000LL||
       b>1000000000LL||b< -1000000000LL)return -1;
    return a>=b?a-b:b-a;
  }
public:
  explicit EngineOptimizationService(IScenarioEvaluator& evaluator)noexcept:
    evaluator_(evaluator){}
  void set_extension(contracts::IServiceExtension* hook) noexcept {observer_=hook;}
  bool analyze(const OptimizationPlan& plan,std::span<const Candidate> candidates,
               Selection& result) noexcept {
    result={};
    if(!plan.goal_count||plan.goal_count>plan.objectives.size()||
       !plan.horizon||plan.horizon>128||candidates.empty()||
       candidates.size()>8||plan.minimum_score<1)return false;
    std::array<bool,5> present{};
    for(std::size_t j=0;j<plan.goal_count;++j){
      const auto& o=plan.objectives[j];
      const auto index=static_cast<unsigned>(o.metric);
      if(index==0||index>=present.size()||present[index]||
         !o.importance||o.importance>1000)return false;
      present[index]=true;
    }
    for(const auto& candidate:candidates){
      if(!candidate.id||!candidate.policy_token||candidate.cost>1000||
         candidate.risk>1000||candidate.uncertainty>1000)return false;
      Forecast forecast{};
      if(!evaluator_.evaluate(plan,candidate,forecast)||!forecast.valid)continue;
      if(!forecast.baseline.id||forecast.baseline.id!=forecast.trial.id||
         forecast.baseline.source_epoch!=forecast.trial.source_epoch||
         forecast.baseline.tick>=forecast.trial.tick)return false;
      std::int64_t score=0;bool allowed=true;
      for(std::size_t k=0;k<plan.goal_count;++k){
        const auto& o=plan.objectives[k];
        const auto before=distance(measure(forecast.baseline,o.metric),o.ideal);
        const auto after=distance(measure(forecast.trial,o.metric),o.ideal);
        if(before<0||after<0)return false;
        if(o.forbid_regression&&after>before){allowed=false;break;}
        score+=(before-after)*o.importance;
      }
      if(!allowed)continue;
      score=score*100-static_cast<std::int64_t>(candidate.cost)*10-
        static_cast<std::int64_t>(candidate.risk)*20-
        static_cast<std::int64_t>(candidate.uncertainty)*15;
      if(score<plan.minimum_score)continue;
      if(!result.approved||score>result.score||
         (score==result.score&&candidate.id<result.candidate.id))
        result={candidate,forecast,score,0,true};
    }
    if(result.approved&&observer_)
      observer_->on_service_event({contracts::ServiceId::adaptive_optimization,
        contracts::ServicePhase::proposed,result.forecast.baseline.revision,
        result.candidate.id,0});
    return result.approved;
  }
  bool commit(Selection& result,IEngineChangePort& change) noexcept {
    if(!result.approved||result.commit_id||result.score<=0)return false;
    std::uint64_t id=0;
    if(!change.commit(result,id)||id==0)return false;
    result.commit_id=id;
    if(observer_)observer_->on_service_event({contracts::ServiceId::adaptive_optimization,
      contracts::ServicePhase::committed,result.forecast.baseline.revision,
      result.candidate.id,0});
    return true;
  }
};
} // namespace anaiero::engine
