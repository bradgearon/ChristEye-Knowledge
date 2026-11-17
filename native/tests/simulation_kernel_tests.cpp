// Portable simulation lifecycle, native SW5 checkpoint + SIM3 durable restart.
#include "anaiero/application/sw5_simulation_adapter.hpp"
#include "anaiero/application/sim3_simulation_adapter.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/memory_checkpoint.hpp"
#include "anaiero/services/simulation_coordinator_service.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x <<" line "<<__LINE__<<"\n";std::abort();}}while(false)
namespace eng=anaiero::engine;
namespace app=anaiero::application;
namespace cell=anaiero::cells;
namespace infra=anaiero::infrastructure;
namespace sim3=christeye::living::sim3;
namespace fs=std::filesystem;
struct Collector final:eng::ISnapshotPublisher {
  std::uint64_t count=0,fail_after=0;
  bool publish(const eng::Snapshot&) noexcept override {
    if(fail_after&&count+1==fail_after){fail_after=0;return false;}
    ++count;return true;
  }
};
int main(int argc,char**argv){
  CHECK(argc==4);
  cell::Atlas atlas{};
  CHECK(cell::files::load_checked_atlas(argv[1],atlas)==cell::Status::ok);
  cell::BoundedPositionPort spatial;
  cell::CellBehaviorService seed(atlas,spatial);
  cell::Cell a{};a.id=3;a.kind=cell::Kind::central_neuron;
  a.position={20,17,2};a.energy=100;
  cell::Cell b=a;b.id=4;b.position={23,17,2};
  CHECK(seed.add(a)==cell::Status::ok);
  CHECK(seed.add(b)==cell::Status::ok);
  app::Sw5SimulationAdapter first(101,61,1,atlas,seed.world(),spatial);
  app::Sw5SimulationAdapter replay(102,62,1,atlas,seed.world(),spatial);
  CHECK(first.initialize()==eng::Status::ok);
  CHECK(replay.initialize()==eng::Status::ok);
  eng::Snapshot x{},y{};
  for(std::uint64_t tick=0;tick<4;++tick){
    CHECK(first.advance({tick,{75,0,64,true}},x)==eng::Status::ok);
    CHECK(replay.advance({tick,{75,0,64,true}},y)==eng::Status::ok);
    CHECK(x.tick==y.tick&&x.state_sha256==y.state_sha256);
  }
  CHECK(first.advance({0,{}},x)==eng::Status::tick_conflict);
  infra::MemoryCheckpoint m(65536);
  CHECK(first.checkpoint(m)==eng::Status::ok&&!m.empty());
  app::Sw5SimulationAdapter restored(103,63,2,atlas,seed.world(),spatial);
  CHECK(restored.restore(m)==eng::Status::ok);
  CHECK(restored.latest(y)==eng::Status::ok&&y.state_sha256==x.state_sha256);
  CHECK(restored.advance({4,{75,0,64,true}},y)==eng::Status::ok);
  CHECK(first.advance({4,{75,0,64,true}},x)==eng::Status::ok);
  CHECK(x.state_sha256==y.state_sha256);
  const auto dir=fs::path(argv[3]);fs::remove_all(dir);
  std::string sim3_sha;
  {
    sim3::Runner runner;
    CHECK(runner.open(argv[2],dir)==sim3::Result::ok);
    app::Sim3SimulationAdapter other(runner,201,71,2);
    eng::SimulationCoordinatorService coordinator;
    Collector collector;coordinator.set_publisher(&collector);
    // independent baseline with no arbitrary cross-engine control values
    app::Sw5SimulationAdapter sw5(202,72,2,atlas,seed.world(),spatial);
    CHECK(coordinator.register_engine(sw5,1));
    CHECK(coordinator.register_engine(other,2));
    CHECK(!coordinator.register_engine(other,2)); // duplicate identity
    CHECK(coordinator.initialize()==eng::Status::ok);
    CHECK(coordinator.run_cycles(4)==eng::Status::ok);
    CHECK(collector.count==6&&coordinator.cycles()==4);
    const auto* sw5state=coordinator.latest(202);
    const auto* sim3state=coordinator.latest(201);
    CHECK(sw5state&&sim3state&&sw5state->tick==4&&sim3state->tick==2);
    infra::MemoryCheckpoint snapshot;
    CHECK(other.checkpoint(snapshot)==eng::Status::ok&&!snapshot.empty());
    CHECK(other.restore(snapshot)==eng::Status::unsupported);
    CHECK(other.advance({0,{}},x)==eng::Status::tick_conflict);
    sim3_sha=runner.snapshot_sha256();
    CHECK(runner.audit_disk());
  }
  {
    sim3::Runner reopened;
    CHECK(reopened.open(argv[2],dir)==sim3::Result::ok);
    CHECK(reopened.state().tick==2&&reopened.snapshot_sha256()==sim3_sha);
    CHECK(reopened.audit_disk());
  }
  {
    app::Sw5SimulationAdapter one(301,81,5,atlas,seed.world(),spatial);
    eng::SimulationCoordinatorService controller;
    CHECK(controller.register_engine(one,1)&&controller.initialize()==eng::Status::ok);
    Collector collector;collector.fail_after=1;controller.set_publisher(&collector);
    CHECK(controller.run_cycles(1)==eng::Status::publish_failed);
    CHECK(controller.run_cycles(1)==eng::Status::invalid);
    CHECK(controller.flush_pending()==eng::Status::ok&&collector.count==1);
    CHECK(controller.run_cycles(1)==eng::Status::ok&&collector.count==2);
  }
  std::cout<<"sim engine adapters, shared coordinator, checkpoints and restart PASS\n";
}
