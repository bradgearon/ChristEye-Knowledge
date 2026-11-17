#pragma once
// Exact analytic step of C dV/dt = -(V-E)/R + I. Model output is voltage;
// it is not a measured cell readout or a calibrated biological predictor.
#include "anaiero/models/passive_membrane.hpp"
#include <cmath>
#include <cstdint>
#include <string_view>
namespace anaiero::science {
inline constexpr std::string_view kPassiveAnalyticBehavior=
  "anaiero.passive-analytic/1;voltage_uv=rest_uv+R_ohm*I_pa/1000000+(old_uv-rest_uv-R_ohm*I_pa/1000000)*exp(-step_ns/tau_ns);I_pa=max_current_pa*input/1000;sample=llround(voltage_uv);uncertainty_uv=1";
inline constexpr std::string_view kNeuronPassiveBehavior=
  "anaiero.neuron-passive/1;neuron=9.0.1;section=L20,diam20,nseg1;g_pas=1/(R*area_cm2);cm=tau_ns*g_pas/1000;e_pas=rest_uv/1000;IClamp.amp=max_current_pa*input/1000000;fixed-dt;sample_uv=round(v*1000);uncertainty_uv=ceil(R*max_current_pa/1000000*step_ns/tau_ns)+1;checkpoint=SaveState";
class PassiveMembraneService final {
  PassiveMembraneModel model_{};
  PassiveMembraneState state_{};
  std::uint64_t step_ns_=0;
public:
  bool initialize(PassiveMembraneModel model,std::uint64_t step_ns)noexcept{
    if(model.rest_microvolt< -1000000||model.rest_microvolt>1000000||
       !model.tau_ns||model.tau_ns>1000000000000ULL||!model.resistance_ohm||
       model.resistance_ohm>10000000000ULL||model.max_current_picoamp>1000000||
       !step_ns||step_ns>1000000000ULL)return false;
    model_=model;state_={0,static_cast<double>(model.rest_microvolt)};step_ns_=step_ns;return true;
  }
  bool advance(std::uint64_t next,std::int32_t input)noexcept{
    if(!step_ns_||state_.sequence==UINT64_MAX||next!=state_.sequence+1||input<0||input>1000)return false;
    const double current=static_cast<double>(model_.max_current_picoamp)*input/1000.0;
    const double target=static_cast<double>(model_.rest_microvolt)+static_cast<double>(model_.resistance_ohm)*current/1000000.0;
    const double voltage=target+(state_.voltage_microvolt-target)*std::exp(-static_cast<double>(step_ns_)/static_cast<double>(model_.tau_ns));
    if(!std::isfinite(voltage)||voltage<INT32_MIN||voltage>INT32_MAX)return false;
    state_={next,voltage};return true;
  }
  bool restore(PassiveMembraneState value)noexcept{
    if(!step_ns_||!std::isfinite(value.voltage_microvolt)||value.voltage_microvolt<INT32_MIN||value.voltage_microvolt>INT32_MAX)return false;
    state_=value;return true;
  }
  PassiveMembraneState state()const noexcept{return state_;}
  std::int64_t voltage_microvolt()const noexcept{return static_cast<std::int64_t>(std::llround(state_.voltage_microvolt));}
};
} // namespace anaiero::science
