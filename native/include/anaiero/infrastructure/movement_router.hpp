#pragma once
// Digital movement bridge. Synthetic transfer observations, never equipment IO.
#include "anaiero/contracts/movement.hpp"
#include "anaiero/infrastructure/windbus_adapter.hpp"
#include <array>
#include <cstddef>
namespace anaiero::runtime {
class ObservationMovementRouter final : public IMovementPort {
  RuntimeService& runtime_;
public:
  explicit ObservationMovementRouter(RuntimeService& r) noexcept : runtime_(r) {}
  Status submit(const Movement& m) noexcept override {
    if(!m.from_colony||!m.to_colony||!m.source||!m.clock_domain||!m.sequence||
       m.amount<0||m.amount>100000 ||
       static_cast<unsigned>(m.kind)<1||static_cast<unsigned>(m.kind)>4)
      return Status::invalid;
    Observation o{};
    o.colony=m.to_colony;o.animon=m.from_colony;o.source=m.source;
    o.clock_domain=m.clock_domain;o.metric=Metric::connection;
    o.lane=Lane::evidence;o.origin=Origin::simulated;
    o.value=m.amount;o.sequence=m.sequence;o.acquired_ns=m.timestamp_ns;
    o.evidence_tag=m.evidence_tag;o.importance=200;
    o.urgency=m.kind==MovementKind::digital_data?80:120;
    std::array<std::byte,kObservationFrame> buffer{};
    std::size_t n=0;
    if(!encode_observation(o,buffer,n))return Status::malformed_wire;
    return ingress_frame(runtime_,std::span<const std::byte>(buffer.data(),n));
  }
};
} // namespace anaiero::runtime
