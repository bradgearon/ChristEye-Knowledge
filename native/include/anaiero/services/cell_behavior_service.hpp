#pragma once
// CE-SW-19: deterministic, illustrative computational behavior rules.
// No cell culture, differentiation, feeding, disease or electrical protocols.
#include "anaiero/contracts/cell_behavior.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
namespace anaiero::cells {
inline std::int32_t bound(std::int64_t n,std::int32_t maximum=kValueLimit) noexcept {
  return n<0?0:n>maximum?maximum:static_cast<std::int32_t>(n);
}
inline Metrics metrics(const World& world) noexcept {
  Metrics m{};
  m.linked=world.link_count;
  for(std::size_t i=0;i<world.cell_count;++i){
    const auto& c=world.cells[i];
    if(c.viable)++m.viable;else ++m.historical;
    m.total_energy+=c.energy;m.total_process+=c.process_proxy;
    m.total_activity+=c.activity_proxy;m.total_learning+=c.learning_proxy;
  }
  return m;
}
// Model is passive; service owns all behavior, lineage and connection logic.
// One writer, bounded scratch space and deterministic iteration order.
class CellBehaviorService final {
  Atlas atlas_{};
  World world_{};
  ISpatialPort& spatial_;
  IEventPort* notifications_=nullptr;
  contracts::IServiceExtension* extension_=nullptr;
  EventBatch changes_{};
  Cell* find(std::uint32_t id) noexcept {
    for(std::size_t i=0;i<world_.cell_count;++i)
      if(world_.cells[i].id==id)return &world_.cells[i];
    return nullptr;
  }
  const Cell* find(std::uint32_t id) const noexcept {
    for(std::size_t i=0;i<world_.cell_count;++i)
      if(world_.cells[i].id==id)return &world_.cells[i];
    return nullptr;
  }
  void emit(EventKind kind,std::uint32_t cell,std::uint32_t other=0,
            std::int32_t value=0) noexcept {
    if(changes_.count<changes_.values.size())
      changes_.values[changes_.count++]={world_.tick,cell,other,kind,value};
    else ++changes_.omitted;
  }
  bool connected(std::uint32_t from,std::uint32_t to) const noexcept {
    for(std::size_t i=0;i<world_.link_count;++i)
      if(world_.links[i].from==from&&world_.links[i].to==to)return true;
    return false;
  }
public:
  CellBehaviorService(Atlas atlas,ISpatialPort& movement) noexcept
      :atlas_(std::move(atlas)),spatial_(movement){}
  const Atlas& atlas() const noexcept {return atlas_;}
  const World& world() const noexcept {return world_;}
  const EventBatch& last_events() const noexcept {return changes_;}
  void set_observer(IEventPort* observer) noexcept {notifications_=observer;}
  void set_extension(contracts::IServiceExtension* extension) noexcept {extension_=extension;}
  Status bind_plan(std::string hash) noexcept {
    if(hash.size()!=64)return Status::invalid;
    for(char c:hash)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return Status::invalid;
    if(world_.run_plan_digest[0]!=0&&std::string_view(world_.run_plan_digest.data())!=hash)
      return Status::conflict;
    std::copy(hash.begin(),hash.end(),world_.run_plan_digest.begin());
    world_.run_plan_digest[64]='\0';
    return Status::ok;
  }
  Status restore(const World& state) noexcept {
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
    world_=state;
    changes_={};
    return Status::ok;
  }
  Status add(Cell cell) noexcept {
    if(cell.id==0||!valid_kind(cell.kind)||find(cell.id))return Status::duplicate;
    if(world_.cell_count>=kMaxCells)return Status::capacity;
    if(cell.parent_id!=0&&!find(cell.parent_id))return Status::invalid;
    if(cell.energy<1||cell.energy>kValueLimit)return Status::invalid;
    if(cell.birth_tick!=world_.tick||!cell.viable||cell.death_tick!=0)return Status::invalid;
    world_.cells[world_.cell_count++]=cell;
    ++world_.generation;return Status::ok;
  }
  Status transition(std::uint32_t id,Kind candidate) noexcept {
    auto* c=find(id);
    if(!c||!c->viable)return Status::invalid;
    if(!can_transition(c->kind,candidate))return Status::incompatible_lineage;
    if(c->maturity<12)return Status::not_ready;
    c->kind=candidate;c->stage=Stage::developing;
    ++world_.generation;
    changes_={};emit(EventKind::developed,id,0,static_cast<std::int32_t>(candidate));
    if(notifications_)notifications_->published(changes_.values[0]);
    return Status::ok;
  }
  Status daughter(std::uint32_t parent_id,std::uint32_t daughter_id) noexcept {
    auto* parent=find(parent_id);
    if(!parent||!parent->viable||!progenitor(parent->kind)||parent->maturity<16)
      return Status::not_ready;
    if(!daughter_id||find(daughter_id))return Status::duplicate;
    if(world_.cell_count>=kMaxCells)return Status::capacity;
    // Simulation-only copy: neither a real division procedure nor cloning.
    Cell daughter{};
    daughter.id=daughter_id;daughter.parent_id=parent_id;daughter.kind=parent->kind;
    daughter.position=parent->position;daughter.energy=60;
    daughter.birth_tick=world_.tick;
    Vec3 proposed{daughter.position.x+1,daughter.position.y,daughter.position.z};
    Vec3 result{};
    if(!spatial_.translate(daughter.position,proposed,result))return Status::movement_rejected;
    daughter.position=result;
    world_.cells[world_.cell_count++]=daughter;
    ++world_.generation;
    changes_={};emit(EventKind::division,daughter_id,parent_id);
    if(notifications_)notifications_->published(changes_.values[0]);
    return Status::ok;
  }
  Status encourage(std::uint32_t from,std::uint32_t to,std::uint16_t weight) noexcept {
    auto* a=find(from);auto* b=find(to);
    if(!a||!b||a==b||!is_neuron(a->kind)||!is_neuron(b->kind)||weight>1000)
      return Status::invalid;
    for(std::size_t i=0;i<world_.intent_count;++i)
      if(world_.intents[i].from==from&&world_.intents[i].to==to){
        world_.intents[i].guidance_weight=weight;return Status::ok;
      }
    if(world_.intent_count>=kMaxIntents)return Status::capacity;
    world_.intents[world_.intent_count++]={from,to,weight};
    ++world_.generation;return Status::ok;
  }
  Status step(Context inputs) noexcept {
    if(inputs.support_proxy<0||inputs.support_proxy>1000||
       inputs.stress_proxy<0||inputs.stress_proxy>1000||
       inputs.digital_signal<0||inputs.digital_signal>1000||
       world_.tick==std::numeric_limits<std::uint64_t>::max())return Status::invalid;
    // Candidate staging ensures that infrastructure movement failures do not
    // commit partial cell states. Do not publish events until commit.
    World next=world_;
    ++next.tick;
    EventBatch events{};
    const auto push=[&](EventKind kind,std::uint32_t id,std::uint32_t other=0,
                       std::int32_t value=0) noexcept {
      if(events.count<events.values.size())
        events.values[events.count++]={next.tick,id,other,kind,value};
      else ++events.omitted;
    };
    for(std::size_t i=0;i<next.cell_count;++i){
      auto& c=next.cells[i];
      if(!c.viable)continue;
      const auto& p=atlas_.by_kind[static_cast<std::size_t>(c.kind)-1];
      const auto support_gain=inputs.support_proxy>0?p.support_response:0;
      const auto stress_loss=inputs.stress_proxy>0?p.stress_response:0;
      c.energy=bound(static_cast<std::int64_t>(c.energy)+support_gain-p.maintenance-stress_loss);
      if(c.energy==0){
        c.viable=false;c.stage=Stage::historical;c.death_tick=next.tick;
        push(EventKind::nonviable,c.id);continue;
      }
      if(inputs.support_proxy>0){
        const auto old=c.maturity;
        c.maturity=bound(static_cast<std::int64_t>(c.maturity)+p.maturation);
        if(old<12&&c.maturity>=12){c.stage=Stage::developing;push(EventKind::developed,c.id);}
        if(c.maturity>=12 && p.process_extension>0){
          const auto before=c.process_proxy;
          c.process_proxy=bound(static_cast<std::int64_t>(c.process_proxy)+p.process_extension);
          if(c.process_proxy!=before)push(EventKind::process_extended,c.id,0,c.process_proxy);
        }
      }
      c.activity_proxy=bound(static_cast<std::int64_t>(c.activity_proxy)-2+
                   (static_cast<std::int64_t>(inputs.digital_signal)*p.activity_response)/32);
      if(inputs.lesson_active && c.maturity>=12 && c.activity_proxy>=10 &&
         p.plasticity>0){
        const auto old=c.learning_proxy;
        c.learning_proxy=bound(static_cast<std::int64_t>(c.learning_proxy)+p.plasticity);
        if(c.learning_proxy!=old)
          push(EventKind::learning_proxy_changed,c.id,0,c.learning_proxy);
      }
      if(p.mobility>0){
        Vec3 desired{c.position.x+static_cast<std::int32_t>(p.mobility),
                     c.position.y,c.position.z};
        Vec3 moved{};
        if(!spatial_.translate(c.position,desired,moved))return Status::movement_rejected;
        c.position=moved;push(EventKind::movement,c.id,0,moved.x);
      }
    }
    for(std::size_t i=0;i<next.intent_count;++i){
      const auto intent=next.intents[i];
      Cell* a=nullptr;Cell* b=nullptr;
      for(std::size_t j=0;j<next.cell_count;++j){
        if(next.cells[j].id==intent.from)a=&next.cells[j];
        if(next.cells[j].id==intent.to)b=&next.cells[j];
      }
      if(!a||!b||!a->viable||!b->viable)continue;
      if(!is_neuron(a->kind)||!is_neuron(b->kind)||a->maturity<12||b->maturity<12)
        continue;
      const auto& p=atlas_.by_kind[static_cast<std::size_t>(a->kind)-1];
      const auto delta_x=static_cast<std::int64_t>(a->position.x)-b->position.x;
      const auto delta_y=static_cast<std::int64_t>(a->position.y)-b->position.y;
      const auto delta_z=static_cast<std::int64_t>(a->position.z)-b->position.z;
      const auto dist2=delta_x*delta_x+delta_y*delta_y+delta_z*delta_z;
      const auto radius=static_cast<std::int64_t>(p.contact_radius);
      if(dist2>radius*radius)continue;
      bool exists=false;
      for(std::size_t j=0;j<next.link_count;++j)
        if(next.links[j].from==a->id&&next.links[j].to==b->id)exists=true;
      if(exists)continue;
      const auto score=static_cast<std::int64_t>(a->process_proxy)+b->process_proxy+
                       intent.guidance_weight;
      if(score<600)continue;
      if(next.link_count>=kMaxLinks)return Status::capacity;
      next.links[next.link_count++]={a->id,b->id,static_cast<std::uint16_t>(
        std::min<std::int64_t>(1000,score)),next.tick};
      a->stage=Stage::integrated;b->stage=Stage::integrated;
      push(EventKind::linked,a->id,b->id,static_cast<std::int32_t>(score));
    }
    if(events.omitted>0)return Status::capacity; // don't lose lineage receipts
    ++next.generation;
    world_=next;changes_=events;
    if(notifications_)
      for(std::size_t i=0;i<changes_.count;++i)
        notifications_->published(changes_.values[i]);
    if(extension_)extension_->on_service_event({contracts::ServiceId::cell_behavior,
      contracts::ServicePhase::committed,world_.generation,0,0});
    return Status::ok;
  }
};
} // namespace anaiero::cells
