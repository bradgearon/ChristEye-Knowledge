#pragma once
// One owner, bounded engine slots, injection-only engine dependencies.
// A cycle keeps its engine cursor and resolved inputs until every due step and
// publication finishes. This is recoverable sequencing, not a global engine
// transaction. The host persists progress; replay-aware engine wrappers own
// durable step identity and may safely finish a step after a process restart.
#include "anaiero/contracts/simulation_engine.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace anaiero::engine {
class SimulationCoordinatorService final {
  struct Slot {
    ISimulationEngine* engine=nullptr;
    Descriptor desc{};
    Snapshot last{};
    Inputs cycle_inputs{};
    std::uint8_t cadence=1;
    bool initialized=false;
    Status status=Status::not_initialized;
  };
  std::array<Slot,kMaxEngines> slots_{};
  std::size_t count_=0,next_slot_=0;
  ISnapshotPublisher* publisher_=nullptr;
  IEngineInputSelector* input_selector_=nullptr;
  contracts::IServiceExtension* extension_=nullptr;
  bool initialized_=false,cycle_active_=false,pending_=false;
  Inputs default_inputs_{};
  Snapshot pending_snapshot_{};
  std::uint64_t cycles_=0;

  static bool same_snapshot(const Snapshot& a,const Snapshot& b) noexcept {
    return a.id==b.id&&a.source_epoch==b.source_epoch&&a.tick==b.tick&&
      a.revision==b.revision&&a.participants==b.participants&&
      a.connections==b.connections&&a.energy==b.energy&&
      a.learning_proxy==b.learning_proxy&&a.state_sha256==b.state_sha256;
  }
  static bool identity_matches(const Snapshot& s,const Descriptor& d) noexcept {
    return s.id==d.id&&s.source_epoch==d.source_epoch;
  }
  void event(contracts::ServicePhase phase,std::uint32_t subject,
             Status status=Status::ok) const noexcept {
    if(extension_)extension_->on_service_event({
      contracts::ServiceId::simulation_coordinator,phase,
      cycle_active_?cycles_+1:cycles_,subject,static_cast<std::int32_t>(status)});
  }
  void seek_due() noexcept {
    while(next_slot_<count_&&(cycles_+1)%slots_[next_slot_].cadence)
      ++next_slot_;
  }
  void finish_if_complete() noexcept {
    if(cycle_active_&&!pending_&&next_slot_==count_) {
      ++cycles_;cycle_active_=false;next_slot_=0;
      event(contracts::ServicePhase::committed,0);
    }
  }
  Status initialize_impl(bool reset) noexcept {
    if(!count_||initialized_||cycle_active_||pending_||cycles_)
      return Status::invalid;
    std::array<Snapshot,kMaxEngines> staged{};
    for(std::size_t i=0;i<count_;++i) {
      auto& slot=slots_[i];
      if(reset) {
        slot.status=slot.engine->initialize();
        if(slot.status!=Status::ok)return slot.status;
      }
      slot.status=slot.engine->latest(staged[i]);
      if(slot.status!=Status::ok)return slot.status;
      if(!identity_matches(staged[i],slot.desc)) {
        slot.status=Status::invalid;return slot.status;
      }
    }
    for(std::size_t i=0;i<count_;++i) {
      slots_[i].last=staged[i];slots_[i].initialized=true;
    }
    initialized_=true;
    event(contracts::ServicePhase::reviewed,0);
    return Status::ok;
  }
public:
  bool register_engine(ISimulationEngine& engine,std::uint8_t cadence) noexcept {
    if(!cadence||cadence>64||count_==slots_.size()||initialized_||
       cycle_active_||cycles_)return false;
    const auto d=engine.describe();
    if(!d.id||!d.source||!d.source_epoch||d.model_digest.empty())return false;
    for(std::size_t i=0;i<count_;++i)
      if(slots_[i].desc.id==d.id||
         (slots_[i].desc.source==d.source&&
          slots_[i].desc.source_epoch==d.source_epoch))return false;
    auto& slot=slots_[count_++];
    slot.engine=&engine;slot.desc=d;slot.cadence=cadence;
    return true;
  }
  void set_publisher(ISnapshotPublisher* publisher) noexcept {publisher_=publisher;}
  void set_input_selector(IEngineInputSelector* selector) noexcept {
    input_selector_=selector;
  }
  void set_extension(contracts::IServiceExtension* hook) noexcept {extension_=hook;}
  Status initialize() noexcept {return initialize_impl(true);}
  // Attach engines which their owner has already initialized/restored. Never
  // calls initialize(), so a recovered engine's checkpoint is not reset.
  Status initialize_from_latest() noexcept {return initialize_impl(false);}

