#pragma once
// Replays offline NEURON/BRAINCELL model-derived features. The trace does NOT
// respond to new inputs: comparisons using it are controlled playback only.
#include "anaiero/contracts/scientific_trace.hpp"
#include "anaiero/services/reference_electrical_service.hpp"
namespace anaiero::science {
class RecordedElectricalService final:public IElectricalFeatureModel {
  ScientificTrace trace_;
  std::uint16_t consumed_=0;
public:
  explicit RecordedElectricalService(ScientificTrace trace) noexcept:trace_(std::move(trace)){}
  ModelDescriptor descriptor()const noexcept override {
    return {trace_.provider,trace_.evidence,1,trace_.source_epoch,62,
      "pinned-offline-trace/1","DIMENSIONLESS_ACTIVITY_PROXY"};
  }
  bool advance(State& s,const Input& input,std::uint32_t) noexcept override {
    if(trace_.channel!=Channel::electrical_activity||input.pattern_proxy<0||input.pattern_proxy>kProxyMax||
       (trace_.provider!=Provider::neuron&&trace_.provider!=Provider::braincell&&
        trace_.provider!=Provider::native_reference))return false;
    if(consumed_>=trace_.count||s.epoch!=trace_.source_epoch)return false;
    s.activity=trace_.features[consumed_++];
    const auto eligibility_input=input.phase==Phase::train?s.activity/7:0;
    s.eligibility=proxy_bound(s.eligibility-s.eligibility/24+eligibility_input);
    ++s.fast_steps;return true;
  }
  std::size_t remaining()const noexcept{return trace_.count-consumed_;}
};
} // namespace anaiero::science
