#pragma once
// Bounded, single-owner scheduler: O(Capacity) push and dispatch.
// Source+epoch FIFO heads keep the existing 8:5:3:2 weighted lane fairness.
// Hot path is allocation-free and does not poll or use any OS-specific APIs.
#include "anaiero/contracts/runtime_events.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
namespace anaiero::runtime {
template<std::size_t Capacity=kMaxWork>
class WeightedQueue final:public IDispatchQueue {
  static_assert(Capacity>0&&Capacity<=1024);
  static constexpr std::size_t kNone=Capacity;
  struct Slot {Observation event{};std::uint64_t inserted=0;
               std::size_t next=kNone;bool used=false;};
  struct SourceHead {
    std::uint32_t source=0;std::uint64_t epoch=0;
    std::size_t first=kNone,last=kNone;bool used=false;
  };
  std::array<Slot,Capacity> slots_{};
  std::array<SourceHead,Capacity> heads_{};
  std::size_t size_=0;
  std::uint64_t ordinal_=0;
  static constexpr std::array<std::int32_t,5> weights_{{0,8,5,3,2}};
  std::array<std::int32_t,5> credits_{};
public:
  std::size_t size()const noexcept override{return size_;}
  std::size_t max_size()const noexcept override{return Capacity;}
  static constexpr std::size_t capacity()noexcept{return Capacity;}
  Status push(const Observation& event) noexcept override {
    if(!valid_observation(event))return Status::invalid;
    if(size_==Capacity||ordinal_==std::numeric_limits<std::uint64_t>::max())
      return Status::queue_full;
    std::size_t slot=kNone,head=kNone,free_head=kNone;
    for(std::size_t i=0;i<Capacity;++i) {
      if(!slots_[i].used&&slot==kNone)slot=i;
      if(!heads_[i].used&&free_head==kNone)free_head=i;
      if(heads_[i].used&&heads_[i].source==event.source&&
         heads_[i].epoch==event.source_epoch)head=i;
    }
    if(slot==kNone)return Status::queue_full;
    if(head==kNone){if(free_head==kNone)return Status::queue_full;head=free_head;}
    slots_[slot]={event,ordinal_++,kNone,true};
    auto& h=heads_[head];
    if(!h.used)h={event.source,event.source_epoch,slot,slot,true};
    else{slots_[h.last].next=slot;h.last=slot;}
    ++size_;return Status::ok;
  }
  bool take(Observation& output) noexcept override {
    if(!size_)return false;
    std::array<bool,5> active{};
    std::array<std::size_t,5> best{};
    std::array<std::uint32_t,5> best_score{};
    for(std::size_t i=0;i<Capacity;++i) {
      if(!heads_[i].used)continue;
      const auto& s=slots_[heads_[i].first];
      const auto lane=static_cast<unsigned>(s.event.lane);
      const auto waited=ordinal_-s.inserted;
      const auto age=static_cast<std::uint32_t>(waited>256?256:waited);
      const auto score=static_cast<std::uint32_t>(s.event.urgency)*2u+
                       s.event.importance+age;
      if(!active[lane]||score>best_score[lane]||
         (score==best_score[lane]&&
          s.inserted<slots_[heads_[best[lane]].first].inserted)) {
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
        if(!found||credits_[lane]>credits_[chosen]||
           (credits_[lane]==credits_[chosen]&&best_score[lane]>best_score[chosen])){
          chosen=lane;found=true;
        }
      }
      if(!found)return false;
      credits_[chosen]-=sum;
    }
    auto& head=heads_[best[chosen]];
    const auto idx=head.first;
    const auto next=slots_[idx].next;
    output=slots_[idx].event;
    slots_[idx].used=false;slots_[idx].next=kNone;
    if(next==kNone)head={};
    else head.first=next;
    --size_;return true;
  }
};
} // namespace anaiero::runtime
