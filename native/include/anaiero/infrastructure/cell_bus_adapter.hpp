#pragma once
// Synthetic behavior receipts -> existing WindPacket/ColonyState projector.
// No actuator interface and no biological source authentication here.
#include "anaiero/contracts/cell_behavior.hpp"
#include "anaiero/infrastructure/windbus_adapter.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace anaiero::cells {
class CellBusAdapter final : public IEventPort {
  anaiero::runtime::IObservationRuntime& bus_;
  std::uint64_t sequence_=0, failures_=0;
public:
  explicit CellBusAdapter(anaiero::runtime::IObservationRuntime& bus) noexcept:bus_(bus){}
  anaiero::runtime::Status enroll() noexcept {
    return bus_.enroll({30,1,anaiero::runtime::Origin::simulated,true,true,false});
  }
  void published(const Event& event) noexcept override {
    using namespace anaiero::runtime;
    Metric metric{};
    switch(event.kind){
      case EventKind::developed:metric=Metric::viability;break;
      case EventKind::nonviable:metric=Metric::viability;break;
      case EventKind::linked:metric=Metric::connection;break;
      case EventKind::learning_proxy_changed:metric=Metric::digital_learning;break;
      default:return; // movement/shape events have a separate timeline contract
    }
    Observation observation{};
    observation.colony=1;observation.animon=event.cell;observation.source=30;
    observation.clock_domain=1;observation.metric=metric;
    observation.origin=Origin::simulated;observation.sequence=++sequence_;
    observation.acquired_ns=event.tick*1000000ULL;
    observation.evidence_tag=event.tick;
    observation.value=event.kind==EventKind::nonviable?0:
      event.kind==EventKind::linked||event.kind==EventKind::learning_proxy_changed?
        event.value:1;
    observation.lane=metric==Metric::digital_learning?Lane::evidence:Lane::health;
    observation.urgency=event.kind==EventKind::nonviable?900:250;
    observation.importance=250;
    std::array<std::byte,kObservationFrame> frame{};
    std::size_t written=0;
    if(!encode_observation(observation,frame,written)) {++failures_;return;}
    if(bus_.pending()>kMaxWork/2)bus_.drain(kMaxWork/2);
    const auto status=ingress_frame(bus_,std::span<const std::byte>(frame.data(),written));
    if(status!=anaiero::runtime::Status::ok)++failures_;
  }
  std::uint64_t failures()const noexcept{return failures_;}
};
} // namespace anaiero::cells
