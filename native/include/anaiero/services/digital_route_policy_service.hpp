#pragma once
// SW5 route guidance policy. Operates on a staged World copy; never on instruments.
#include "anaiero/contracts/fork_optimization.hpp"
#include "anaiero/services/cell_behavior_service.hpp"
#include <cstddef>
namespace anaiero::optimization {
class DigitalRoutePolicyService final {
  cells::Atlas atlas_{};
  cells::ISpatialPort& spatial_;
public:
  DigitalRoutePolicyService(cells::Atlas atlas,cells::ISpatialPort& spatial) noexcept
    :atlas_(std::move(atlas)),spatial_(spatial){}
  bool apply(const cells::World& source,const ForkOption& option,
             cells::World& changed,bool& existed,std::uint16_t& old_weight) noexcept {
    const auto& candidate=option.route;
    if(!candidate.id||candidate.kind!=RouteKind::digital_signal||
       !candidate.from||!candidate.to||candidate.from==candidate.to||
       !option.guidance||option.guidance>1000)return false;
    cells::CellBehaviorService check(atlas_,spatial_);
    if(check.restore(source)!=cells::Status::ok)return false;
    existed=false;old_weight=0;
    for(std::size_t i=0;i<source.intent_count;++i){
      const auto& intent=source.intents[i];
      if(intent.from==candidate.from&&intent.to==candidate.to){
        existed=true;old_weight=intent.guidance_weight;break;
      }
    }
    if(check.encourage(candidate.from,candidate.to,option.guidance)!=cells::Status::ok)
      return false;
    changed=check.world();
    // Existing SW5 service does not bump generation when updating an intent.
    if(existed&&old_weight!=option.guidance)++changed.generation;
    return true;
  }
  bool undo(const cells::World& source,const Candidate& candidate,
            bool existed,std::uint16_t original_weight,
            cells::World& result) noexcept {
    cells::CellBehaviorService check(atlas_,spatial_);
    if(check.restore(source)!=cells::Status::ok)return false;
    cells::World staged=source;
    bool found=false;
    for(std::size_t i=0;i<staged.intent_count;++i){
      auto& intent=staged.intents[i];
      if(intent.from!=candidate.from||intent.to!=candidate.to)continue;
      found=true;
      if(existed)intent.guidance_weight=original_weight;
      else {
        for(std::size_t j=i+1;j<staged.intent_count;++j)
          staged.intents[j-1]=staged.intents[j];
        --staged.intent_count;
        staged.intents[staged.intent_count]={};
      }
      break;
    }
    if(!found)return false;
    ++staged.generation;
    if(check.restore(staged)!=cells::Status::ok)return false;
    result=staged;return true;
  }
};
} // namespace anaiero::optimization
