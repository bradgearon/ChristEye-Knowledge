#pragma once
// Bounded, deterministic *computational* surrogate, not a NEURON cell model.
#include "anaiero/contracts/scientific_simulation.hpp"
#include <algorithm>
namespace anaiero::science {
inline std::int32_t proxy_bound(std::int64_t n) noexcept {
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(n,0,kProxyMax));
}
class ReferenceElectricalService final:public IElectricalFeatureModel {
public:
  ModelDescriptor descriptor() const noexcept override {return {};}
  bool advance(State& s,const Input& input,std::uint32_t) noexcept override {
    if(input.pattern_proxy<0||input.pattern_proxy>kProxyMax)return false;
    // Integer smoothing is deterministic across Windows/Linux; NOT membrane physics.
    s.activity=proxy_bound((3LL*s.activity+input.pattern_proxy)/4);
    const auto eligibility_input=input.phase==Phase::train?s.activity/7:0;
    s.eligibility=proxy_bound(s.eligibility-s.eligibility/24+eligibility_input);
    ++s.fast_steps;
    return true;
  }
};
} // namespace anaiero::science
