#pragma once
// A fixed-volume, well-mixed scalar compartment. These are passive records;
// concentration and flux totals are numerical model output, not measurements.
#include <cstdint>
namespace anaiero::science {
struct EnvironmentCompartmentModel {
  std::int64_t initial_micromolar=100,inlet_max_micromolar=1000;
  std::uint64_t exchange_tau_ns=1000000000ULL;
  // First-order consumption rate in millionths per second (250000 = 0.25/s).
  std::uint64_t consumption_ppm_per_second=250000;
};
struct EnvironmentCompartmentState {
  std::uint64_t sequence=0;
  double concentration_micromolar=100;
};
// Integrated amount per fixed compartment volume over the most recent step.
// Every field has units of micromolar; residual is the floating-point closure
// error of before + supply - exchange_out - consumption - after.
struct EnvironmentBalance {
  double before=0,supply=0,exchange_out=0,consumption=0,after=0,residual=0;
};
} // namespace anaiero::science
