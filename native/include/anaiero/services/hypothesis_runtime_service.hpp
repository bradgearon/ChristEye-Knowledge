#pragma once
// Digital astrocyte-coupling hypothesis; every option is a software ablation.
#include "anaiero/services/reference_electrical_service.hpp"
#include <algorithm>
namespace anaiero::science {
class HypothesisRuntimeService {
public:
  static Code step(State& s,Condition condition,Phase phase) noexcept {
    const auto c=static_cast<unsigned>(condition);
    if(c<1||c>4)return Code::bad_plan;
    // Slowly integrating synthetic astrocyte state. Not calcium concentration.
    s.astro_state=proxy_bound((7LL*s.astro_state+s.activity)/8);
    if(phase==Phase::train){
      s.strength=proxy_bound(s.strength + s.eligibility/16);
    } else {
      // Base decay in an untrained interval.
      const std::int32_t decay=14;
      std::int32_t stabilization=0;
      if(condition==Condition::astrocyte_coupled){
        stabilization=std::min<std::int32_t>(12,s.astro_state/15);
      } else if(condition==Condition::astrocyte_ablation){
        // The state was computed in training, but its effect is disabled later.
        stabilization=0;
      } else if(condition==Condition::shuffled_glial_signal){
        // Independent deterministic control, intentionally decoupled from activity.
        s.prng=s.prng*1664525u+1013904223u;
        stabilization=static_cast<std::int32_t>((s.prng>>25)&3u);
      }
      s.strength=proxy_bound(static_cast<std::int64_t>(s.strength)-decay+stabilization);
    }
    return Code::ok;
  }
};
} // namespace anaiero::science
