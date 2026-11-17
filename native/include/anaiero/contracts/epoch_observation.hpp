#pragma once
// SW6.2: epoch-scoped digital/read-only observation persistence.
#include "anaiero/contracts/runtime_events.hpp"
#include <array>
#include <cstddef>
#include <span>
namespace anaiero::runtime {
inline constexpr std::uint32_t kEpochObservationSchema=0xCE600041u;
inline constexpr std::size_t kEpochObservationPayload=72;
inline constexpr std::size_t kEpochObservationFrame=56+kEpochObservationPayload;
struct IObservationEpochReplay {
  virtual ~IObservationEpochReplay()=default;
  virtual bool accept(std::span<const std::byte> wire) noexcept=0;
};
struct IObservationEpochJournal {
  virtual ~IObservationEpochJournal()=default;
  virtual bool commit(const Observation&,std::span<const std::byte>) noexcept=0;
  virtual bool replay(IObservationEpochReplay&) noexcept=0;
};
struct IObservationEpochProjection {
  virtual ~IObservationEpochProjection()=default;
  virtual bool enroll(SourcePermission) noexcept=0;
  virtual bool can_accept(const Observation&) const noexcept=0;
  virtual bool apply(const Observation&) noexcept=0;
  virtual ColonyState latest() const noexcept=0;
};
} // namespace anaiero::runtime
