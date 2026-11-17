#pragma once
// Portable, type-erased simulator lifecycle. Engine internals are NOT homogenized.
// Digital models only; cannot actuate instruments or living preparations.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
namespace anaiero::engine {
inline constexpr std::size_t kMaxEngines=8;
enum class Kind : std::uint8_t {sw5=1,sim3=2,scientific=3};
enum class Status : std::uint8_t {ok,invalid,not_initialized,tick_conflict,
  advance_failed,io_failure,unsupported,capacity,publish_failed};
struct Descriptor {
  std::uint32_t id=0,source=0;
  std::uint64_t source_epoch=1;
  Kind kind=Kind::sw5;
  std::string_view model_digest{},classification="SYNTHETIC_UNCALIBRATED";
  bool supports_checkpoint=true,supports_restore=false;
};
struct Inputs {
  std::int32_t support_proxy=0,stress_proxy=0,digital_signal=0;
  bool lesson=false;
};
struct StepRequest {
  std::uint64_t expected_tick=0;
  Inputs inputs{};
};
struct Snapshot {
  std::uint32_t id=0;
  std::uint64_t source_epoch=0,tick=0,revision=0;
  std::uint32_t participants=0,connections=0;
  std::int64_t energy=0,learning_proxy=0;
  std::array<char,65> state_sha256{};
};
struct ICheckpointWriter {
  virtual ~ICheckpointWriter()=default;
  virtual bool write(std::string_view complete_bytes) noexcept=0;
};
struct ICheckpointReader {
  virtual ~ICheckpointReader()=default;
  virtual std::string_view bytes() const noexcept=0;
};
struct ISimulationEngine {
  virtual ~ISimulationEngine()=default;
  virtual Descriptor describe() const noexcept=0;
  virtual Status initialize() noexcept=0;
  virtual Status advance(const StepRequest&,Snapshot&) noexcept=0;
  virtual Status latest(Snapshot&) const noexcept=0;
  virtual Status checkpoint(ICheckpointWriter&) const noexcept=0;
  virtual Status restore(ICheckpointReader&) noexcept=0;
};
struct ISnapshotPublisher {
  virtual ~ISnapshotPublisher()=default;
  virtual bool publish(const Snapshot&) noexcept=0;
};
} // namespace anaiero::engine
