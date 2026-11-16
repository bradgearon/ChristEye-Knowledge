#pragma once
// Single-writer materialization service; concurrent bounded snapshot readers.
#include "anaiero/contracts/runtime_events.hpp"
#include <mutex>
namespace anaiero::runtime {
class ColonyProjector {
  mutable std::mutex mutex_;
  ColonyState state_{};
  static bool same_key(const MetricValue& a, const Observation& b) noexcept {
    return a.colony==b.colony&&a.animon==b.animon&&a.metric==b.metric&&a.source==b.source;
  }
public:
  // Single-owner admission check: do not append an impossible observation to
  // the durable evidence log. Apply must follow on the same writer owner.
  Status can_apply(const Observation& o) const noexcept {
    if(!valid_observation(o))return Status::invalid;
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t source_index=state_.sources;
    for(std::size_t i=0;i<state_.sources;++i)
      if(state_.cursors[i].id==o.source){source_index=i;break;}
    if(source_index==kMaxSources)return Status::source_full;
    if(source_index<state_.sources){
      const auto& cursor=state_.cursors[source_index];
      if(cursor.clock_domain!=o.clock_domain)return Status::unauthorized;
      if(cursor.has_sequence){
        if(o.sequence==cursor.last_sequence)return Status::duplicate;
        if(o.sequence<cursor.last_sequence||o.acquired_ns<cursor.last_acquired_ns)
          return Status::out_of_order;
      }
    }
    for(std::size_t i=0;i<state_.count;++i)if(same_key(state_.values[i],o))return Status::ok;
    return state_.count==kMaxMetrics?Status::state_full:Status::ok;
  }
  Status apply(const Observation& o) noexcept {
    if(!valid_observation(o))return Status::invalid;
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t source_index=state_.sources;
    for(std::size_t i=0;i<state_.sources;++i)
      if(state_.cursors[i].id==o.source){source_index=i;break;}
    if(source_index==kMaxSources)return Status::source_full;
    if(source_index<state_.sources){
      const auto& source=state_.cursors[source_index];
      if(source.clock_domain!=o.clock_domain)return Status::unauthorized;
      if(source.has_sequence){
        if(o.sequence==source.last_sequence)return Status::duplicate;
        if(o.sequence<source.last_sequence || o.acquired_ns<source.last_acquired_ns)
          return Status::out_of_order;
      }
    }
    std::size_t index=state_.count;
    for(std::size_t i=0;i<state_.count;++i)
      if(same_key(state_.values[i],o)){index=i;break;}
    if(index==kMaxMetrics)return Status::state_full;
    // New source+metric observations in different clock domains are not used
    // to infer biological causality. Source revision is the ordering authority.
    bool gap=false;
    if(source_index==state_.sources){
      state_.cursors[state_.sources++]={o.source,o.clock_domain,0,0,false,false};
    }
    auto& cursor=state_.cursors[source_index];
    if(cursor.has_sequence&&o.sequence>cursor.last_sequence+1){gap=true;cursor.gap=true;++state_.sequence_gaps;}
    cursor.has_sequence=true;
    cursor.last_sequence=o.sequence;cursor.last_acquired_ns=o.acquired_ns;
    if(index==state_.count)++state_.count;
    auto& m=state_.values[index];
    m={o.colony,o.animon,o.source,o.metric,o.value,o.origin,
       gap?Quality::degraded:Quality::current,o.sequence,o.acquired_ns,o.evidence_tag};
    ++state_.committed_updates;++state_.revision;
    return Status::ok;
  }
  // A copy under a short mutex hold gives readers a consistent revision;
  // callbacks, JSON and filesystem activity happen AFTER the lock is released.
  ColonyState snapshot(std::uint64_t now_ns,std::uint64_t max_age_ns) const noexcept {
    ColonyState copy;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      copy=state_;
    }
    for(std::size_t i=0;i<copy.count;++i){
      auto& m=copy.values[i];
      if(now_ns>=m.acquired_ns&&now_ns-m.acquired_ns>max_age_ns)
        m.quality=Quality::stale;
    }
    return copy;
  }
  std::uint64_t revision() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.revision;
  }
};
} // namespace anaiero::runtime
