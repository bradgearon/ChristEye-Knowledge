#pragma once
// Journal-owner projection mirror. It never writes files or notifies live state.
#include "anaiero/contracts/epoch_observation.hpp"
#include "anaiero/services/colony_projector.hpp"
#include <limits>
namespace anaiero::runtime {
class ObservationCheckpointService final:public IObservationSnapshotBuilder {
  ColonyProjector projector_;
  std::array<Observation,kMaxSources> last_{};
  std::size_t count_=0;
  static bool same(const Observation& a,const Observation& b)noexcept {
    return a.colony==b.colony&&a.animon==b.animon&&a.source==b.source&&a.clock_domain==b.clock_domain&&
      a.metric==b.metric&&a.lane==b.lane&&a.origin==b.origin&&a.value==b.value&&a.urgency==b.urgency&&
      a.importance==b.importance&&a.sequence==b.sequence&&a.acquired_ns==b.acquired_ns&&
      a.evidence_tag==b.evidence_tag&&a.source_epoch==b.source_epoch;
  }
public:
  bool can_accept(const Observation& o)const noexcept override {
    if(!valid_observation(o)||!o.source_epoch)return false;
    for(std::size_t i=0;i<count_;++i){const auto& last=last_[i];
      if(last.source!=o.source||last.source_epoch!=o.source_epoch)continue;
      if(last.sequence==o.sequence)return same(last,o);
      return last.sequence!=UINT64_MAX&&o.sequence==last.sequence+1&&projector_.can_apply(o)==Status::ok;
    }
    return count_<kMaxSources&&o.sequence==1&&projector_.can_apply(o)==Status::ok;
  }
  bool apply(const Observation& o)noexcept override {
    if(!can_accept(o))return false;
    std::size_t index=0;for(;index<count_;++index)if(last_[index].source==o.source&&last_[index].source_epoch==o.source_epoch)break;
    if(index<count_&&last_[index].sequence==o.sequence)return true;
    if(projector_.apply(o)!=Status::ok)return false;
    if(index==count_)++count_;
    last_[index]=o;return true;
  }
  bool restore(const ObservationReplayCheckpoint& saved)noexcept override {
    if(count_||saved.count>kMaxSources||saved.state.count>kMaxMetrics||saved.count!=saved.state.sources)return false;
    std::uint64_t updates=0;
    if(saved.state.sequence_gaps)return false;
    for(std::size_t i=0;i<saved.count;++i){const auto& last=saved.last[i];const auto& c=saved.state.cursors[i];
      if(!valid_observation(last)||last.source!=c.id||last.source_epoch!=c.epoch||last.sequence!=c.last_sequence||
        last.clock_domain!=c.clock_domain||last.acquired_ns!=c.last_acquired_ns||c.gap||
        updates>UINT64_MAX-last.sequence)return false;
      updates+=last.sequence;
      bool matches=false;for(std::size_t j=0;j<saved.state.count;++j){const auto& m=saved.state.values[j];
        if(m.source==last.source&&m.source_epoch==last.source_epoch&&m.colony==last.colony&&m.animon==last.animon&&
          m.metric==last.metric&&m.value==last.value&&m.sequence==last.sequence&&m.evidence_tag==last.evidence_tag&&
          m.acquired_ns==last.acquired_ns&&m.origin==last.origin)matches=true;}
      if(!matches)return false;
    }
    if(updates!=saved.state.revision||projector_.restore(saved.state)!=Status::ok)return false;
    count_=saved.count;last_=saved.last;return true;
  }
  ObservationReplayCheckpoint snapshot()const noexcept override {
    return {projector_.snapshot(UINT64_MAX,UINT64_MAX),last_,count_};
  }
};
} // namespace anaiero::runtime
