#pragma once
// SW7: multirate digital-only coupling of an electrical feature model and SW5.
// Scientific solvers and acquisition adapters can implement IElectricalFeatureModel,
// but their outputs must be independently qualified and model-specific.
#include "anaiero/services/hypothesis_runtime_service.hpp"
#include "anaiero/services/cell_behavior_service.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace anaiero::science {
class CoupledSimulationService final {
  static constexpr std::size_t kMaxFrames=128;
  cells::Atlas atlas_;
  cells::ISpatialPort& spatial_;
  IElectricalFeatureModel& electrical_;
  IEnvironmentFeatureModel* environment_=nullptr;
  ICellFramePublisher* publisher_=nullptr;
  IServiceSimulationHook* hook_=nullptr;
  State state_{};
  cells::World world_{};
  std::array<Frame,kMaxFrames> frames_{};
  std::size_t count_=0;
  bool initialized_=false,has_pending_=false;
  Frame pending_{};
  static bool valid(const Plan& p) noexcept {
    return static_cast<unsigned>(p.condition)>=1&&static_cast<unsigned>(p.condition)<=4&&
       p.train_ticks>0&&p.rest_ticks>0&&p.probe_ticks>0&&
       static_cast<unsigned>(p.train_ticks)+p.rest_ticks+p.probe_ticks<=kMaxFrames&&
       p.electrical_steps_per_tick>=1&&p.electrical_steps_per_tick<=16&&
       p.development_every>=1&&p.development_every<=16&&
       p.digital_signal>=0&&p.digital_signal<=kProxyMax;
  }
  Code emit(const Frame& f) noexcept {
    if(publisher_&&!publisher_->publish(f)){
      has_pending_=true;pending_=f;
      return Code::publishing_failed;
    }
    if(hook_)hook_->on_frame_committed(f);
    return Code::ok;
  }
public:
  CoupledSimulationService(cells::Atlas atlas,cells::ISpatialPort& spatial,
                           IElectricalFeatureModel& electrical) noexcept:
    atlas_(std::move(atlas)),spatial_(spatial),electrical_(electrical){}
  void set_publisher(ICellFramePublisher* p) noexcept {publisher_=p;}
  void set_environment(IEnvironmentFeatureModel* e) noexcept {environment_=e;}
  void set_hook(IServiceSimulationHook* h) noexcept {hook_=h;}
  Code initialize(const cells::World& world,State initial={}) noexcept {
    if(initial.epoch==0||initial.source==0||initial.tick!=0||
       initial.strength<0||initial.strength>kProxyMax||
       electrical_.descriptor().schema!=1)return Code::bad_input;
    cells::CellBehaviorService validation(atlas_,spatial_);
    if(validation.restore(world)!=cells::Status::ok||world.tick!=0)return Code::bad_input;
    world_=world;state_=initial;count_=0;initialized_=true;has_pending_=false;
    return Code::ok;
  }
  Code flush_pending() noexcept {
    if(!has_pending_)return Code::ok;
    if(publisher_&&!publisher_->publish(pending_))return Code::publishing_failed;
    if(hook_)hook_->on_frame_committed(pending_);
    has_pending_=false;
    return Code::ok;
  }
  Code step(const Plan& plan,Phase phase,std::uint16_t phase_tick) noexcept {
    if(!initialized_||!valid(plan)||count_>=kMaxFrames)return Code::bad_plan;
    if(has_pending_)return Code::publishing_failed;
    State next=state_;
    const std::int32_t signal=phase==Phase::train?
      ((phase_tick%3u==0u)?plan.digital_signal/2:plan.digital_signal):
      phase==Phase::probe?plan.digital_signal/3:0;
    const Input input{phase,signal};
    for(std::uint8_t f=0;f<plan.electrical_steps_per_tick;++f)
      if(!electrical_.advance(next,input,f))return Code::model_failure;
    if(environment_&&!environment_->advance(next,next.tick+1))return Code::model_failure;
    if(HypothesisRuntimeService::step(next,plan.condition,phase)!=Code::ok)
      return Code::model_failure;
    cells::World staged=world_;
    if((next.tick+1)%plan.development_every==0){
      cells::CellBehaviorService growth(atlas_,spatial_);
      if(growth.restore(staged)!=cells::Status::ok)return Code::model_failure;
      cells::Context context{};
      context.support_proxy=next.environment; // dimensionless proxy, not actual feeding
      context.digital_signal=next.activity;
      context.lesson_active=phase==Phase::train;
      if(growth.step(context)!=cells::Status::ok)return Code::model_failure;
      staged=growth.world();
    }
    ++next.tick;
    const auto metrics=cells::metrics(staged);
    Frame frame{};
    frame.epoch=next.epoch;frame.source=next.source;frame.tick=next.tick;
    frame.fast_steps=next.fast_steps;frame.phase=phase;
    frame.condition=plan.condition;frame.evidence=electrical_.descriptor().evidence;
    frame.activity=next.activity;frame.astro_state=next.astro_state;
    frame.environment=next.environment;
    if(environment_&&environment_->descriptor().evidence!=Evidence::computational_prototype)
      frame.evidence=Evidence::published_model_unverified;
    frame.eligibility=next.eligibility;
    frame.retention_proxy=next.strength;
    frame.learning_proxy=static_cast<std::int32_t>(
      std::clamp<std::int64_t>(metrics.total_learning,0,kProxyMax));
    frame.viable=metrics.viable;frame.candidate_links=metrics.linked;
    state_=next;world_=staged;frames_[count_++]=frame;
    return emit(frame);
  }
  Code run(const Plan& plan,Result& out) noexcept {
    if(!initialized_||!valid(plan)||state_.tick!=0)return Code::bad_plan;
    std::int64_t probe_sum=0;
    for(std::uint16_t i=0;i<plan.train_ticks;++i)
      if(const auto c=step(plan,Phase::train,i);c!=Code::ok)return c;
    for(std::uint16_t i=0;i<plan.rest_ticks;++i)
      if(const auto c=step(plan,Phase::rest,i);c!=Code::ok)return c;
    for(std::uint16_t i=0;i<plan.probe_ticks;++i){
      if(const auto c=step(plan,Phase::probe,i);c!=Code::ok)return c;
      probe_sum+=frames_[count_-1].retention_proxy;
    }
    const auto m=cells::metrics(world_);
    out={Code::ok,plan.condition,static_cast<std::uint32_t>(count_),
      state_.strength,static_cast<std::int32_t>(probe_sum/plan.probe_ticks),m.viable,m.linked};
    return Code::ok;
  }
  const State& state() const noexcept{return state_;}
  const cells::World& world() const noexcept{return world_;}
  std::size_t frame_count()const noexcept{return count_;}
  const Frame& frame(std::size_t i)const noexcept{return frames_[i];}
  static constexpr std::size_t capacity()noexcept{return kMaxFrames;}
};
} // namespace anaiero::science
