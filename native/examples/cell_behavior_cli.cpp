#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/services/runtime_service.hpp"
#include "anaiero/services/cell_behavior_service.hpp"
// Public: anaiero cells <sample|run|inspect> <atlas.cevatlas> <state.cevcells> [ticks].
// Only a deterministic digital illustration. No physical culture controls.
#include "anaiero/infrastructure/cell_checkpoint.hpp"
#include "anaiero/infrastructure/cell_run_plan.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/cell_bus_adapter.hpp"
#include <filesystem>
#include <iostream>
#include <string_view>
namespace cell=anaiero::cells;
namespace files=anaiero::cells::files;
namespace fs=std::filesystem;
static int fail(const char* text,int status){std::cerr<<text<<" status="<<status<<'\n';return 1;}
static void show(const cell::World& world,const cell::Atlas& atlas,
                 std::uint64_t published=0) {
  const auto m=cell::metrics(world);
  std::cout<<"{\"origin\":\"SIMULATED_UNCALIBRATED\",\"format\":\"CEVCELLSTATE/1\","
     <<"\"atlas_sha256\":\""<<atlas.model_digest<<"\",\"tick\":"<<world.tick
     <<",\"cells\":"<<world.cell_count<<",\"viable\":"<<m.viable
     <<",\"historical\":"<<m.historical<<",\"links\":"<<m.linked
     <<",\"energy_proxy\":"<<m.total_energy<<",\"learning_proxy\":"<<m.total_learning
     <<",\"bus_revisions\":"<<published<<",\"plan_sha256\":\""
     <<(world.run_plan_digest[0]==0?"none":world.run_plan_digest.data())<<"\"}\n";
}
int main(int argc,char** argv) {
  if(argc<4||argc>5){
    std::cerr<<"anaiero cells sample <atlas.cevatlas> <state.cevcells>\n"
             <<"anaiero cells run <atlas.cevatlas> <state.cevcells> <ticks:1..1000>\n"
             <<"anaiero cells execute <atlas.cevatlas> <state.cevcells> <plan.cevcellrun>\n"
             <<"anaiero cells inspect <atlas.cevatlas> <state.cevcells>\n";
    return 2;
  }
  const std::string_view action{argv[1]};
  cell::Atlas atlas;
  if(auto r=files::load_checked_atlas(argv[2],atlas);r!=cell::Status::ok)
    return fail("atlas",static_cast<int>(r));
  cell::BoundedPositionPort movement;
  cell::CellBehaviorService service(atlas,movement);
  if(action=="sample"&&argc==4){
    if(fs::exists(argv[3]))return fail("state exists",2);
    const auto seed=[&](std::uint32_t id,cell::Kind k,std::int32_t x,
                        std::int32_t y,std::uint32_t parent=0){
      cell::Cell c{};c.id=id;c.kind=k;c.position={x,y,2};c.parent_id=parent;
      return service.add(c);
    };
    if(seed(1,cell::Kind::crest_progenitor,10,12)!=cell::Status::ok||
       seed(2,cell::Kind::blood_carrier,3,5)!=cell::Status::ok||
       seed(3,cell::Kind::central_neuron,20,17)!=cell::Status::ok||
       seed(4,cell::Kind::central_neuron,23,17)!=cell::Status::ok||
       service.encourage(3,4,700)!=cell::Status::ok)return fail("sample",1);
    if(auto r=files::write_world(argv[3],atlas,service.world());r!=cell::Status::ok)
      return fail("save",static_cast<int>(r));
    show(service.world(),atlas);return 0;
  }
  if(action!="inspect"&&action!="run"&&action!="execute")return 2;
  cell::World old;
  if(auto r=files::read_world(argv[3],atlas,old);r!=cell::Status::ok)
    return fail("checkpoint",static_cast<int>(r));
  if(service.restore(old)!=cell::Status::ok)return fail("invalid-state",1);
  if(action=="inspect"&&argc==4){show(service.world(),atlas);return 0;}
  if(action=="execute"&&argc==5){
    files::RunPlan plan;
    if(auto r=files::load_run_plan(argv[4],atlas,plan);r!=cell::Status::ok)
      return fail("run-plan",static_cast<int>(r));
    if(auto r=service.bind_plan(plan.sha256);r!=cell::Status::ok)
      return fail("plan-conflict",static_cast<int>(r));
    if(service.world().tick>plan.total_ticks)return fail("past-plan-end",2);
    anaiero::runtime::WeightedQueue<> runtime_work;
    anaiero::runtime::RuntimeService runtime(runtime_work);
    cell::CellBusAdapter bus(runtime);
    if(bus.enroll()!=anaiero::runtime::Status::ok)return fail("bus",1);
    service.set_observer(&bus);
    while(service.world().tick<plan.total_ticks){
      const auto* stage=files::stage_for_tick(plan,service.world().tick);
      if(!stage)return fail("stage",1);
      if(auto r=service.step(stage->context);r!=cell::Status::ok)
        return fail("behavior",static_cast<int>(r));
      runtime.drain(anaiero::runtime::kMaxWork);
      if(auto r=files::write_world(argv[3],atlas,service.world());r!=cell::Status::ok)
        return fail("save",static_cast<int>(r));
    }
    show(service.world(),atlas,runtime.latest(service.world().tick*1000000ULL,10000000ULL).revision);
    return bus.failures()==0?0:fail("bus-overflow",static_cast<int>(bus.failures()));
  }
  if(action!="run"||argc!=5)return 2;
  if(service.world().run_plan_digest[0]!=0)return fail("plan-bound-use-execute",2);
  std::uint32_t steps=0;
  if(!files::u32(argv[4],steps)||steps==0||steps>1000)return fail("ticks",2);
  anaiero::runtime::WeightedQueue<> runtime_work;
    anaiero::runtime::RuntimeService runtime(runtime_work);
  cell::CellBusAdapter bus(runtime);
  if(bus.enroll()!=anaiero::runtime::Status::ok)return fail("bus",1);
  service.set_observer(&bus);
  for(std::uint32_t step=0;step<steps;++step){
    cell::Context context{75,0,64,true};
    if(auto result=service.step(context);result!=cell::Status::ok)
      return fail("behavior",static_cast<int>(result));
    runtime.drain(anaiero::runtime::kMaxWork);
    if(auto r=files::write_world(argv[3],atlas,service.world());r!=cell::Status::ok)
      return fail("save",static_cast<int>(r));
  }
  show(service.world(),atlas,runtime.latest(steps*1000000ULL,10000000ULL).revision);
  if(bus.failures()>0)return fail("bus-overflow",static_cast<int>(bus.failures()));
  return 0;
}
