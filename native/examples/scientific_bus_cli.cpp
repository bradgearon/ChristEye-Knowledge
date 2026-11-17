// End-to-end native synthetic multirate SW5 -> WindPacket/1 -> SW4 ColonyState.
#include "anaiero/infrastructure/scientific_windbus.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/services/coupled_simulation_service.hpp"
#include <iostream>
namespace cell=anaiero::cells;
namespace sci=anaiero::science;
namespace rt=anaiero::runtime;
int run_science_bus(const cell::Atlas& atlas){
  cell::BoundedPositionPort spatial;
  cell::CellBehaviorService starter(atlas,spatial);
  cell::Cell a{};a.id=3;a.kind=cell::Kind::central_neuron;
  a.position={20,17,2};a.energy=100;
  cell::Cell b=a;b.id=4;b.position={23,17,2};
  cell::Cell c=a;c.id=5;c.kind=cell::Kind::astrocyte;c.position={21,18,2};
  if(starter.add(a)!=cell::Status::ok||starter.add(b)!=cell::Status::ok||
     starter.add(c)!=cell::Status::ok||starter.encourage(3,4,700)!=cell::Status::ok)return 1;
  rt::RuntimeService runtime;
  sci::ScientificWindBusPublisher bus(runtime);
  if(!bus.enroll(61,1))return 1;
  sci::ReferenceElectricalService electrical;
  sci::CoupledSimulationService simulation(atlas,spatial,electrical);
  simulation.set_publisher(&bus);
  if(simulation.initialize(starter.world())!=sci::Code::ok)return 1;
  sci::Plan plan{};plan.condition=sci::Condition::astrocyte_coupled;
  sci::Result result{};
  if(simulation.run(plan,result)!=sci::Code::ok)return 1;
  bus.drain();
  const auto state=runtime.latest(100000000ULL,1000000000ULL);
  std::cout<<"{\"schema\":\"anaiero.science.bus/1\","
    <<"\"origin\":\"SIMULATED_UNCALIBRATED\",\"latest_revision\":"<<state.revision
    <<",\"windpacket_updates\":"<<bus.accepted()
    <<",\"current_metrics\":"<<state.count
    <<",\"retention_proxy\":"<<result.end_retention<<"}\n";
  return state.revision==bus.accepted()&&state.count==3&&bus.accepted()==84?0:1;
}
