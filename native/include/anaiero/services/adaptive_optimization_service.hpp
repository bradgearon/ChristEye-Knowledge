#pragma once
// All scoring, eligibility and rollback decisions live in this service.
#include "anaiero/contracts/optimization.hpp"
#include <cstdint>
namespace anaiero::optimization {
inline std::int64_t deviation(std::int32_t measured,std::int32_t ideal) noexcept {
  const auto delta=static_cast<std::int64_t>(measured)-ideal;
  return delta<0?-delta:delta;
}
class WeightedGoalPolicy final : public IScorePolicy {
public:
  std::int64_t score(const Goal& goal,std::int32_t baseline,
                    const Candidate& c,const TrialResult& trial) const noexcept override {
    const auto gain=deviation(baseline,goal.ideal)-deviation(trial.predicted,goal.ideal);
    return gain*static_cast<std::int64_t>(goal.importance)*100-
      static_cast<std::int64_t>(c.digital_cost)*10-
      static_cast<std::int64_t>(c.model_risk)*20-
      static_cast<std::int64_t>(trial.uncertainty)*15;
  }
};
class AdaptiveOptimizationService final {
  IScorePolicy& scoring_;
  contracts::IServiceExtension* extension_=nullptr;
  static constexpr std::size_t kMaxTrials=16;
  static bool valid_goal(const Goal& g) noexcept {
    return g.colony&&g.source&&runtime::valid_metric(g.metric)&&g.importance>=1&&
      g.importance<=1000&&g.minimum_gain>0&&g.minimum_gain<=1000&&
      g.allowed_regression<=1000;
  }
  static bool valid_candidate(const Candidate& c,const Goal& goal) noexcept {
    return c.id&&c.colony==goal.colony&&c.from&&c.to&&c.from!=c.to&&
      static_cast<unsigned>(c.kind)>=1&&static_cast<unsigned>(c.kind)<=3&&
      c.digital_cost<=1000&&c.model_risk<=1000;
  }
  static Status observe(const runtime::ColonyState& state,const Goal& goal,
                        std::int32_t& value) noexcept {
    bool found=false;
    for(std::size_t i=0;i<state.count;++i){
      const auto& v=state.values[i];
      if(v.colony!=goal.colony||v.animon!=goal.animon||
         v.source!=goal.source||v.metric!=goal.metric)continue;
      if(found)return Status::invalid;
      if(v.origin!=runtime::Origin::simulated ||
         v.quality!=runtime::Quality::current)return Status::unqualified;
      value=v.value;found=true;
    }
    return found?Status::ok:Status::missing_metric;
  }
  void notify(contracts::ServicePhase phase,const Decision& d,Status code) noexcept {
    if(extension_)extension_->on_service_event({contracts::ServiceId::adaptive_optimization,
      phase,d.snapshot_revision,d.selected.id,static_cast<std::int32_t>(code)});
  }
public:
  explicit AdaptiveOptimizationService(IScorePolicy& scoring) noexcept:scoring_(scoring){}
  void set_extension(contracts::IServiceExtension* extension) noexcept {extension_=extension;}
  Status analyze(const runtime::ColonyState& current,const Goal& goal,
                 const ICandidateSource& candidates,ITrialEvaluator& evaluator,
                 Decision& selected) noexcept {
    selected={};
    if(!valid_goal(goal)||current.count>runtime::kMaxMetrics)return Status::invalid;
    std::int32_t baseline=0;
    if(auto s=observe(current,goal,baseline);s!=Status::ok)return s;
    if(candidates.count()==0)return Status::no_improvement;
    if(candidates.count()>kMaxTrials)return Status::exhausted;
    Decision best{};
    for(std::size_t i=0;i<candidates.count();++i){
      Candidate route{};
      if(!candidates.at(i,route)||!valid_candidate(route,goal))return Status::invalid;
      TrialResult trial{};
      if(!evaluator.dry_run(current,goal,route,trial)||!trial.valid||
         trial.uncertainty>1000)continue;
      const auto gain=deviation(baseline,goal.ideal)-
                      deviation(trial.predicted,goal.ideal);
      if(gain<goal.minimum_gain)continue;
      const auto score=scoring_.score(goal,baseline,route,trial);
      // Policy plug-ins may rank alternatives but cannot bypass the gain gate.
      if(score<=0)continue;
      if(best.status!=Status::ok||score>best.score||
         (score==best.score&&route.id<best.selected.id))
        best={Status::ok,goal,route,current.revision,0,baseline,
              trial.predicted,gain,score,false};
    }
    if(best.status!=Status::ok)return Status::no_improvement;
    selected=best;
    notify(contracts::ServicePhase::proposed,selected,Status::ok);
    return Status::ok;
  }
  Status commit(const runtime::ColonyState& current,Decision& decision,
                IRouteChangePort& changes) noexcept {
    if(decision.status!=Status::ok||decision.committed)return Status::invalid;
    if(current.revision!=decision.snapshot_revision)return Status::stale_revision;
    std::int32_t baseline=0;
    if(auto s=observe(current,decision.goal,baseline);s!=Status::ok)return s;
    if(baseline!=decision.baseline)return Status::stale_revision;
    std::uint64_t token=0;
    if(!changes.commit(decision,token)||token==0){
      notify(contracts::ServicePhase::rejected,decision,Status::commit_failed);
      return Status::commit_failed;
    }
    decision.commit_id=token;decision.committed=true;
    notify(contracts::ServicePhase::committed,decision,Status::ok);
    return Status::ok;
  }
  Status review(const runtime::ColonyState& later,Decision& decision,
                IRouteChangePort& changes) noexcept {
    if(!decision.committed||decision.commit_id==0)return Status::not_committed;
    if(later.revision<=decision.snapshot_revision)return Status::awaiting_observation;
    std::int32_t value=0;
    if(auto s=observe(later,decision.goal,value);s!=Status::ok)return s;
    const auto change=deviation(decision.baseline,decision.goal.ideal)-
                      deviation(value,decision.goal.ideal);
    // Digital regression check, not a causal interpretation of a living cell.
    if(change+static_cast<std::int64_t>(decision.goal.allowed_regression)<
       decision.goal.minimum_gain){
      if(!changes.revert(decision.commit_id))return Status::rollback_failed;
      decision.committed=false;
      notify(contracts::ServicePhase::reverted,decision,Status::reverted);
      return Status::reverted;
    }
    notify(contracts::ServicePhase::reviewed,decision,Status::ok);
    return Status::ok;
  }
};
} // namespace anaiero::optimization
