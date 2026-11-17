#pragma once
// Anaiero scientific simulation contracts. Digital models only, not instruments.
// Units in the native reference are arbitrary normalized proxy values.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace anaiero::science {
enum class Provider : std::uint8_t {
  native_reference=1, neuron, braincell, physicell, moose, steps
};
enum class Evidence : std::uint8_t {
  computational_prototype=1, published_model_unverified,
  research_hypothesis, independently_supported_prediction
};
enum class Condition : std::uint8_t {
  neuron_only=1, astrocyte_coupled, astrocyte_ablation, shuffled_glial_signal
};
enum class Phase : std::uint8_t {train=1,rest=2,probe=3};
enum class Channel : std::uint8_t {electrical_activity=1,environment_proxy=2};
enum class Code : std::uint8_t {
  ok, bad_plan, bad_input, capacity, model_failure, source_mismatch,
  publishing_failed, unsupported, incomplete
};
inline constexpr std::int32_t kProxyMax=1000;
struct ModelDescriptor {
  Provider provider=Provider::native_reference;
  Evidence evidence=Evidence::computational_prototype;
  std::uint32_t schema=1;
  std::uint32_t source_epoch=1;
  std::uint32_t source_id=61;
  std::string_view model_ref="native-proxy/1";
  std::string_view units="DIMENSIONLESS_PROXY";
};
struct Input {
  Phase phase=Phase::train;
  std::int32_t pattern_proxy=0; // purely digital input, never a waveform
};
struct Frame {
  std::uint32_t epoch=1,source=61;
  std::uint64_t tick=0,fast_steps=0;
  Phase phase=Phase::train;
  Condition condition=Condition::neuron_only;
  Evidence evidence=Evidence::computational_prototype;
  std::int32_t activity=0,astro_state=0,eligibility=0,environment=75;
  std::int32_t retention_proxy=0,learning_proxy=0;
  std::uint32_t viable=0,candidate_links=0;
};
struct State {
  std::uint32_t epoch=1,source=61;
  std::uint64_t tick=0,fast_steps=0;
  std::int32_t activity=0,astro_state=0,eligibility=0,environment=75;
  std::int32_t strength=120;
  std::uint32_t prng=0x5a17cafeu; // fixed seed in digital ablation control
};
struct Plan {
  Condition condition=Condition::neuron_only;
  std::uint16_t train_ticks=12,rest_ticks=12,probe_ticks=4;
  std::uint8_t electrical_steps_per_tick=4;
  std::uint8_t development_every=2;
  std::int32_t digital_signal=320;
};
struct Result {
  Code status=Code::ok;
  Condition condition=Condition::neuron_only;
  std::uint32_t steps=0;
  std::int32_t end_retention=0,average_probe=0;
  std::uint32_t viable=0,candidate_links=0;
};
struct IElectricalFeatureModel {
  virtual ~IElectricalFeatureModel()=default;
  virtual ModelDescriptor descriptor() const noexcept=0;
  // Frame input is normalized and must carry an explicit model/evidence label.
  // No physical instrument control or unscoped side effects.
  virtual bool advance(State&, const Input&, std::uint32_t fast_step) noexcept=0;
};
struct IEnvironmentFeatureModel {
  virtual ~IEnvironmentFeatureModel()=default;
  virtual ModelDescriptor descriptor() const noexcept=0;
  virtual bool advance(State&,std::uint64_t macro_tick) noexcept=0;
};
struct ICellFramePublisher {
  virtual ~ICellFramePublisher()=default;
  virtual bool publish(const Frame&) noexcept=0;
};
struct IServiceSimulationHook {
  virtual ~IServiceSimulationHook()=default;
  virtual void on_frame_committed(const Frame&) noexcept=0;
};
} // namespace anaiero::science
