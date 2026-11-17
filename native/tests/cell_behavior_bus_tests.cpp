#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/services/runtime_service.hpp"
#include "anaiero/infrastructure/cell_bus_adapter.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/services/cell_behavior_service.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
using namespace anaiero::cells;
namespace rt=anaiero::runtime;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL " #x " line "<<__LINE__<<'\n';std::exit(1);}}while(false)
int main(int argc,char**argv){
  CHECK(argc==2);
  Atlas atlas;
  CHECK(files::load_checked_atlas(std::filesystem::path(argv[1])/"examples/cells/crest-blood-neural.cevatlas",atlas)==Status::ok);
  BoundedPositionPort port;
  CellBehaviorService service(atlas,port);
  rt::WeightedQueue<> runtime_queue;rt::RuntimeService runtime(runtime_queue);
  CellBusAdapter adapter(runtime);
  CHECK(adapter.enroll()==rt::Status::ok);
  service.set_observer(&adapter);
  for(int i=0;i<2;++i){
    Cell c{};c.id=static_cast<std::uint32_t>(i+1);c.kind=Kind::central_neuron;
    c.position={10+i*2,12,1};
    CHECK(service.add(c)==Status::ok);
  }
  CHECK(service.encourage(1,2,900)==Status::ok);
  for(int n=0;n<12;++n){
    CHECK(service.step({80,0,64,true})==Status::ok);
    runtime.drain(96);
  }
  CHECK(adapter.failures()==0);
  CHECK(service.world().link_count==1);
  auto state=runtime.latest(12000000ULL,4000000ULL);
  CHECK(state.revision>0);
  bool found_learning=false,found_link=false;
  for(std::size_t i=0;i<state.count;++i){
    const auto& entry=state.values[i];
    if(entry.metric==rt::Metric::digital_learning){
      found_learning=true;CHECK(entry.value>0);
    }
    if(entry.metric==rt::Metric::connection)found_link=true;
  }
  CHECK(found_learning&&found_link);
  std::cout<<"Cell behavior -> WindPacket -> ColonyState integration passed\n";
}
