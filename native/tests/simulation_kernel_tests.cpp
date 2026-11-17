// Portable simulation lifecycle, native SW5 checkpoint + SIM3 durable restart.
#include "anaiero/application/sw5_simulation_adapter.hpp"
#include "anaiero/application/sim3_simulation_adapter.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/memory_checkpoint.hpp"
#include "anaiero/services/simulation_coordinator_service.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include <cstdlib>
#include <algorithm>
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
static bool same_inputs(const eng::Inputs& a,const eng::Inputs& b) {
  return a.support_proxy==b.support_proxy&&a.stress_proxy==b.stress_proxy&&
    a.digital_signal==b.digital_signal&&a.lesson==b.lesson;
}
struct RoutedInputs final:eng::IEngineInputSelector {
  std::uint32_t fail_id=0,calls=0;
  std::int32_t extra=0;
  eng::Status select(const eng::Descriptor& d,std::uint64_t,
      const eng::Inputs& defaults,eng::Inputs& out) noexcept override {
    ++calls;
    if(d.id==fail_id)return eng::Status::unsupported;
    out=d.kind==eng::Kind::sim3?eng::Inputs{}:defaults;
    if(d.kind==eng::Kind::sw5)out.digital_signal+=extra;
    return eng::Status::ok;
  }
};
struct ScriptedEngine final:eng::ISimulationEngine {
  std::uint32_t id,source;
  std::uint64_t epoch;
  eng::Kind kind;
  std::string model=std::string(64,'a');
  eng::Snapshot state{};
  eng::StepRequest committed_request{};
  std::uint32_t initializations=0,advances=0,attempts=0,failures=0;
  bool ready=false,replayable=false;
  ScriptedEngine(std::uint32_t i,std::uint32_t s,std::uint64_t e=1,
                 eng::Kind k=eng::Kind::sw5):id(i),source(s),epoch(e),kind(k){}
  eng::Descriptor describe() const noexcept override {
    return {id,source,epoch,kind,model,"SYNTHETIC_UNCALIBRATED",false,false};
  }
  void digest() noexcept {
    state.state_sha256.fill('a');state.state_sha256[64]='\0';
    state.state_sha256[0]=static_cast<char>('0'+state.tick%10);
  }
  eng::Status initialize() noexcept override {
    ++initializations;ready=true;state={};state.id=id;state.source_epoch=epoch;
    digest();return eng::Status::ok;
  }
  eng::Status latest(eng::Snapshot& out) const noexcept override {
    if(!ready)return eng::Status::not_initialized;
    out=state;return eng::Status::ok;
  }
  eng::Status advance(const eng::StepRequest& request,eng::Snapshot& out) noexcept override {
    ++attempts;
    if(!ready)return eng::Status::not_initialized;
    if(failures){--failures;return eng::Status::advance_failed;}
    if(replayable&&request.expected_tick!=UINT64_MAX&&
       request.expected_tick+1==state.tick) {
      if(request.controller_cycle!=committed_request.controller_cycle||
         !same_inputs(request.inputs,committed_request.inputs))
        return eng::Status::tick_conflict;
      out=state;return eng::Status::ok;
    }
    if(request.expected_tick!=state.tick)return eng::Status::tick_conflict;
    if(kind==eng::Kind::sim3&&!same_inputs(request.inputs,{}))
      return eng::Status::unsupported;
    committed_request=request;++advances;++state.tick;++state.revision;
    state.energy+=request.inputs.digital_signal;digest();out=state;
    return eng::Status::ok;
  }
  eng::Status checkpoint(eng::ICheckpointWriter&) const noexcept override {
    return eng::Status::unsupported;
  }
  eng::Status restore(eng::ICheckpointReader&) noexcept override {
    return eng::Status::unsupported;
  }
};
struct LifecycleObserver final:anaiero::contracts::IServiceExtension {
  std::uint32_t engine_commits=0,rejections=0;
  bool valid=true;
  void on_service_event(const anaiero::contracts::ServiceEvent& e) noexcept override {
    valid=valid&&e.service==anaiero::contracts::ServiceId::simulation_coordinator;
    if(e.phase==anaiero::contracts::ServicePhase::committed&&e.subject)
      ++engine_commits;
    if(e.phase==anaiero::contracts::ServicePhase::rejected)++rejections;
  }
};
static void coordinator_regressions() {
  // A failed first publication must not skip the second engine. Persisting the
  // pending snapshot and resuming it must not call initialize or advance again.
  {
    ScriptedEngine a(1,11),b(2,12);Collector sink;sink.fail_after=1;
    eng::SimulationCoordinatorService c;
    CHECK(c.register_engine(a,1)&&c.register_engine(b,1));
    c.set_publisher(&sink);CHECK(c.initialize()==eng::Status::ok);
    const eng::Inputs original{5,0,17,true};
    CHECK(c.run_cycles(1,original)==eng::Status::publish_failed);
    CHECK(a.state.tick==1&&b.state.tick==0&&c.cycles()==0);
    CHECK(c.refresh_latest()==eng::Status::invalid);
    eng::EngineRuntimeStatus status{};
    CHECK(c.inspect(1,status)&&status.publication_pending&&
      status.last_status==eng::Status::publish_failed);
    const auto pending=c.progress();
    CHECK(pending.cycle_active&&pending.next_slot==0&&pending.publication_pending);
    eng::SimulationCoordinatorService recovered;
    CHECK(recovered.register_engine(a,1)&&recovered.register_engine(b,1));
    recovered.set_publisher(&sink);
    CHECK(recovered.resume(pending)==eng::Status::ok);
    CHECK(a.initializations==1&&b.initializations==1);
    CHECK(recovered.step_cycle({0,0,999,false})==eng::Status::ok);
    CHECK(a.advances==1&&b.advances==0&&recovered.progress().next_slot==1);
    CHECK(recovered.step_cycle({0,0,999,false})==eng::Status::ok);
    CHECK(recovered.cycles()==1&&a.state.tick==1&&b.state.tick==1);
    CHECK(same_inputs(b.committed_request.inputs,original)&&sink.count==2);
    CHECK(recovered.run_cycles(1)==eng::Status::ok);
    CHECK(recovered.cycles()==2&&a.state.tick==2&&b.state.tick==2);
  }
  // A failed later advance must not re-run earlier engines; a new call cannot
  // replace the already resolved inputs or controller-cycle identity.
  {
    ScriptedEngine a(1,11),b(2,12);b.failures=1;LifecycleObserver hook;
    eng::SimulationCoordinatorService c;
    CHECK(c.register_engine(a,1)&&c.register_engine(b,1));
    c.set_extension(&hook);CHECK(c.initialize()==eng::Status::ok);
    CHECK(c.run_cycles(1,{0,0,23,true})==eng::Status::advance_failed);
    CHECK(c.cycles()==0&&c.progress().next_slot==1&&a.state.tick==1&&b.state.tick==0);
    CHECK(c.run_cycles(1,{0,0,777,false})==eng::Status::ok);
    CHECK(c.cycles()==1&&a.advances==1&&b.advances==1&&b.state.energy==23);
    CHECK(b.committed_request.controller_cycle==1&&b.committed_request.inputs.lesson);
    CHECK(hook.valid&&hook.engine_commits==2&&hook.rejections==1);
  }
  // Crash between durable engine commit and saved coordinator progress: only
  // the next engine may be one tick ahead, and replay must reuse its inputs.
  {
    ScriptedEngine a(1,11),b(2,12);a.replayable=true;
    eng::SimulationCoordinatorService c;
    CHECK(c.register_engine(a,1)&&c.register_engine(b,1));
    CHECK(c.initialize()==eng::Status::ok&&c.begin_cycle({0,0,31,true})==eng::Status::ok);
    const auto before=c.progress();eng::Snapshot advanced{};
    CHECK(a.advance({0,before.engines[0].cycle_inputs,1},advanced)==eng::Status::ok);
    eng::SimulationCoordinatorService recovered;
    CHECK(recovered.register_engine(a,1)&&recovered.register_engine(b,1));
    CHECK(recovered.resume(before)==eng::Status::tick_conflict);
    auto invalid=before;invalid.engines[0].model_digest[0]='b';
    CHECK(recovered.resume(invalid,true)==eng::Status::invalid);
    invalid=before;++invalid.engines[0].source_epoch;
    CHECK(recovered.resume(invalid,true)==eng::Status::invalid);
    invalid=before;invalid.engines[1].cadence=2;
    CHECK(recovered.resume(invalid,true)==eng::Status::invalid);
    CHECK(recovered.resume(before,true)==eng::Status::ok);
    CHECK(recovered.run_cycles(1,{0,0,888,false})==eng::Status::ok);
    CHECK(a.advances==1&&a.attempts==2&&b.advances==1&&recovered.cycles()==1);
    CHECK(b.state.energy==31&&a.initializations==1);
  }
  // Recovery may not hide extra steps on another engine or more than one
  // already committed step on the pending engine.
  {
    ScriptedEngine a(1,11),b(2,12);eng::SimulationCoordinatorService c;
    CHECK(c.register_engine(a,1)&&c.register_engine(b,1));
    CHECK(c.initialize()==eng::Status::ok&&c.begin_cycle()==eng::Status::ok);
    const auto before=c.progress();eng::Snapshot out{};
    CHECK(b.advance({0,{},1},out)==eng::Status::ok);
    eng::SimulationCoordinatorService wrong;
    CHECK(wrong.register_engine(a,1)&&wrong.register_engine(b,1));
    CHECK(wrong.resume(before,true)==eng::Status::tick_conflict);
    CHECK(b.initialize()==eng::Status::ok);
    CHECK(a.advance({0,{},1},out)==eng::Status::ok);
    CHECK(a.advance({1,{},2},out)==eng::Status::ok);
    CHECK(wrong.resume(before,true)==eng::Status::tick_conflict);
  }
  // Mixed cadence includes cycles with no due engines and a failure where both
  // are due. Completing the remaining engine keeps the original cadence.
  {
    ScriptedEngine a(1,11),b(2,12);Collector sink;
    eng::SimulationCoordinatorService c;
    CHECK(c.register_engine(a,2)&&c.register_engine(b,3));
    c.set_publisher(&sink);CHECK(c.initialize()==eng::Status::ok);
    CHECK(c.run_cycles(5)==eng::Status::ok&&a.state.tick==2&&b.state.tick==1);
    sink.fail_after=sink.count+1;
    CHECK(c.run_cycles(1)==eng::Status::publish_failed);
    CHECK(c.cycles()==5&&a.state.tick==3&&b.state.tick==1);
    CHECK(c.flush_pending()==eng::Status::ok&&c.cycles()==5);
    CHECK(c.run_cycles(1)==eng::Status::ok&&c.cycles()==6&&b.state.tick==2);
  }
  // Input selection is per engine and happens before any side effect. Changing
  // the selector after begin_cycle must not rewrite a staged cycle.
  {
    ScriptedEngine a(1,11),b(2,12,1,eng::Kind::sim3);RoutedInputs inputs;
    inputs.fail_id=2;eng::SimulationCoordinatorService c;
    CHECK(c.register_engine(a,1)&&c.register_engine(b,1));
    c.set_input_selector(&inputs);CHECK(c.initialize()==eng::Status::ok);
    CHECK(c.begin_cycle({0,0,40,true})==eng::Status::unsupported);
    CHECK(!c.cycle_active()&&a.advances==0&&b.advances==0);
    inputs.fail_id=0;CHECK(c.begin_cycle({0,0,40,true})==eng::Status::ok);
    inputs.extra=100;CHECK(c.run_cycles(1,{0,0,900,false})==eng::Status::ok);
    CHECK(a.state.energy==40&&b.state.energy==0&&inputs.calls==4);
    CHECK(a.committed_request.inputs.lesson&&!b.committed_request.inputs.lesson);
  }
  // Attach already restored state without reset, and refresh a separately
  // committed route change at the same tick only when the coordinator is idle.
  {
    ScriptedEngine a(1,11);eng::Snapshot out{};
    CHECK(a.initialize()==eng::Status::ok&&a.advance({0,{},0},out)==eng::Status::ok);
    eng::SimulationCoordinatorService c;CHECK(c.register_engine(a,1));
    CHECK(c.initialize_from_latest()==eng::Status::ok&&c.latest(1)->tick==1);
    CHECK(a.initializations==1&&c.run_cycles(2)==eng::Status::ok&&a.state.tick==3);
    a.state.energy=91;++a.state.revision;
    CHECK(c.refresh_latest()==eng::Status::ok&&c.cycles()==2&&c.latest(1)->energy==91);
    CHECK(a.advance({3,{},0},out)==eng::Status::ok);
    CHECK(c.refresh_latest()==eng::Status::tick_conflict&&c.latest(1)->tick==3);
  }
  {
    ScriptedEngine a(1,11),duplicate_source(2,11);eng::SimulationCoordinatorService c;
    CHECK(c.run_cycles(1)==eng::Status::not_initialized);
    CHECK(!c.register_engine(a,0)&&!c.register_engine(a,65));
    CHECK(c.register_engine(a,1)&&!c.register_engine(duplicate_source,1));
    CHECK(c.initialize()==eng::Status::ok&&!c.register_engine(duplicate_source,1));
    CHECK(c.initialize()==eng::Status::invalid);
    auto invalid=c.progress();invalid.cycle_active=false;invalid.publication_pending=true;
    eng::SimulationCoordinatorService recovered;CHECK(recovered.register_engine(a,1));
    CHECK(recovered.resume(invalid)==eng::Status::invalid);
  }
}
int main(int argc,char**argv){
  CHECK(argc==4);
  coordinator_regressions();
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
    // The common cycle routes nonzero SW5 controls only to SW5; SIM3 retains
    // its own plan inputs and receives the supported empty input record.
    app::Sw5SimulationAdapter sw5(202,72,2,atlas,seed.world(),spatial);
    CHECK(coordinator.register_engine(sw5,1));
    CHECK(coordinator.register_engine(other,2));
    CHECK(!coordinator.register_engine(other,2)); // duplicate identity
    RoutedInputs routed;coordinator.set_input_selector(&routed);
    CHECK(coordinator.initialize()==eng::Status::ok);
    CHECK(coordinator.run_cycles(4,{75,0,64,true})==eng::Status::ok);
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
