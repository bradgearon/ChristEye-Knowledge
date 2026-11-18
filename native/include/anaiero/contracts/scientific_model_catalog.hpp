#pragma once
// Descriptive registry only: declaring an engine is not loading or validating it.
#include "anaiero/contracts/scientific_simulation.hpp"
#include <array>
namespace anaiero::science {
struct ModelFamily {
  Provider provider;const char* name;const char* specialization;
  bool native_executable_bundled;bool model_validated;
};
inline constexpr std::array<ModelFamily,6> kModelFamilies{{
  {Provider::native_reference,"ANA-NATIVE","proxy features; passive voltage and scalar environment numerical models",true,false},
  {Provider::neuron,"NEURON","neuronal electrophysiology",false,false},
  {Provider::braincell,"BRAINCELL","NEURON-based nanoscale neurons/glia",false,false},
  {Provider::physicell,"PHYSICELL","multicellular agents and environments",false,false},
  {Provider::moose,"MOOSE","electrical and biochemical models",false,false},
  {Provider::steps,"STEPS","stochastic reaction-diffusion",false,false}
}};
} // namespace anaiero::science
