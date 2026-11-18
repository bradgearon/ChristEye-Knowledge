#pragma once
// Exact step for dC/dt = (Cin-C)/tau - k*C, with constant input during a step.
// This generic scalar model does not identify a chemical or validate biology.
#include "anaiero/models/environment_compartment.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
namespace anaiero::science {
inline constexpr std::string_view kEnvironmentAnalyticBehavior=
  "anaiero.environment-analytic/1;fixed-volume;C_um'=a*(Cin_um-C_um)-k*C_um;a=1000000000/tau_ns;k=consumption_ppm_per_second/1000000;Cin_um=inlet_max_um*input/1000;exact-exponential-step;monotone-lerp;balance=before+supply-exchange_out-consumption-after;sample=llround(C_um);numerical_envelope_um=1;max_steps=1000000";
class EnvironmentCompartmentService final {
  EnvironmentCompartmentModel model_{};
  EnvironmentCompartmentState state_{};
  EnvironmentBalance balance_{};
  std::uint64_t step_ns_=0;
  static constexpr std::uint64_t max_steps_=1000000;
public:
  static bool valid_model(const EnvironmentCompartmentModel& model)noexcept{
    return model.initial_micromolar>=0&&model.initial_micromolar<=1000000&&
      model.inlet_max_micromolar>=0&&model.inlet_max_micromolar<=1000000&&
      model.exchange_tau_ns>0&&model.exchange_tau_ns<=1000000000000ULL&&
      model.consumption_ppm_per_second<=1000000000ULL;
  }
  bool initialize(EnvironmentCompartmentModel model,std::uint64_t step_ns)noexcept{
    if(!valid_model(model)||!step_ns||step_ns>1000000000ULL)return false;
    model_=model;step_ns_=step_ns;
    state_={0,static_cast<double>(model.initial_micromolar)};
    balance_={state_.concentration_micromolar,0,0,0,state_.concentration_micromolar,0};
    return true;
  }
  bool advance(std::uint64_t next,std::int32_t input)noexcept{
    if(!step_ns_||state_.sequence>=max_steps_||next!=state_.sequence+1||input<0||input>1000)return false;
    const double seconds=static_cast<double>(step_ns_)/1000000000.0;
    const double exchange=1000000000.0/static_cast<double>(model_.exchange_tau_ns);
    const double consumption=static_cast<double>(model_.consumption_ppm_per_second)/1000000.0;
    const double rate=exchange+consumption;
    const double inlet=static_cast<double>(model_.inlet_max_micromolar)*static_cast<double>(input)/1000.0;
    const double target=exchange/rate*inlet;
    const double x=rate*seconds;
    const double gained=-std::expm1(-x);
    const double retained=std::exp(-x);
    const double before=state_.concentration_micromolar;
    // A direct sum of complementary exponentials can drift at an exact
    // equilibrium during many very short steps. Monotone interpolation keeps
    // equilibrium exact and the result within its endpoint concentrations.
    const double after=before>target?std::lerp(target,before,retained):std::lerp(before,target,gained);
    // phi = integral(exp(-rate*t),0,dt)/dt. Use a short series for
    // 1-phi near zero so a short step does not lose its influx integral.
    const double phi=gained/x;
    const double complement=x<0.0001?
      x*(0.5+x*(-1.0/6.0+x*(1.0/24.0+x*(-1.0/120.0+x/720.0)))):1.0-phi;
    const double integral=seconds*(before*phi+target*complement);
    EnvironmentBalance budget{before,exchange*inlet*seconds,exchange*integral,consumption*integral,after,0};
    budget.residual=budget.before+budget.supply-budget.exchange_out-budget.consumption-budget.after;
    const double maximum=static_cast<double>(std::max(model_.initial_micromolar,model_.inlet_max_micromolar));
    if(!std::isfinite(after)||after<0||after>maximum+0.00000001||
       !std::isfinite(budget.residual)||budget.supply<0||budget.exchange_out<0||budget.consumption<0)return false;
    state_={next,after};balance_=budget;return true;
  }
  bool restore(EnvironmentCompartmentState value)noexcept{
    const double maximum=static_cast<double>(std::max(model_.initial_micromolar,model_.inlet_max_micromolar));
    if(!step_ns_||value.sequence>max_steps_||!std::isfinite(value.concentration_micromolar)||
       value.concentration_micromolar<0||value.concentration_micromolar>maximum+0.00000001||
       (value.sequence==0&&value.concentration_micromolar!=static_cast<double>(model_.initial_micromolar)))return false;
    state_=value;
    // A recovered state has no preceding local step budget. This is an
    // identity budget, not a fabricated reconstruction of historical fluxes.
    balance_={value.concentration_micromolar,0,0,0,value.concentration_micromolar,0};
    return true;
  }
  EnvironmentCompartmentState state()const noexcept{return state_;}
  EnvironmentBalance balance()const noexcept{return balance_;}
  std::int64_t concentration_micromolar()const noexcept{
    return static_cast<std::int64_t>(std::llround(state_.concentration_micromolar));
  }
};
} // namespace anaiero::science
