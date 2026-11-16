#pragma once
// Typed transfers; infrastructure owns routing, models carry only data.
// No biological material movement or physical actuation is represented here.
#include "anaiero/contracts/runtime_events.hpp"
#include <cstdint>
namespace anaiero::runtime {
enum class MovementKind : std::uint8_t {
  synthetic_food=1, synthetic_signal=2, digital_data=3, synthetic_molecule=4
};
struct Movement {
  std::uint32_t from_colony=0, to_colony=0, source=0, clock_domain=0;
  MovementKind kind=MovementKind::digital_data;
  std::int32_t amount=0;
  std::uint64_t sequence=0, timestamp_ns=0, evidence_tag=0;
};
struct IMovementPort {
  virtual ~IMovementPort()=default;
  virtual Status submit(const Movement& transfer) noexcept=0;
};
} // namespace anaiero::runtime
