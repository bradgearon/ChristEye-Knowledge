#pragma once
// Deterministic single-owner, fixed-capacity digital priority scheduler.
// Dynamic urgency within lanes; weighted fairness across lanes.
#include "anaiero/contracts/runtime_events.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
namespace anaiero::runtime {
template<std::size_t Capacity=kMaxWork>
class WeightedQueue final: public IDispatchQueue {
  static_assert(Capacity>0 && Capacity<=1024);
  struct Slot {Observation event{};std::uint64_t inserted=0;bool used=false;};
  std::array<Slot,Capacity> slots_{};
  std::size_t size_=0;
  std::uint64_t ordinal_=0;
  // 8:5:3:2 nonfault work shares; urgent fault notifications preempt.
  static constexpr std::array<std::int32_t,5> weights_{{0,8,5,3,2}};
  std::array<std::int32_t,5> credits_{};
public:
  std::size_t size() const noexcept override{return size_;}
  std::size_t max_size() const noexcept override{return Capacity;}
  static constexpr std::size_t capacity() noexcept{return Capacity;}
  Status push(const Observation& event) noexcept override {
    if(!valid_observation(event))return Status::invalid;
    if(size_==Capacity)return Status::queue_full;
    for(auto& slot:slots_)if(!slot.used){slot={event,ordinal_++,true};++size_;return Status::ok;}
    return Status::queue_full;
  }
  // Preserve each source's event order even when a later event has more urgency.
  // Projection can therefore use monotonic per-source sequences safely.
  bool take(Observation& output) noexcept override {
    if(size_==0)return false;
    std::array<bool,5> active{};
    std::array<std::size_t,5> best{};
    std::array<std::uint32_t,5> best_score{};
    for(std::size_t i=0;i<Capacity;++i){
      if(!slots_[i].used)continue;
      const auto& s=slots_[i];
      bool blocked=false;
      for(std::size_t j=0;j<Capacity;++j){
        if(!slots_[j].used||i==j)continue;
        if(slots_[j].event.source==s.event.source&&
           slots_[j].event.source_epoch==s.event.source_epoch&&
           slots_[j].inserted<s.inserted){blocked=true;break;}
      }
      if(blocked)continue;
      const auto lane=static_cast<unsigned>(s.event.lane);
      const std::uint64_t waited=ordinal_-s.inserted;
      const auto age=static_cast<std::uint32_t>(waited>256?256:waited);
      const auto score=static_cast<std::uint32_t>(s.event.urgency)*2u+
                       s.event.importance+age;
      if(!active[lane]||score>best_score[lane] ||
         (score==best_score[lane]&&s.inserted<slots_[best[lane]].inserted)){
        active[lane]=true;best[lane]=i;best_score[lane]=score;
      }
    }
    unsigned chosen=0;
    if(!active[0]){
      bool found=false;
      std::int32_t sum=0;
      for(unsigned lane=1;lane<5;++lane){
        if(!active[lane]){credits_[lane]=0;continue;}
        credits_[lane]+=weights_[lane];sum+=weights_[lane];
        if(!found||credits_[lane]>credits_[chosen] ||
          (credits_[lane]==credits_[chosen]&&best_score[lane]>best_score[chosen])){
          chosen=lane;found=true;
        }
      }
      if(!found)return false;
      credits_[chosen]-=sum;
    }
    const auto index=best[chosen];
    output=slots_[index].event;slots_[index].used=false;--size_;
    return true;
  }
};
} // namespace anaiero::runtime
