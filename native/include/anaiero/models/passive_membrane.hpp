#pragma once
#include <cstdint>
namespace anaiero::science {
struct PassiveMembraneModel {
  std::int64_t rest_microvolt=-65000;
  std::uint64_t tau_ns=1000000,resistance_ohm=79577472,max_current_picoamp=20;
};
struct PassiveMembraneState {
  std::uint64_t sequence=0;
  double voltage_microvolt=-65000;
};
} // namespace anaiero::science
