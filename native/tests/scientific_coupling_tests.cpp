#include "anaiero/services/coupled_simulation_service.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#ifdef NDEBUG
#undef assert
#define assert(expr) do {if(!(expr))std::abort();}while(false)
#endif
using namespace anaiero;
struct Frames final:science::ICellFramePublisher {
  science::Frame last{};
  unsigned received=0;
  bool publish(const science::Frame& frame)noexcept override {
    last=frame;++received;return true;
  }
};
static cells::World seed(const cells::Atlas& a,cells::ISpatialPort& position){
  cells::CellBehaviorService engine(a,position);
  cells::Cell x{};x.id=3;x.kind=cells::Kind::central_neuron;x.energy=100;x.position={20,17,2};
  cells::Cell y=x;y.id=4;y.position={23,17,2};
  cells::Cell z=x;z.id=5;z.kind=cells::Kind::astrocyte;z.position={21,18,2};
  assert(engine.add(x)==cells::Status::ok&&engine.add(y)==cells::Status::ok&&
    engine.add(z)==cells::Status::ok);
  assert(engine.encourage(3,4,700)==cells::Status::ok);
  return engine.world();
}
int main(){
  cells::Atlas atlas{};
  for(unsigned i=0;i<cells::kKinds;++i){
    atlas.by_kind[i].kind=static_cast<cells::Kind>(i+1);
    atlas.by_kind[i].maintenance=1;
    atlas.by_kind[i].support_response=2;
    atlas.by_kind[i].maturation=2;
    atlas.by_kind[i].process_extension=4;
    atlas.by_kind[i].activity_response=5;
    atlas.by_kind[i].plasticity=2;
    atlas.by_kind[i].contact_radius=6;
  }
  cells::BoundedPositionPort pos;
  auto initial=seed(atlas,pos);
  science::Plan plan{};
  std::array<science::Result,4> results{};
  for(unsigned i=0;i<4;++i){
    science::ReferenceElectricalService electrical;
    science::CoupledSimulationService simulation(atlas,pos,electrical);
    Frames frames;simulation.set_publisher(&frames);
    assert(simulation.initialize(initial)==science::Code::ok);
    plan.condition=static_cast<science::Condition>(i+1);
    assert(simulation.run(plan,results[i])==science::Code::ok);
    assert(results[i].steps==28&&frames.received==28&&
           frames.last.fast_steps==112);
    assert(simulation.world().tick==14);
    assert(results[i].viable==3);
    science::ReferenceElectricalService second_electrical;
    science::CoupledSimulationService verify(atlas,pos,second_electrical);
    assert(verify.initialize(initial)==science::Code::ok);
    science::Result again{};
    assert(verify.run(plan,again)==science::Code::ok);
    assert(again.end_retention==results[i].end_retention);
  }
  for(const auto& r:results)std::cout<<static_cast<int>(r.condition)<<" "<<r.end_retention<<" "<<r.average_probe<<"\n";
  assert(results[1].end_retention>results[0].end_retention);
  assert(results[1].end_retention>results[3].end_retention);
  assert(results[2].end_retention==results[0].end_retention);
  {
    science::ReferenceElectricalService model;
    science::CoupledSimulationService blocked(atlas,pos,model);
    assert(blocked.initialize(initial)==science::Code::ok);
    plan.condition=science::Condition::astrocyte_coupled;
    struct Once final:science::ICellFramePublisher {
      unsigned calls=0;
      bool publish(const science::Frame&) noexcept override {return ++calls!=1;}
    } once;
    blocked.set_publisher(&once);
    assert(blocked.step(plan,science::Phase::train,0)==science::Code::publishing_failed);
    assert(blocked.step(plan,science::Phase::train,1)==science::Code::publishing_failed);
    assert(blocked.flush_pending()==science::Code::ok);
    assert(once.calls==2&&blocked.frame_count()==1);
    assert(blocked.step(plan,science::Phase::train,1)==science::Code::ok);
    assert(blocked.frame_count()==2);
  }
  {
    science::ReferenceElectricalService model;
    science::CoupledSimulationService rejected(atlas,pos,model);
    assert(rejected.initialize(initial)==science::Code::ok);
    science::Plan malformed{};malformed.electrical_steps_per_tick=0;
    assert(rejected.step(malformed,science::Phase::train,0)==science::Code::bad_plan);
    assert(rejected.frame_count()==0);
  }
}
