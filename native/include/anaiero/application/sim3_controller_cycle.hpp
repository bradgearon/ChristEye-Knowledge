#pragma once
// Authorized *digital* tick integration. All movement and learning remain
// simulated; this port cannot address physical equipment.
#include "anaiero/contracts/host_runtime.hpp"
#include "anaiero/infrastructure/sim3_ingress.hpp"
#include "christeye/living/colony_runner.hpp"
namespace anaiero::application {
class Sim3ControllerCycle final:public host::IDigitalCyclePort {
  christeye::living::sim3::Runner& runner_;
  runtime::Sim3Ingress& adapter_;
  runtime::IObservationRuntime& runtime_;
public:
  Sim3ControllerCycle(christeye::living::sim3::Runner& runner,
    runtime::Sim3Ingress& adapter,runtime::IObservationRuntime& runtime)noexcept:
    runner_(runner),adapter_(adapter),runtime_(runtime){}
  bool advance_one_simulated_tick() noexcept override {
    if(runner_.step(false)!=christeye::living::sim3::Result::ok)return false;
    if(adapter_.import(runner_.configuration(),runner_.state())!=runtime::Status::ok)
      return false;
    while(runtime_.pending())runtime_.drain(64);
    return runtime_.pending()==0;
  }
};
} // namespace anaiero::application
