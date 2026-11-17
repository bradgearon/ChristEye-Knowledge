#pragma once
// Application adapter maps a generic approved digital selection back into
// the current SW5 durable route transaction with the original state hash.
#include "anaiero/contracts/engine_optimization.hpp"
#include "anaiero/services/durable_digital_route_service.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace anaiero::application {
class Sw5EngineChangeAdapter final:public engine::IEngineChangePort {
  optimization::DurableDigitalRouteService& routes_;
  struct Choice {engine::Candidate candidate{};optimization::ForkOption option{};};
  std::array<Choice,8> choices_{};
  std::size_t count_=0;
public:
  explicit Sw5EngineChangeAdapter(
      optimization::DurableDigitalRouteService& routes) noexcept:routes_(routes){}
  bool add(engine::Candidate c,optimization::ForkOption opt) noexcept {
    if(!c.id||c.id!=opt.route.id||c.policy_token!=opt.route.id||
       c.cost!=opt.route.digital_cost||c.risk!=opt.route.model_risk||
       c.uncertainty!=opt.uncertainty||count_==choices_.size())return false;
    for(std::size_t i=0;i<count_;++i)
      if(choices_[i].candidate.id==c.id)return false;
    choices_[count_++]={c,opt};return true;
  }
  bool commit(const engine::Selection& selected,std::uint64_t& id) noexcept override {
    id=0;
    if(!selected.approved||selected.commit_id!=0||selected.score<=0||
       selected.forecast.source_revision==0||
       selected.forecast.source_state_sha256.size()!=64||
       selected.forecast.baseline.revision!=selected.forecast.source_revision)
      return false;
    for(std::size_t i=0;i<count_;++i){
      const auto& choice=choices_[i];
      const auto& c=selected.candidate;
      if(choice.candidate.id!=c.id||
         choice.candidate.policy_token!=c.policy_token||
         choice.candidate.cost!=c.cost||
         choice.candidate.risk!=c.risk||
         choice.candidate.uncertainty!=c.uncertainty)continue;
      optimization::ForkSelection native{};
      native.status=optimization::ForkStatus::ok;
      native.option=choice.option;
      native.option.route.guidance_weight=native.option.guidance;
      native.score=selected.score;
      native.snapshot_revision=selected.forecast.source_revision;
      native.initial_world_sha256=selected.forecast.source_state_sha256;
      return routes_.commit_selected(native,id);
    }
    return false;
  }
  bool revert(std::uint64_t id) noexcept override{return routes_.revert(id);}
};
} // namespace anaiero::application
