#pragma once
// SW7.1: port for co-simulation with an optional external numerical solver.
// Requests are model-data only, never instrument or laboratory commands.
#include "anaiero/contracts/scientific_simulation.hpp"
#include <cstdint>
#include <string_view>
namespace anaiero::science {
inline constexpr std::uint64_t kLiveFastStepNs=25000;  // simulated 0.025ms
inline constexpr std::uint64_t kLiveSlowStepNs=100000;
struct LiveRequest {
  Channel channel=Channel::electrical_activity;
  std::uint32_t epoch=0;
  std::uint64_t sequence=0,step_ns=0;
  std::int32_t input_proxy=0;
  std::string_view model_sha256{};
};
struct LiveResponse {
  Channel channel=Channel::electrical_activity;
  std::uint32_t epoch=0;
  std::uint64_t sequence=0,elapsed_ns=0,uncertainty_ns=0;
  std::int32_t normalized_feature=0;
};
struct ILiveSolverExchange {
  virtual ~ILiveSolverExchange()=default;
  // Each accepted response must correspond to exactly one requested step.
  // A failed exchange irreversibly poisons the current solver session.
  virtual bool advance(const LiveRequest&,LiveResponse&) noexcept=0;
};
} // namespace anaiero::science
