#pragma once
// Digital simulation only. These interfaces contain NO equipment/actuator action.
#include "anaiero/models/cell_behavior.hpp"
#include <cstdint>
namespace anaiero::cells {
enum class Status : std::uint8_t {
  ok, invalid, duplicate, capacity, incompatible_lineage, not_ready,
  movement_rejected, conflict, bad_profile, corrupt_state, io_failure
};
enum class EventKind : std::uint8_t {
  developed=1, process_extended, linked, learning_proxy_changed,
  nonviable, division, movement, observation
};
struct Event {
  std::uint64_t tick=0;
  std::uint32_t cell=0,other=0;
  EventKind kind=EventKind::observation;
  std::int32_t value=0;
};
struct EventBatch {
  std::array<Event,256> values{};
  std::uint16_t count=0;
  std::uint16_t omitted=0;
};
// Low-level coordinate movement is provided by infrastructure, never a model.
struct ISpatialPort {
  virtual ~ISpatialPort()=default;
  virtual bool translate(const Vec3& start,const Vec3& desired,Vec3& result) noexcept=0;
};
struct IEventPort {
  virtual ~IEventPort()=default;
  virtual void published(const Event& event) noexcept=0;
};
inline constexpr bool is_neuron(Kind k) noexcept {
  return k==Kind::peripheral_neuron||k==Kind::central_neuron;
}
inline constexpr bool progenitor(Kind k) noexcept {
  return k==Kind::crest_progenitor||k==Kind::neural_progenitor;
}
inline constexpr bool can_transition(Kind from,Kind to) noexcept {
  if(from==Kind::crest_progenitor) return to==Kind::peripheral_neuron||to==Kind::schwann;
  if(from==Kind::neural_progenitor) return to==Kind::central_neuron||to==Kind::astrocyte;
  return false;
}
inline constexpr bool valid_kind(Kind kind) noexcept {
  return static_cast<unsigned>(kind)>=1&&static_cast<unsigned>(kind)<=kKinds;
}
} // namespace anaiero::cells
