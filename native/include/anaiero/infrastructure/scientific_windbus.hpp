#pragma once
// Digital-only projection of multirate features into existing WindPacket/1 +
// weighted SW4 ColonyState. No actual neural stimulation or sensors here.
#include "anaiero/contracts/scientific_simulation.hpp"
#include "anaiero/infrastructure/windbus_adapter.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace anaiero::science {
class ScientificWindBusPublisher final:public ICellFramePublisher {
  runtime::RuntimeService& runtime_;
  std::uint32_t source_=0,epoch_=0;
  std::uint64_t next_sequence_=1;
  std::uint64_t accepted_=0,finished_tick_=0,active_tick_=0;
  unsigned delivered_parts_=0;
  bool registered_=false;
  bool send(const Frame& f,runtime::Metric metric,std::int32_t value) noexcept {
    runtime::Observation o{};
    o.source=source_;o.colony=1;o.animon=3;o.clock_domain=3;
    o.origin=runtime::Origin::simulated;o.metric=metric;
    o.lane=runtime::health_metric(metric)?runtime::Lane::health:runtime::Lane::evidence;
    o.value=value;o.sequence=next_sequence_;o.acquired_ns=f.tick*1000000ULL;
    o.evidence_tag=(static_cast<std::uint64_t>(epoch_)<<32)|f.tick;
    o.importance=100;o.urgency=120;
    std::array<std::byte,runtime::kObservationFrame> packet{};
    std::size_t written=0;
    if(!runtime::encode_observation(o,packet,written))return false;
    if(runtime_.pending()>32)runtime_.drain(32);
    if(runtime::ingress_frame(runtime_,{packet.data(),written})!=runtime::Status::ok)return false;
    ++next_sequence_;
    ++accepted_;
    return true;
  }
public:
  explicit ScientificWindBusPublisher(runtime::RuntimeService& r) noexcept:runtime_(r){}
  bool enroll(std::uint32_t source,std::uint32_t epoch) noexcept {
    if(registered_||!source||!epoch)return false;
    if(runtime_.enroll({source,3,runtime::Origin::simulated,true,true,false})!=runtime::Status::ok)
      return false;
    registered_=true;source_=source;epoch_=epoch;return true;
  }
  bool publish(const Frame& f) noexcept override {
    if(!registered_||f.source!=source_||f.epoch!=epoch_||f.tick==0)return false;
    if(active_tick_==0){
      if(f.tick!=finished_tick_+1)return false;
      active_tick_=f.tick;delivered_parts_=0;
    }else if(f.tick!=active_tick_)return false;
    const std::array<runtime::Metric,3> metrics{{runtime::Metric::activity,
      runtime::Metric::digital_learning,runtime::Metric::connection}};
    const std::array<std::int32_t,3> values{{f.activity,f.learning_proxy,
      static_cast<std::int32_t>(f.candidate_links)}};
    for(;delivered_parts_<metrics.size();++delivered_parts_)
      if(!send(f,metrics[delivered_parts_],values[delivered_parts_]))return false;
    finished_tick_=active_tick_;active_tick_=0;delivered_parts_=0;
    return true;
  }
  void drain() noexcept {while(runtime_.pending())runtime_.drain(64);}
  std::uint64_t accepted()const noexcept{return accepted_;}
};
} // namespace anaiero::science
