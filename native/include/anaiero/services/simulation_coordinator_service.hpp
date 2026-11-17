#pragma once
// One owner, bounded engine slots, injection-only engine dependencies.
// Publishing failure blocks subsequent steps until the exact pending event
// is retried; cross-engine atomic tick is not claimed.
#include "anaiero/contracts/simulation_engine.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace anaiero::engine {
class SimulationCoordinatorService final {
  struct Slot {
    ISimulationEngine* engine=nullptr;
    Descriptor desc{};
    Snapshot last{};
    std::uint8_t cadence=1;
    bool initialized=false;
  };
  std::array<Slot,kMaxEngines> slots_{};
  std::size_t count_=0;
  ISnapshotPublisher* publisher_=nullptr;
  bool pending_=false;
  Snapshot pending_snapshot_{};
  std::uint64_t cycles_=0;
public:
  bool register_engine(ISimulationEngine& engine,std::uint8_t cadence) noexcept {
    if(!cadence||cadence>64||count_==slots_.size()||cycles_)return false;
    const auto d=engine.describe();
    if(!d.id||!d.source||!d.source_epoch||d.model_digest.empty())return false;
    for(std::size_t i=0;i<count_;++i)
      if(slots_[i].desc.id==d.id||
         (slots_[i].desc.source==d.source&&
          slots_[i].desc.source_epoch==d.source_epoch))return false;
    slots_[count_++]={&engine,d,{},cadence,false};
    return true;
  }
  void set_publisher(ISnapshotPublisher* publisher) noexcept {publisher_=publisher;}
  Status initialize() noexcept {
    if(!count_||cycles_||pending_)return Status::invalid;
    for(std::size_t i=0;i<count_;++i) {
      if(const auto code=slots_[i].engine->initialize();code!=Status::ok)return code;
      if(const auto code=slots_[i].engine->latest(slots_[i].last);code!=Status::ok)
        return code;
      slots_[i].initialized=true;
    }
    return Status::ok;
  }
  Status flush_pending() noexcept {
    if(!pending_)return Status::ok;
    if(publisher_&&!publisher_->publish(pending_snapshot_))
      return Status::publish_failed;
    pending_=false;return Status::ok;
  }
  Status run_cycles(std::uint32_t requested,Inputs inputs={}) noexcept {
    if(!requested||requested>1024||pending_)return Status::invalid;
    for(std::size_t n=0;n<requested;++n) {
      if(cycles_==UINT64_MAX)return Status::capacity;
      const auto next=cycles_+1;
      for(std::size_t j=0;j<count_;++j) {
        auto& slot=slots_[j];
        if(!slot.initialized)return Status::not_initialized;
        if(next%slot.cadence)continue;
        Snapshot snap{};
        const auto status=slot.engine->advance({slot.last.tick,inputs},snap);
        if(status!=Status::ok)return status;
        slot.last=snap;
        if(publisher_&&!publisher_->publish(snap)) {
          pending_=true;pending_snapshot_=snap;cycles_=next;
          return Status::publish_failed;
        }
      }
      cycles_=next;
    }
    return Status::ok;
  }
  const Snapshot* latest(std::uint32_t id) const noexcept {
    for(std::size_t i=0;i<count_;++i)
      if(slots_[i].desc.id==id&&slots_[i].initialized)return &slots_[i].last;
    return nullptr;
  }
  std::uint64_t cycles()const noexcept{return cycles_;}
  std::size_t size()const noexcept{return count_;}
};
} // namespace anaiero::engine