  Status resume(const CoordinatorProgress& saved,
                bool allow_pending_step_replay=false) noexcept {
    if(cycle_active_||pending_||!count_||saved.schema!=1||!saved.initialized||
       saved.engine_count!=count_||saved.next_slot>count_||
       (!saved.cycle_active&&(saved.next_slot||saved.publication_pending))||
       (saved.cycle_active&&saved.completed_cycles==UINT64_MAX)||
       (saved.publication_pending&&saved.next_slot>=count_))return Status::invalid;
    for(std::size_t i=0;i<count_;++i) {
      const auto& slot=slots_[i];const auto& from=saved.engines[i];
      if(from.id!=slot.desc.id||from.source!=slot.desc.source||
         from.source_epoch!=slot.desc.source_epoch||from.kind!=slot.desc.kind||
         from.cadence!=slot.cadence||from.model_digest!=slot.desc.model_digest||
         !identity_matches(from.last,slot.desc)||
         static_cast<unsigned>(from.last_status)>
           static_cast<unsigned>(Status::publish_failed))return Status::invalid;
      Snapshot actual{};
      const auto status=slot.engine->latest(actual);
      if(status!=Status::ok)return status;
      if(!identity_matches(actual,slot.desc))return Status::invalid;
      const bool may_replay=allow_pending_step_replay&&saved.cycle_active&&
        !saved.publication_pending&&i==saved.next_slot&&
        (saved.completed_cycles+1)%slot.cadence==0&&from.last.tick!=UINT64_MAX;
      if(!same_snapshot(actual,from.last)&&
         !(may_replay&&actual.tick==from.last.tick+1))return Status::tick_conflict;
    }
    if(saved.cycle_active&&saved.next_slot<count_&&
       (saved.completed_cycles+1)%slots_[saved.next_slot].cadence)
      return Status::invalid;
    if(saved.publication_pending&&
       !same_snapshot(saved.pending_snapshot,saved.engines[saved.next_slot].last))
      return Status::invalid;
    // No mutation before all identity and progress checks succeed. Keep the
    // saved expected tick even when a durable engine is already one tick ahead;
    // its wrapper must replay the same request rather than advance twice.
    for(std::size_t i=0;i<count_;++i) {
      slots_[i].last=saved.engines[i].last;
      slots_[i].cycle_inputs=saved.engines[i].cycle_inputs;
      slots_[i].status=saved.engines[i].last_status;
      slots_[i].initialized=true;
    }
    cycles_=saved.completed_cycles;next_slot_=saved.next_slot;
    initialized_=true;cycle_active_=saved.cycle_active;
    pending_=saved.publication_pending;pending_snapshot_=saved.pending_snapshot;
    default_inputs_=saved.default_inputs;
    event(contracts::ServicePhase::reviewed,0);
    return Status::ok;
  }

  CoordinatorProgress progress() const {
    CoordinatorProgress result{};
    result.completed_cycles=cycles_;
    result.engine_count=static_cast<std::uint8_t>(count_);
    result.next_slot=static_cast<std::uint8_t>(next_slot_);
    result.initialized=initialized_;result.cycle_active=cycle_active_;
    result.publication_pending=pending_;result.default_inputs=default_inputs_;
    result.pending_snapshot=pending_snapshot_;
    for(std::size_t i=0;i<count_;++i) {
      const auto& slot=slots_[i];auto& out=result.engines[i];
      out.id=slot.desc.id;out.source=slot.desc.source;
      out.source_epoch=slot.desc.source_epoch;out.kind=slot.desc.kind;
      out.cadence=slot.cadence;out.model_digest=slot.desc.model_digest;
      out.last=slot.last;out.cycle_inputs=slot.cycle_inputs;
      out.last_status=slot.status;
    }
    return result;
  }

  Status begin_cycle(Inputs inputs={}) noexcept {
    if(!initialized_)return Status::not_initialized;
    if(cycle_active_)return Status::ok; // preserve resolved inputs on retry
    if(cycles_==UINT64_MAX)return Status::capacity;
    std::array<Inputs,kMaxEngines> staged{};
    for(std::size_t i=0;i<count_;++i) {
      auto& slot=slots_[i];
      if((cycles_+1)%slot.cadence)continue;
      staged[i]=inputs;
      if(input_selector_) {
        const auto status=input_selector_->select(slot.desc,cycles_+1,inputs,staged[i]);
        if(status!=Status::ok) {
          slot.status=status;event(contracts::ServicePhase::rejected,slot.desc.id,status);
          return status;
        }
      }
    }
    for(std::size_t i=0;i<count_;++i)slots_[i].cycle_inputs=staged[i];
    default_inputs_=inputs;cycle_active_=true;next_slot_=0;seek_due();
    event(contracts::ServicePhase::proposed,0);
    return Status::ok;
  }

