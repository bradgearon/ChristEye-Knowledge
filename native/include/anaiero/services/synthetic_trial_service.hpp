#pragma once
// Toy synthetic trial fixture. No physiological prediction is implied.
#include "anaiero/contracts/optimization.hpp"
#include <array>
#include <cstddef>
namespace anaiero::optimization {
struct SyntheticTrial {std::uint32_t route_id=0;std::int32_t expected_delta=0;
  std::uint16_t uncertainty=0;};
class SyntheticTrialService final : public ITrialEvaluator {
  std::array<SyntheticTrial,16> trials_{};
  std::size_t count_=0;
public:
  bool add(SyntheticTrial t) noexcept {
    if(!t.route_id||t.uncertainty>1000||count_==trials_.size())return false;
    for(std::size_t i=0;i<count_;++i)if(trials_[i].route_id==t.route_id)return false;
    trials_[count_++]=t;return true;
  }
  bool dry_run(const runtime::ColonyState& state,const Goal& goal,
               const Candidate& candidate,TrialResult& out) noexcept override {
    out={};
    for(std::size_t i=0;i<state.count;++i){
      const auto& v=state.values[i];
      if(v.colony!=goal.colony||v.source!=goal.source||v.animon!=goal.animon||
         v.metric!=goal.metric||v.quality!=runtime::Quality::current||
         v.origin!=runtime::Origin::simulated)continue;
      for(std::size_t j=0;j<count_;++j)if(trials_[j].route_id==candidate.id){
        const auto proposed=static_cast<std::int64_t>(v.value)+
                             trials_[j].expected_delta;
        if(proposed<0||proposed>1000)return false;
        out={true,static_cast<std::int32_t>(proposed),trials_[j].uncertainty};
        return true;
      }
      return false;
    }
    return false;
  }
};
} // namespace anaiero::optimization
