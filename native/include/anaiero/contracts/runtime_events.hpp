#pragma once
// Bounded, fixed-width observability contract. No actuator methods exist here.
#include "anaiero/models/colony_state.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace anaiero::runtime {
inline constexpr std::uint32_t kObservationSchema=0xCE600020u;
inline constexpr std::uint32_t kRuntimeContractVersion=1;
struct Observation {
  std::uint32_t colony=0, animon=0, source=0, clock_domain=0;
  Metric metric=Metric::energy;
  Lane lane=Lane::telemetry;
  Origin origin=Origin::simulated;
  std::int32_t value=0;
  std::uint16_t urgency=0, importance=0; // 0..1000 digital queue hints
  std::uint64_t sequence=0, acquired_ns=0, evidence_tag=0;
};
struct SourcePermission {
  std::uint32_t source=0, clock_domain=0;
  Origin origin=Origin::simulated;
  bool can_read_health=false, can_publish_digital_desires=false, can_publish_fault=false;
};
enum class Status : std::uint8_t {
  ok, invalid, unauthorized, duplicate, out_of_order, queue_full,
  state_full, source_full, expired, malformed_wire, persistence_failed
};
inline constexpr bool health_metric(Metric m) noexcept {
  return m==Metric::viability || m==Metric::energy || m==Metric::support ||
         m==Metric::stress || m==Metric::activity || m==Metric::acquisition_health;
}
inline constexpr bool valid_metric(Metric m) noexcept {
  return static_cast<unsigned>(m)>=1 && static_cast<unsigned>(m)<=10;
}
inline constexpr bool valid_lane(Lane l) noexcept {return static_cast<unsigned>(l)<=4;}
inline constexpr bool valid_origin(Origin o) noexcept {return o==Origin::simulated||o==Origin::external_unverified_readonly;}
inline constexpr bool valid_observation(const Observation& o) noexcept {
  return o.colony!=0&&o.source!=0&&o.sequence!=0&&o.clock_domain!=0&&
         valid_metric(o.metric)&&valid_lane(o.lane)&&valid_origin(o.origin)&&
         o.urgency<=1000&&o.importance<=1000&&
         (o.metric!=Metric::digital_desire || o.lane==Lane::desire) &&
         (o.metric!=Metric::digital_learning || o.origin==Origin::simulated);
}
// Port is an infrastructural interface. Receipts are nonblocking notifications;
// all physical care, treatment, and stimulation remain out of scope.
struct IUpdateObserver {
  virtual ~IUpdateObserver()=default;
  virtual void on_revision(std::uint64_t revision, const Observation& update) noexcept=0;
};
struct IEventCommit {
  virtual ~IEventCommit()=default;
  virtual bool commit(const Observation& event) noexcept=0;
};
} // namespace anaiero::runtime