  Status flush_pending() noexcept {
    if(!pending_)return Status::ok;
    if(!cycle_active_||next_slot_>=count_)return Status::invalid;
    auto& slot=slots_[next_slot_];
    if(publisher_&&!publisher_->publish(pending_snapshot_)) {
      slot.status=Status::publish_failed;
      event(contracts::ServicePhase::rejected,slot.desc.id,slot.status);
      return slot.status;
    }
    pending_=false;pending_snapshot_={};slot.status=Status::ok;
    ++next_slot_;seek_due();
    event(contracts::ServicePhase::committed,slot.desc.id);
    finish_if_complete();
    return Status::ok;
  }

  // Performs at most one engine step or one pending-publication retry. The host
  // can persist progress after begin_cycle(), then after each step_cycle().
  Status step_cycle(Inputs inputs={}) noexcept {
    if(const auto status=begin_cycle(inputs);status!=Status::ok)return status;
    if(pending_)return flush_pending();
    if(next_slot_==count_) {finish_if_complete();return Status::ok;}
    auto& slot=slots_[next_slot_];
    if(slot.last.tick==UINT64_MAX)return Status::capacity;
    Snapshot snap{};
    const auto status=slot.engine->advance(
      {slot.last.tick,slot.cycle_inputs,cycles_+1},snap);
    if(status!=Status::ok) {
      slot.status=status;event(contracts::ServicePhase::rejected,slot.desc.id,status);
      return status;
    }
    if(!identity_matches(snap,slot.desc)||snap.tick!=slot.last.tick+1) {
      slot.status=Status::invalid;
      event(contracts::ServicePhase::rejected,slot.desc.id,slot.status);
      return slot.status;
    }
    slot.last=snap;slot.status=Status::ok;
    pending_snapshot_=snap;pending_=true;
    return flush_pending();
  }

  Status run_cycles(std::uint32_t requested,Inputs inputs={}) noexcept {
    if(!requested||requested>1024||pending_)return Status::invalid;
    if(!initialized_)return Status::not_initialized;
    if(UINT64_MAX-cycles_<requested)return Status::capacity;
    const auto target=cycles_+requested;
    while(cycles_<target)
      if(const auto status=step_cycle(inputs);status!=Status::ok)return status;
    return Status::ok;
  }

  // An external digital route transaction can change state at the same tick.
  // Refresh only between cycles, and validate all snapshots before replacing
  // any. The session owner must persist the refreshed progress afterwards.
  Status refresh_latest() noexcept {
    if(!initialized_)return Status::not_initialized;
    if(cycle_active_||pending_)return Status::invalid;
    std::array<Snapshot,kMaxEngines> staged{};
    for(std::size_t i=0;i<count_;++i) {
      const auto status=slots_[i].engine->latest(staged[i]);
      if(status!=Status::ok)return status;
      if(!identity_matches(staged[i],slots_[i].desc)||
         staged[i].tick!=slots_[i].last.tick)return Status::tick_conflict;
    }
    for(std::size_t i=0;i<count_;++i)slots_[i].last=staged[i];
    event(contracts::ServicePhase::reviewed,0);
    return Status::ok;
  }
  bool inspect(std::uint32_t id,EngineRuntimeStatus& result) const noexcept {
    for(std::size_t i=0;i<count_;++i) {
      const auto& slot=slots_[i];if(slot.desc.id!=id)continue;
      result={slot.desc,slot.last,slot.cadence,slot.initialized,
        cycle_active_&&i>=next_slot_&&(cycles_+1)%slot.cadence==0,
        pending_&&next_slot_==i,slot.status};
      return true;
    }
    return false;
  }
  const Snapshot* latest(std::uint32_t id) const noexcept {
    for(std::size_t i=0;i<count_;++i)
      if(slots_[i].desc.id==id&&slots_[i].initialized)return &slots_[i].last;
    return nullptr;
  }
  std::uint64_t cycles()const noexcept{return cycles_;}
  std::size_t size()const noexcept{return count_;}
  bool cycle_active()const noexcept{return cycle_active_;}
  bool publication_pending()const noexcept{return pending_;}
};
} // namespace anaiero::engine
