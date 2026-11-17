#pragma once
// Actual SW5 digital World forks; same baseline, stages and rule atlas for all options.
#include "anaiero/contracts/fork_optimization.hpp"
#include "anaiero/services/cell_behavior_service.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
namespace anaiero::optimization {
inline ForkMetrics extract_metrics(const cells::World& world) noexcept {
  const auto m=cells::metrics(world);
  return {m.viable,m.linked,m.total_energy,m.total_activity,
          m.total_learning,m.total_process};
}
inline std::int64_t metric_value(const ForkMetrics& m,Measure metric) noexcept {
  switch(metric){
    case Measure::viable:return m.viable;
    case Measure::directed_links:return m.links;
    case Measure::total_energy:return m.energy;
    case Measure::total_activity:return m.activity;
    case Measure::total_learning:return m.learning;
    case Measure::total_process:return m.process;
  }
  return -1;
}
inline bool valid_plan(const ForkPlan& plan) noexcept {
  if(!plan.objective_count||plan.objective_count>kForkMaxGoals||
     plan.horizon==0||plan.horizon>kForkMaxHorizon||plan.minimum_score<1||
     plan.minimum_score>100000000)return false;
  const auto& context=plan.trial_context;
  if(context.support_proxy<0||context.support_proxy>1000||
     context.stress_proxy<0||context.stress_proxy>1000||
     context.digital_signal<0||context.digital_signal>1000)return false;
  std::array<bool,7> found{};
  for(std::size_t i=0;i<plan.objective_count;++i){
    const auto& g=plan.objectives[i];
    const auto index=static_cast<unsigned>(g.metric);
    if(index==0||index>=found.size()||found[index]||!g.importance||
       g.importance>1000||g.ideal<0||g.ideal>1000000)return false;
    found[index]=true;
  }
  return true;
}
inline std::int64_t absdiff(std::int64_t a,std::int64_t b) noexcept {
  return a>=b?a-b:b-a;
}
class Sw5ForkOptimizationService {
  cells::Atlas atlas_{};
  cells::ISpatialPort& spatial_;
  bool simulate(const cells::World& initial,const ForkPlan& plan,
                const ForkOption* alternative,ForkMetrics& output) noexcept {
    cells::CellBehaviorService fork(atlas_,spatial_);
    if(fork.restore(initial)!=cells::Status::ok)return false;
    if(alternative){
      if(alternative->route.kind!=RouteKind::digital_signal||
         !alternative->guidance||alternative->guidance>1000||
         fork.encourage(alternative->route.from,alternative->route.to,
                        alternative->guidance)!=cells::Status::ok)return false;
    }
    for(std::uint16_t tick=0;tick<plan.horizon;++tick)
      if(fork.step(plan.trial_context)!=cells::Status::ok)return false;
    output=extract_metrics(fork.world());return true;
  }
public:
  Sw5ForkOptimizationService(cells::Atlas atlas,cells::ISpatialPort& spatial) noexcept
    :atlas_(std::move(atlas)),spatial_(spatial){}
  const cells::Atlas& atlas()const noexcept{return atlas_;}
  // No state mutation: all candidate trials are independent World copies.
  ForkStatus analyze(const cells::World& initial,std::uint64_t revision,
                     std::string_view initial_sha,
                     const ForkPlan& plan,std::span<const ForkOption> candidates,
                     ForkSelection& answer) noexcept {
    answer={};
    if(initial_sha.size()!=64||!valid_plan(plan)||candidates.empty()||
       candidates.size()>kForkMaxCandidates)return ForkStatus::invalid;
    if(!initial.cell_count||initial.cell_count>cells::kMaxCells||
       initial.link_count>cells::kMaxLinks||initial.intent_count>cells::kMaxIntents)
      return ForkStatus::invalid_world;
    ForkMetrics baseline{};
    if(!simulate(initial,plan,nullptr,baseline))return ForkStatus::invalid_world;
    const auto before=extract_metrics(initial);
    bool found=false;
    std::int64_t best_score=0;
    for(const auto& candidate:candidates){
      const auto& c=candidate.route;
      if(!c.id||!c.colony||!c.from||!c.to||c.from==c.to||
         c.kind!=RouteKind::digital_signal||candidate.guidance==0||
         candidate.guidance>1000||candidate.uncertainty>1000||
         c.digital_cost>1000||c.model_risk>1000)return ForkStatus::invalid;
      ForkMetrics after{};
      if(!simulate(initial,plan,&candidate,after))continue;
      bool eligible=true;
      std::int64_t score=0;
      for(std::size_t i=0;i<plan.objective_count;++i){
        const auto& goal=plan.objectives[i];
        const auto d_before=absdiff(metric_value(baseline,goal.metric),goal.ideal);
        const auto d_after=absdiff(metric_value(after,goal.metric),goal.ideal);
        if(goal.forbid_regression && d_after>d_before){eligible=false;break;}
        score+=static_cast<std::int64_t>(goal.importance)*(d_before-d_after)*100;
      }
      if(!eligible)continue;
      score-=static_cast<std::int64_t>(c.digital_cost)*10 +
        static_cast<std::int64_t>(c.model_risk)*20 +
        static_cast<std::int64_t>(candidate.uncertainty)*15;
      if(score<plan.minimum_score)continue;
      if(!found||score>best_score||
        (score==best_score&&c.id<answer.option.route.id)){
        found=true;best_score=score;
        answer.status=ForkStatus::ok;
        answer.option=candidate;answer.option.route.guidance_weight=candidate.guidance;
        answer.original=before;
        answer.baseline_forecast=baseline;answer.candidate_forecast=after;
        answer.snapshot_revision=revision;answer.score=score;
        answer.initial_world_sha256=std::string(initial_sha);
      }
    }
    return found?ForkStatus::ok:ForkStatus::no_improvement;
  }
  // Follow-up measurements are still synthetic. Return true only for digital
  // regression versus a prespecified baseline forecast, not causality.
  bool should_rollback(const ForkSelection& chosen,const ForkPlan& plan,
                       const cells::World& observed) const noexcept {
    if(chosen.status!=ForkStatus::ok||!valid_plan(plan))return false;
    const auto followup=extract_metrics(observed);
    for(std::size_t i=0;i<plan.objective_count;++i){
      const auto& goal=plan.objectives[i];
      const auto predicted_distance=absdiff(metric_value(chosen.baseline_forecast,goal.metric),goal.ideal);
      const auto actual_distance=absdiff(metric_value(followup,goal.metric),goal.ideal);
      if(goal.forbid_regression&&actual_distance>predicted_distance)return true;
    }
    return false;
  }
};
} // namespace anaiero::optimization
