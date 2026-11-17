// anaiero kernel: one portable C++20 coordinator owns SW5 and SIM3 adapters.
#include "anaiero/application/sw5_simulation_adapter.hpp"
#include "anaiero/application/sim3_simulation_adapter.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/memory_checkpoint.hpp"
#include "anaiero/services/simulation_coordinator_service.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>
namespace cell=anaiero::cells;
namespace engine=anaiero::engine;
namespace application=anaiero::application;
namespace sim3=christeye::living::sim3;
namespace infra=anaiero::infrastructure;
struct Collector final:engine::ISnapshotPublisher {
  std::uint64_t count=0;
  bool publish(const engine::Snapshot& s) noexcept override {
    if(!s.id||!s.tick)return false;
    ++count;return true;
  }
};
int main(int argc,char**argv) {
  if(argc!=5||std::string_view(argv[1])!="demo"){
    std::cerr<<"usage: anaiero kernel demo <atlas.cevatlas> <sim3-config.cev> <sim3-run-dir>\n";
    return 2;
  }
  cell::Atlas atlas{};
  if(cell::files::load_checked_atlas(argv[2],atlas)!=cell::Status::ok)return 1;
  cell::BoundedPositionPort spatial;
  cell::CellBehaviorService builder(atlas,spatial);
  cell::Cell a{};a.id=3;a.kind=cell::Kind::central_neuron;
  a.position={20,17,2};a.energy=100;
  cell::Cell b=a;b.id=4;b.position={23,17,2};
  if(builder.add(a)!=cell::Status::ok||builder.add(b)!=cell::Status::ok)
    return 1;
  application::Sw5SimulationAdapter sw5(101,61,1,atlas,builder.world(),spatial);
  sim3::Runner runner;
  if(runner.open(argv[3],argv[4])!=sim3::Result::ok)return 1;
  application::Sim3SimulationAdapter legacy(runner,102,62,1);
  engine::SimulationCoordinatorService coordinator;
  Collector observations;
  coordinator.set_publisher(&observations);
  if(!coordinator.register_engine(sw5,1)||!coordinator.register_engine(legacy,2)||
     coordinator.initialize()!=engine::Status::ok||
     coordinator.run_cycles(4)!=engine::Status::ok)return 1;
  const auto* first=coordinator.latest(101);
  const auto* second=coordinator.latest(102);
  if(!first||!second||first->tick!=4||second->tick!=2||
     observations.count!=6)return 1;
  infra::MemoryCheckpoint backup(65536);
  if(sw5.checkpoint(backup)!=engine::Status::ok)return 1;
  std::cout<<"{\"schema\":\"anaiero.kernel.demo/1\","
           <<"\"domain\":\"SYNTHETIC_ONLY\","
           <<"\"sw5_tick\":"<<first->tick
           <<",\"sim3_tick\":"<<second->tick
           <<",\"published_snapshots\":"<<observations.count
           <<",\"sw5_checkpoint_bytes\":"<<backup.bytes().size()
           <<",\"sim3_durable\":"<<(runner.audit_disk()?"true":"false")
           <<"}\n";
  return 0;
}
