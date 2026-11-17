#pragma once
// SW7.1 -> SW6.2 bridge: four epoch-scoped observation events per coupled
// frame, preserving unfinished frame position when admission backpressures.
#include "anaiero/contracts/scientific_simulation.hpp"
#include "anaiero/services/epoch_observation_delivery_service.hpp"
#include <array>
#include <cstdint>
namespace anaiero::science {
class EpochScientificFramePublisher final:public ICellFramePublisher {
  runtime::EpochObservationDeliveryService& delivery_;
  std::uint32_t source_=0,epoch_=0;
  std::uint64_t sequence_=1,last_tick_=0,partial_tick_=0;
  std::size_t part_=0;
  static constexpr std::array<runtime::Metric,4> metrics_{{
    runtime::Metric::activity,runtime::Metric::digital_learning,
    runtime::Metric::connection,runtime::Metric::support}};
public:
  EpochScientificFramePublisher(runtime::EpochObservationDeliveryService& delivery,
    std::uint32_t source,std::uint32_t epoch) noexcept:
    delivery_(delivery),source_(source),epoch_(epoch){}
  bool publish(const Frame& frame) noexcept override {
    if(!source_||!epoch_||frame.source!=source_||frame.epoch!=epoch_||
       frame.tick==0)return false;
    if(partial_tick_==0){
      if(frame.tick!=last_tick_+1)return false;
      partial_tick_=frame.tick;part_=0;
    }
    if(frame.tick!=partial_tick_)return false;
    const std::array<std::int32_t,4> values{{frame.activity,
      frame.learning_proxy,static_cast<std::int32_t>(frame.candidate_links),
      frame.environment}};
    for(;part_<metrics_.size();++part_){
      runtime::Observation event{};
      event.colony=1;event.animon=3;event.source=source_;
      event.source_epoch=epoch_;event.clock_domain=3;
      event.origin=runtime::Origin::simulated;
      event.metric=metrics_[part_];
      event.lane=runtime::health_metric(event.metric)?
        runtime::Lane::health:runtime::Lane::evidence;
      event.value=values[part_];event.sequence=sequence_;
      event.acquired_ns=frame.tick*1000000ULL;
      event.evidence_tag=(static_cast<std::uint64_t>(epoch_)<<32)|frame.tick;
      event.urgency=120;event.importance=150;
      if(!delivery_.submit(event))return false;
      ++sequence_;
    }
    last_tick_=partial_tick_;partial_tick_=0;part_=0;return true;
  }
  std::uint64_t published_count()const noexcept{return sequence_-1;}
};
} // namespace anaiero::science
