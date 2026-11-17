#pragma once
// Project any authorized digital or *unverified read-only* observation
// stream, including source restarts. Does not control any physical device.
#include "anaiero/contracts/epoch_observation.hpp"
#include "anaiero/infrastructure/epoch_observation_codec.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
namespace anaiero::runtime {
class EpochRuntimeProjectionPort final:public IObservationEpochProjection {
  IObservationRuntime& runtime_;
  EpochObservationCodec codec_{};
  std::array<SourcePermission,kMaxSources> sources_{};
  std::size_t enrolled_=0;
public:
  explicit EpochRuntimeProjectionPort(IObservationRuntime& runtime) noexcept:
    runtime_(runtime){}
  bool enroll(SourcePermission permission) noexcept override {
    if(enrolled_==sources_.size()||permission.source_epoch!=0)return false;
    if(runtime_.enroll(permission)!=Status::ok)return false;
    sources_[enrolled_++]=permission;return true;
  }
  ColonyState latest()const noexcept override {
    return runtime_.latest(std::numeric_limits<std::uint64_t>::max(),
                           std::numeric_limits<std::uint64_t>::max());
  }
  bool restore(const ColonyState& state) noexcept override {
    if(state.sources>kMaxSources||state.count>kMaxMetrics)return false;
    for(std::size_t i=0;i<state.sources;++i){const auto& c=state.cursors[i];bool enrolled=false;
      for(std::size_t j=0;j<enrolled_;++j)if(sources_[j].source==c.id&&sources_[j].clock_domain==c.clock_domain)enrolled=true;
      if(!enrolled)return false;
    }
    for(std::size_t i=0;i<state.count;++i){const auto& m=state.values[i];bool allowed=false;
      for(std::size_t j=0;j<enrolled_;++j)if(sources_[j].source==m.source&&sources_[j].origin==m.origin&&
        (!health_metric(m.metric)||sources_[j].can_read_health)&&
        (m.metric!=Metric::digital_desire||sources_[j].can_publish_digital_desires)&&
        (m.metric!=Metric::digital_learning||m.origin==Origin::simulated))allowed=true;
      if(!allowed)return false;
    }
    return runtime_.restore(state)==Status::ok;
  }
  bool can_accept(const Observation& observation) const noexcept override {
    if(observation.source_epoch==0||!valid_observation(observation))return false;
    bool source_ok=false;
    for(std::size_t i=0;i<enrolled_;++i){
      const auto& p=sources_[i];
      if(p.source==observation.source&&p.clock_domain==observation.clock_domain&&
         p.origin==observation.origin){source_ok=true;break;}
    }
    if(!source_ok)return false;
    const auto status=runtime_.can_accept(observation);
    if(status==Status::ok)return true;
    if(status!=Status::duplicate)return false;
    const auto view=runtime_.latest(observation.acquired_ns,
      std::numeric_limits<std::uint64_t>::max());
    for(std::size_t i=0;i<view.count;++i){
      const auto& v=view.values[i];
      if(v.source==observation.source&&v.source_epoch==observation.source_epoch&&
         v.sequence==observation.sequence&&v.colony==observation.colony&&
         v.animon==observation.animon&&v.metric==observation.metric&&
         v.value==observation.value&&v.evidence_tag==observation.evidence_tag)
        return true;
    }
    return false;
  }
  bool apply(const Observation& observation) noexcept override {
    if(!can_accept(observation))return false;
    if(observation.source_epoch==0||!valid_observation(observation))return false;
    bool source_ok=false;
    for(std::size_t i=0;i<enrolled_;++i) {
      const auto& p=sources_[i];
      if(p.source==observation.source&&p.clock_domain==observation.clock_domain&&
         p.origin==observation.origin){source_ok=true;break;}
    }
    if(!source_ok)return false;
    auto is_visible=[&]() noexcept {
      const auto snapshot=runtime_.latest(observation.acquired_ns,
            std::numeric_limits<std::uint64_t>::max());
      for(std::size_t i=0;i<snapshot.count;++i){
        const auto& v=snapshot.values[i];
        if(v.source==observation.source&&v.source_epoch==observation.source_epoch&&
           v.colony==observation.colony&&v.animon==observation.animon&&
           v.metric==observation.metric&&v.sequence==observation.sequence&&
           v.value==observation.value&&v.evidence_tag==observation.evidence_tag)
          return true;
      }
      return false;
    };
    if(is_visible())return true;
    std::array<std::byte,kEpochObservationFrame> frame{};
    Observation decoded{};
    if(!codec_.encode(observation,frame)||!codec_.decode(frame,decoded))return false;
    if(runtime_.post(decoded)!=Status::ok)return false;
    for(unsigned i=0;i<4&&runtime_.pending();++i)
      runtime_.drain(64);
    return is_visible();
  }
};
} // namespace anaiero::runtime
