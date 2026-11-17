#pragma once
// Digital simulation only. These interfaces contain NO equipment/actuator action.
#include "anaiero/models/cell_behavior.hpp"
#include <cstdint>
#include <algorithm>
#include <string_view>
namespace anaiero::cells {
enum class Status : std::uint8_t {
  ok, invalid, duplicate, capacity, incompatible_lineage, not_ready,
  movement_rejected, conflict, bad_profile, corrupt_state, io_failure
};
enum class EventKind : std::uint8_t {
  developed=1, process_extended, linked, learning_proxy_changed,
  nonviable, division, movement, observation
};
struct Event {
  std::uint64_t tick=0;
  std::uint32_t cell=0,other=0;
  EventKind kind=EventKind::observation;
  std::int32_t value=0;
};
struct EventBatch {
  std::array<Event,256> values{};
  std::uint16_t count=0;
  std::uint16_t omitted=0;
};
// Low-level coordinate movement is provided by infrastructure, never a model.
struct ISpatialPort {
  virtual ~ISpatialPort()=default;
  virtual bool translate(const Vec3& start,const Vec3& desired,Vec3& result) noexcept=0;
};
struct IEventPort {
  virtual ~IEventPort()=default;
  virtual void published(const Event& event) noexcept=0;
};
inline constexpr bool is_neuron(Kind k) noexcept {
  return k==Kind::peripheral_neuron||k==Kind::central_neuron;
}
inline constexpr bool progenitor(Kind k) noexcept {
  return k==Kind::crest_progenitor||k==Kind::neural_progenitor;
}
inline constexpr bool can_transition(Kind from,Kind to) noexcept {
  if(from==Kind::crest_progenitor) return to==Kind::peripheral_neuron||to==Kind::schwann;
  if(from==Kind::neural_progenitor) return to==Kind::central_neuron||to==Kind::astrocyte;
  return false;
}
inline constexpr bool valid_kind(Kind kind) noexcept {
  return static_cast<unsigned>(kind)>=1&&static_cast<unsigned>(kind)<=kKinds;
}
// Passive World structural invariants used by both read and write boundaries.
// Simulation behavior and decisions remain exclusively in services.
inline Status validate_world_invariants(const World& state) noexcept {
    const auto terminator=std::find(state.run_plan_digest.begin(),
                                    state.run_plan_digest.end(),'\0');
    if(terminator==state.run_plan_digest.end()||
       (terminator!=state.run_plan_digest.begin()&&
        terminator!=state.run_plan_digest.begin()+64)||
       state.cell_count>kMaxCells||state.link_count>kMaxLinks||state.intent_count>kMaxIntents)
      return Status::corrupt_state;
    for(auto it=state.run_plan_digest.begin();it!=terminator;++it)
      if(!((*it>='0'&&*it<='9')||(*it>='a'&&*it<='f')))
        return Status::corrupt_state;
    for(std::size_t i=0;i<state.cell_count;++i){
      const auto& cell=state.cells[i];
      if(cell.id==0||!valid_kind(cell.kind)||cell.energy<0||cell.energy>kValueLimit||
         cell.maturity<0||cell.maturity>kValueLimit||cell.process_proxy<0||
         cell.process_proxy>kValueLimit||cell.activity_proxy<0||
         cell.activity_proxy>kValueLimit||cell.learning_proxy<0||
         cell.learning_proxy>kValueLimit||static_cast<unsigned>(cell.stage)>3||
         (cell.viable&&cell.death_tick!=0)||(!cell.viable&&cell.death_tick==0)||
         (cell.viable&&cell.stage==Stage::historical)||
         (!cell.viable&&cell.stage!=Stage::historical)||
         cell.birth_tick>state.tick||cell.death_tick>state.tick)
        return Status::corrupt_state;
      for(std::size_t j=0;j<i;++j)if(state.cells[j].id==cell.id)return Status::duplicate;
      if(cell.parent_id!=0){
        bool found=false;
        for(std::size_t j=0;j<state.cell_count;++j)
          if(state.cells[j].id==cell.parent_id&&state.cells[j].birth_tick<=cell.birth_tick)
            found=true;
        if(!found)return Status::corrupt_state;
      }
    }
    for(std::size_t i=0;i<state.link_count;++i){
      const auto& edge=state.links[i];
      const Cell *a=nullptr,*b=nullptr;
      for(std::size_t j=0;j<state.cell_count;++j){
        if(state.cells[j].id==edge.from)a=&state.cells[j];
        if(state.cells[j].id==edge.to)b=&state.cells[j];
      }
      if(!a||!b||a==b||!is_neuron(a->kind)||!is_neuron(b->kind)||
         edge.strength==0||edge.strength>1000||edge.established_tick>state.tick)
        return Status::corrupt_state;
      for(std::size_t j=0;j<i;++j)
        if(state.links[j].from==edge.from&&state.links[j].to==edge.to)
          return Status::duplicate;
    }
    for(std::size_t i=0;i<state.intent_count;++i){
      const auto& intent=state.intents[i];
      if(intent.from==intent.to||intent.guidance_weight>1000)return Status::corrupt_state;
      bool found_a=false,found_b=false;
      for(std::size_t j=0;j<state.cell_count;++j){
        found_a|=state.cells[j].id==intent.from;
        found_b|=state.cells[j].id==intent.to;
      }
      if(!found_a||!found_b)return Status::corrupt_state;
    }
    return Status::ok;
}
} // namespace anaiero::cells
