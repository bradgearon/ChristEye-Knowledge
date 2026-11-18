#pragma once
// SW6.2 service: bounded, ordered, journal-first epoch observation delivery.
// Distinct source epochs can restart sequence numbers without aliasing.
#include "anaiero/contracts/epoch_observation.hpp"
#include "anaiero/services/observation_checkpoint_service.hpp"
#include <array>
#include <cstddef>
namespace anaiero::runtime {
class EpochObservationDeliveryService final:private IObservationEpochReplay {
  struct Cursor {
    std::uint32_t source=0;
    std::uint64_t epoch=0,sequence=0;
    Observation last{};
  };
  IObservationEpochJournal& journal_;
  IObservationEpochProjection& projection_;
  IObservationEpochCodec& codec_;
  std::array<Cursor,kMaxSources> cursors_{};
  std::size_t count_=0;
  bool ready_=false,has_pending_=false;
  Observation pending_{};
  static bool same(const Observation& a,const Observation& b) noexcept {
    return a.source==b.source&&a.source_epoch==b.source_epoch&&
      a.sequence==b.sequence&&a.colony==b.colony&&a.animon==b.animon&&
      a.metric==b.metric&&a.lane==b.lane&&a.origin==b.origin&&
      a.clock_domain==b.clock_domain&&a.value==b.value&&
      a.urgency==b.urgency&&a.importance==b.importance&&
      a.evidence_tag==b.evidence_tag&&a.acquired_ns==b.acquired_ns;
  }
  Cursor* cursor(const Observation& o) noexcept {
    for(std::size_t i=0;i<count_;++i)
      if(cursors_[i].source==o.source&&cursors_[i].epoch==o.source_epoch)
        return &cursors_[i];
    return nullptr;
  }
  void record(const Observation& o,Cursor* existing) noexcept {
    if(existing){existing->sequence=o.sequence;existing->last=o;}
    else cursors_[count_++]={o.source,o.source_epoch,o.sequence,o};
  }
  bool supports_checkpoint()const noexcept override{return true;}
  bool restore(const ObservationReplayCheckpoint& saved)noexcept override {
    if(ready_||count_)return false;
    ObservationCheckpointService validation;
    if(!validation.restore(saved))return false;
    for(std::size_t i=0;i<saved.count;++i){const auto& last=saved.last[i];const auto& source=saved.state.cursors[i];
      if(!valid_observation(last)||last.source!=source.id||last.source_epoch!=source.epoch||
        last.sequence!=source.last_sequence||last.clock_domain!=source.clock_domain||last.acquired_ns!=source.last_acquired_ns)return false;
    }
    if(!projection_.restore(saved.state))return false;
    for(std::size_t i=0;i<saved.count;++i)record(saved.last[i],nullptr);
    return true;
  }
  bool accept(std::span<const std::byte> wire) noexcept override {
    Observation o{};
    if(!codec_.decode(wire,o))return false;
    auto* existing=cursor(o);
    if((existing&&o.sequence!=existing->sequence+1)||
       (!existing&&(count_==cursors_.size()||o.sequence!=1))||
       !projection_.apply(o))return false;
    record(o,existing);return true;
  }
public:
  EpochObservationDeliveryService(IObservationEpochCodec& codec,
    IObservationEpochJournal& journal,IObservationEpochProjection& projection) noexcept:
    journal_(journal),projection_(projection),codec_(codec){}
  bool recover() noexcept {
    if(ready_||!journal_.replay(*this))return false;
    ready_=true;return true;
  }
  bool submit(const Observation& o) noexcept {
    if(!ready_||o.source_epoch==0)return false;
    if(has_pending_){
      if(!same(o,pending_)||!projection_.apply(o))return false;
      has_pending_=false;
      record(o,cursor(o));
      return true;
    }
    auto* existing=cursor(o);
    if(existing&&o.sequence==existing->sequence)
      return same(o,existing->last)&&projection_.can_accept(o);
    if((existing&&o.sequence!=existing->sequence+1)||
       (!existing&&(count_==cursors_.size()||o.sequence!=1))||
       !projection_.can_accept(o))return false;
    std::array<std::byte,kEpochObservationFrame> frame{};
    if(!codec_.encode(o,frame)||!journal_.commit(o,frame))return false;
    if(!projection_.apply(o)){
      has_pending_=true;pending_=o;
      return false;
    }
    record(o,existing);return true;
  }
  std::size_t source_epochs()const noexcept{return count_;}
};
} // namespace anaiero::runtime
