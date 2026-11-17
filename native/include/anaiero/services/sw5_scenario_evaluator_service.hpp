#pragma once
// SW5 scenario evaluation through the generic IScenarioEvaluator port.
// All forks start from the same immutable digital World and tick horizon.
// No filesystem, WindBus, external instruments, or physical cell operations.
#include "anaiero/contracts/engine_optimization.hpp"
#include "anaiero/contracts/fork_optimization.hpp"
#include "anaiero/services/cell_behavior_service.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
namespace anaiero::engine {
class Sw5ScenarioEvaluatorService final:public IScenarioEvaluator {
  struct Choice {Candidate policy{};optimization::ForkOption option{};};
  cells::Atlas atlas_{};
  cells::World initial_{};
  cells::ISpatialPort& spatial_;
  cells::Context context_{};
  std::array<Choice,8> choices_{};
  std::size_t count_=0;
  std::uint32_t engine_id_=0,source_id_=0;
  std::uint64_t epoch_=0,source_revision_=0;
  std::string source_hash_;
  bool simulate(std::uint16_t horizon,const optimization::ForkOption* trial,
                Snapshot& output) noexcept {
    cells::CellBehaviorService engine(atlas_,spatial_);
    if(engine.restore(initial_)!=cells::Status::ok)return false;
    if(trial&&engine.encourage(trial->route.from,trial->route.to,
                              trial->guidance)!=cells::Status::ok)return false;
    for(std::uint16_t i=0;i<horizon;++i)
      if(engine.step(context_)!=cells::Status::ok)return false;
    const auto data=cells::metrics(engine.world());
    output={};output.id=engine_id_;output.source_epoch=epoch_;
    output.tick=engine.world().tick;output.revision=source_revision_;
    output.participants=data.viable;output.connections=data.linked;
    output.energy=data.total_energy;output.learning_proxy=data.total_learning;
    return true;
  }
public:
  Sw5ScenarioEvaluatorService(cells::Atlas atlas,cells::World initial,
    cells::ISpatialPort& spatial,cells::Context context,
    std::uint32_t engine_id,std::uint32_t source_id,
    std::uint64_t epoch,std::uint64_t source_revision,
    std::string state_sha256) noexcept:
    atlas_(std::move(atlas)),initial_(initial),spatial_(spatial),context_(context),
    engine_id_(engine_id),source_id_(source_id),epoch_(epoch),
    source_revision_(source_revision),source_hash_(std::move(state_sha256)){}
  bool add(Candidate c,optimization::ForkOption option) noexcept {
    if(count_==choices_.size()||!c.id||c.policy_token!=option.route.id||
       c.id!=option.route.id||!option.route.colony||
       option.route.kind!=optimization::RouteKind::digital_signal||
       c.cost!=option.route.digital_cost||c.risk!=option.route.model_risk||
       c.uncertainty!=option.uncertainty||option.guidance==0||
       option.guidance>1000)return false;
    for(std::size_t i=0;i<count_;++i)
      if(choices_[i].policy.id==c.id)return false;
    choices_[count_++]={c,option};
    return true;
  }
  bool evaluate(const OptimizationPlan& plan,const Candidate& candidate,
                Forecast& forecast) noexcept override {
    forecast={};
    if(!atlas_.synthetic_only||!engine_id_||!source_id_||!epoch_||
       !source_revision_||source_hash_.size()!=64||plan.horizon==0||
       plan.horizon>128||initial_.tick>UINT64_MAX-plan.horizon||
       cells::validate_world_invariants(initial_)!=cells::Status::ok)
      return false;
    for(char c:source_hash_)
      if(!((c>='a'&&c<='f')||(c>='0'&&c<='9')))return false;
    const Choice* found=nullptr;
    for(std::size_t i=0;i<count_;++i)
      if(choices_[i].policy.id==candidate.id&&
         choices_[i].policy.policy_token==candidate.policy_token&&
         choices_[i].policy.cost==candidate.cost&&
         choices_[i].policy.risk==candidate.risk&&
         choices_[i].policy.uncertainty==candidate.uncertainty)
        found=&choices_[i];
    if(!found)return false;
    if(!simulate(plan.horizon,nullptr,forecast.baseline)||
       !simulate(plan.horizon,&found->option,forecast.trial))return false;
    if(forecast.baseline.tick!=forecast.trial.tick)return false;
    forecast.source_state_sha256=source_hash_;
    forecast.source_revision=source_revision_;forecast.valid=true;
    return true;
  }
};
} // namespace anaiero::engine
