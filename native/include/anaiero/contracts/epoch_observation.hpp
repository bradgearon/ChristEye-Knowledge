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
struct IObservationEpochCodec {
  virtual ~IObservationEpochCodec()=default;
  virtual bool encode(const Observation&,
    std::array<std::byte,kEpochObservationFrame>&) const noexcept=0;
  virtual bool decode(std::span<const std::byte>,Observation&) const noexcept=0;
};
struct ObservationReplayCheckpoint {
  ColonyState state{};
  std::array<Observation,kMaxSources> last{};
  std::size_t count=0;
};
struct IObservationSnapshotBuilder {
  virtual ~IObservationSnapshotBuilder()=default;
  virtual bool can_accept(const Observation&)const noexcept=0;
  virtual bool apply(const Observation&)noexcept=0;
  virtual bool restore(const ObservationReplayCheckpoint&)noexcept=0;
  virtual ObservationReplayCheckpoint snapshot()const noexcept=0;
};
struct IObservationEpochReplay {
  virtual ~IObservationEpochReplay()=default;
  virtual bool accept(std::span<const std::byte> wire) noexcept=0;
  virtual bool supports_checkpoint()const noexcept{return false;}
  virtual bool restore(const ObservationReplayCheckpoint&)noexcept{return false;}
};
struct ObservationJournalCapacity {
  bool known=false,sustained=false,writable=true;
  std::uint64_t available_events=UINT64_MAX,available_bytes=UINT64_MAX;
  std::uint64_t append_reservation_bytes=512*1024,total_events=0,resident_events=0,archived_segments=0;
};
struct IObservationEpochJournal {
  virtual ~IObservationEpochJournal()=default;
  virtual bool commit(const Observation&,std::span<const std::byte>) noexcept=0;
  virtual bool replay(IObservationEpochReplay&) noexcept=0;
  virtual ObservationJournalCapacity storage_capacity()const noexcept{return {};}
};
struct IObservationEpochProjection {
  virtual ~IObservationEpochProjection()=default;
  virtual bool enroll(SourcePermission) noexcept=0;
  virtual bool can_accept(const Observation&) const noexcept=0;
  virtual bool apply(const Observation&) noexcept=0;
  virtual ColonyState latest() const noexcept=0;
  virtual bool restore(const ColonyState&) noexcept{return false;}
};
} // namespace anaiero::runtime
