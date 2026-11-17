#pragma once
// One deterministic SIM3 transition implementation for live and isolated runs.
#include "anaiero/models/sim3_state.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>
namespace anaiero::simulation {
class Sim3TransitionService final {
  contracts::IServiceExtension* extension_=nullptr;
  using Result=models::sim3::Result;
  using ConfigurationData=models::sim3::ConfigurationData;
  using Snapshot=models::sim3::Snapshot;
  using Colony=models::sim3::Colony;
  using Cell=models::sim3::Cell;
  using Channel=models::sim3::Channel;
  using ProcessKind=models::sim3::ProcessKind;
  using ModelLaws=models::sim3::ModelLaws;
  static constexpr auto kMaxCells=models::sim3::kMaxCells;
  static std::int32_t clamp(std::int64_t value,std::int32_t maximum=100)noexcept {
    return static_cast<std::int32_t>(std::max<std::int64_t>(0,std::min<std::int64_t>(maximum,value)));
  }
  static Colony* find_colony(Snapshot& st,const std::string& name){for(auto& c:st.colonies)if(c.id==name)return &c;return nullptr;}
  static Cell* find_cell(Snapshot& st,const std::string& col,std::uint32_t id){for(auto& c:st.cells)if(c.colony==col&&c.id==id)return &c;return nullptr;}
  static Result apply(const ConfigurationData& config,Snapshot& next){
    if(next.tick >= 10000)return Result::capacity;
    ++next.tick;
    // Policy-only synthetic processes; foreign_stress is not a pathogen model.
    for(const auto& p:config.processes){
      auto* col=find_colony(next,p.colony);if(!col)return Result::corruption;
      if(p.kind==ProcessKind::support && p.at>0 && next.tick%p.at==0)col->food=clamp(std::int64_t(col->food)+p.value);
      else if(p.kind==ProcessKind::foreign_stress && next.tick==p.at)col->stress=clamp(std::int64_t(col->stress)+p.value);
      else if(p.kind==ProcessKind::teach && next.tick==p.at){auto* cell=find_cell(next,p.colony,p.subject);
        if(cell&&cell->alive)cell->learning=clamp(std::int64_t(cell->learning)+p.value);
      }else if(p.kind==ProcessKind::divide && next.tick==p.at){
        if(next.cells.size()>=kMaxCells)return Result::capacity;
        auto* parent=find_cell(next,p.colony,p.subject);
        if(parent&&parent->alive && !find_cell(next,p.colony,p.value)){
          auto child=*parent;child.id=p.value;child.parent=parent->id;
          child.x+=1;child.y+=1;child.z+=1;child.energy=20;child.maturity=0;child.learning=0;
          child.birth=next.tick;child.death=0;child.alive=true;next.cells.push_back(child);
        }
      }
    }
    for(const auto& edge:config.links){
      auto* from=find_colony(next,edge.from);auto* to=find_colony(next,edge.to);
      if(!from||!to)return Result::corruption;
      if(edge.channel==Channel::food){const auto amount=std::min<int>(from->food,std::min<int>(static_cast<int>(edge.units),100-to->food));from->food-=amount;to->food+=amount;}
      else if(edge.channel==Channel::data)to->data=clamp(std::int64_t(to->data)+edge.units);
      else to->signal=clamp(std::int64_t(to->signal)+edge.units);
    }
    for(auto& col:next.colonies)if(col.stress>0)--col.stress;
    for(auto& c:next.cells){if(!c.alive)continue;
      auto* col=find_colony(next,c.colony);if(!col)return Result::corruption;
      const bool fed=col->food>0;
      if(fed)col->food=clamp(std::int64_t(col->food)-config.laws.food_per_cell);
      c.energy=clamp(std::int64_t(c.energy)+(fed?config.laws.fed_gain:-config.laws.unfed_cost)-
        (col->stress>config.laws.stress_cutoff?config.laws.stress_cost:config.laws.upkeep_cost));
      if(c.energy==0){c.alive=false;c.death=next.tick;continue;}
      if(fed)c.maturity=clamp(std::int64_t(c.maturity)+config.laws.maturity_gain);
      if(c.kind=="blood_carrier")c.x=static_cast<std::int32_t>((std::int64_t(c.x)+config.laws.blood_step)%256);
      if(c.kind=="neuron"&&col->data>0)c.learning=clamp(std::int64_t(c.learning)+config.laws.learning_gain);
    }
    return Result::ok;
  }
public:
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  static bool set_law(ModelLaws& laws,std::string_view name,std::uint32_t value) noexcept {
    if(value>100)return false;
    const auto n=static_cast<std::int32_t>(value);
    if(name=="food_per_cell")laws.food_per_cell=n;
    else if(name=="fed_gain")laws.fed_gain=n;
    else if(name=="unfed_cost")laws.unfed_cost=n;
    else if(name=="upkeep_cost")laws.upkeep_cost=n;
    else if(name=="stress_cutoff")laws.stress_cutoff=n;
    else if(name=="stress_cost")laws.stress_cost=n;
    else if(name=="maturity_gain")laws.maturity_gain=n;
    else if(name=="blood_step")laws.blood_step=n;
    else if(name=="learning_gain")laws.learning_gain=n;
    else return false;
    return true;
  }
  Result advance(const ConfigurationData& configuration,Snapshot& state)const noexcept {
    auto staged=state;const auto result=apply(configuration,staged);
    if(result==Result::ok){
      state=std::move(staged);
      if(extension_)extension_->on_service_event({contracts::ServiceId::cell_behavior,
        contracts::ServicePhase::committed,state.tick,0,0});
    }
    return result;
  }
};
} // namespace anaiero::simulation
