#pragma once
#include "anaiero/contracts/child_process.hpp"
#include "anaiero/models/scientific_quantity.hpp"
#include <cstdint>
#include <string>
#include <string_view>
namespace anaiero::science {
struct SolverIdentity {
  std::string model_sha256,behavior_sha256,provider;
  std::uint64_t epoch=0,step_ns=0,max_steps=0;
  Unit unit=Unit::microvolt;
  std::uint32_t checkpoint_schema=0;
};
struct SolverSample {
  std::uint64_t sequence=0,elapsed_ns=0;
  std::int64_t value=0,uncertainty=0;
};
struct ScientificConfiguration {
  process::Specification process;
  SolverIdentity expected;
  std::uint32_t timeout_ms=2000;
  std::int32_t input_proxy=0;
  std::string configuration_sha256;
};
struct ISupervisedSolver {
  virtual ~ISupervisedSolver()=default;
  virtual bool initialize(const SolverIdentity&)=0;
  virtual bool step(std::uint64_t expected_sequence,std::int32_t input,SolverSample&)noexcept=0;
  virtual bool checkpoint(std::string&)noexcept=0;
  virtual bool restore(std::string_view,SolverSample&)noexcept=0;
  virtual bool latest(SolverSample&)const noexcept=0;
  virtual bool failed()const noexcept=0;
  virtual void cancel()noexcept=0;
};
} // namespace anaiero::science
