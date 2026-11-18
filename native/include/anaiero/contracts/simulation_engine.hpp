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
enum class SummarySemantic : std::uint8_t {
  unsupported=0,live_cells,active_cell_links,configured_colony_links,
  model_energy_proxy,learning_proxy,normalized_activity,
  membrane_voltage_microvolt,model_entities,concentration_micromolar
};
inline constexpr bool valid_summary_semantic(SummarySemantic value)noexcept {
  return static_cast<unsigned>(value)<=static_cast<unsigned>(SummarySemantic::concentration_micromolar);
}
// Versioned interpretation of the small compatibility Snapshot. A zero tick_ns
// declares logical time; a positive value declares simulated nanoseconds/tick.
struct Capabilities {
  bool isolated_fork=false,evaluate_candidates=false,apply_changes=false;
  std::uint32_t input_schema=0,metric_schema=0,checkpoint_schema=0,clock_domain=0;
  std::uint64_t tick_ns=0;
  std::array<SummarySemantic,4> summaries{}; // participants, connections, energy, learning
  std::string_view behavior_id{};
};
struct Descriptor {
  std::uint32_t id=0,source=0;
  std::uint64_t source_epoch=1;
  Kind kind=Kind::sw5;
  std::string_view model_digest{},classification="SYNTHETIC_UNCALIBRATED";
  bool supports_checkpoint=true,supports_restore=false;
  Capabilities capabilities{};
};
struct Inputs {
  std::int32_t support_proxy=0,stress_proxy=0,digital_signal=0;
  bool lesson=false;
};
struct StepRequest {
  std::uint64_t expected_tick=0;
  Inputs inputs{};
  // A coordinator retry preserves this value, the source identity and inputs.
  // Standalone adapters may leave it at zero; it is not a wall-clock timestamp.
  std::uint64_t controller_cycle=0;
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
struct IEngineInputSelector {
  virtual ~IEngineInputSelector()=default;
  // Resolve model-specific inputs before any engine in a cycle advances.
  // Selection must not mutate an engine; retry uses the recorded result.
  virtual Status select(const Descriptor&,std::uint64_t controller_cycle,
                        const Inputs& defaults,Inputs& selected) noexcept=0;
};
struct CoordinatorSlotProgress {
  std::uint32_t id=0,source=0;
  std::uint64_t source_epoch=0;
  Kind kind=Kind::sw5;
  std::uint8_t cadence=1;
  std::string model_digest{};
  Snapshot last{};
  Inputs cycle_inputs{};
  Status last_status=Status::not_initialized;
};
// Passive continuation data. Persistence belongs to an injected host/store.
// next_slot identifies the unfinished engine in an active controller cycle.
// A pending publication has already advanced that engine and records its exact
// snapshot; it must be acknowledged before next_slot can move forward.
struct CoordinatorProgress {
  std::uint32_t schema=1;
  std::uint64_t completed_cycles=0;
  std::uint8_t engine_count=0,next_slot=0;
  bool initialized=false,cycle_active=false,publication_pending=false;
  Inputs default_inputs{};
  Snapshot pending_snapshot{};
  std::array<CoordinatorSlotProgress,kMaxEngines> engines{};
};
struct EngineRuntimeStatus {
  Descriptor descriptor{};
  Snapshot last{};
  std::uint8_t cadence=1;
  bool initialized=false,due=false,publication_pending=false;
  Status last_status=Status::not_initialized;
};
} // namespace anaiero::engine
