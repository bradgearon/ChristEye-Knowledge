#pragma once
// Engine-neutral optimization. Candidate meaning belongs to an injected adapter.
#include "anaiero/contracts/simulation_engine.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace anaiero::engine {
enum class ObjectiveMetric : std::uint8_t {
  participants=1,connections=2,energy=3,learning_proxy=4
};
struct Objective {
  ObjectiveMetric metric=ObjectiveMetric::learning_proxy;
  std::int64_t ideal=0;
  std::uint16_t importance=1;
  bool forbid_regression=false;
};
struct Candidate {
  std::uint32_t id=0,policy_token=0;
  std::uint16_t cost=0,risk=0,uncertainty=0;
};
struct Forecast {
  Snapshot baseline{},trial{};
  bool valid=false;
};
struct OptimizationPlan {
  std::array<Objective,4> objectives{};
  std::uint8_t goal_count=0;
  std::uint16_t horizon=0;
  std::int64_t minimum_score=1;
};
struct Selection {
  Candidate candidate{};
  Forecast forecast{};
  std::int64_t score=0;
  std::uint64_t commit_id=0;
  bool approved=false;
};
struct IScenarioEvaluator {
  virtual ~IScenarioEvaluator()=default;
  virtual bool evaluate(const OptimizationPlan&,const Candidate&,Forecast&) noexcept=0;
};
struct IEngineChangePort {
  virtual ~IEngineChangePort()=default;
  virtual bool commit(const Selection&,std::uint64_t& id) noexcept=0;
  virtual bool revert(std::uint64_t id) noexcept=0;
};
} // namespace anaiero::engine
