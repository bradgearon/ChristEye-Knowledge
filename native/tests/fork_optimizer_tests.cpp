#include "anaiero/services/sw5_fork_optimization_service.hpp"
#include "anaiero/services/durable_digital_route_service.hpp"
#include "anaiero/infrastructure/durable_route_ledger.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <cstdlib>
namespace opt=anaiero::optimization;
namespace cells=anaiero::cells;
namespace fs=std::filesystem;
namespace pl=christeye::living::plans;
#ifdef NDEBUG
#undef assert
#define assert(e) do{if(!(e)){std::cerr<<"FAILED " #e << " at " <<__LINE__<<"\n";std::abort();}}while(false)
#endif
struct RecordingSink:opt::IRouteReceiptSink {
  std::uint64_t seen=0,oldest=0;int deliveries=0;
  bool allow=true;
  bool publish(const opt::RouteReceipt& value) noexcept override {
    if(!allow)return false;
    ++deliveries;
    if(seen==value.sequence)return true; // idempotent in the test sink
    if(value.sequence<seen||value.kind==opt::RouteEventKind::none)return false;
    seen=value.sequence;
    if(!oldest)oldest=value.sequence;
    return true;
  }
};
int main(int argc,char**argv){
 assert(argc==3);
 cells::Atlas atlas;
 assert(cells::files::load_checked_atlas(argv[1],atlas)==cells::Status::ok);
 cells::BoundedPositionPort position;
 cells::CellBehaviorService seed(atlas,position);
 cells::Cell a{};a.id=3;a.kind=cells::Kind::central_neuron;
 a.position={20,17,2};a.energy=100;
 cells::Cell b=a;b.id=4;b.position={23,17,2};
 assert(seed.add(a)==cells::Status::ok);
 assert(seed.add(b)==cells::Status::ok);
 auto initial=seed.world();
 const auto hash=pl::sha256_hex(cells::files::serialize_world(initial,atlas));
 opt::ForkPlan plan{};plan.horizon=18;
 plan.trial_context={75,0,64,true};plan.objective_count=3;
 plan.objectives[0]={opt::Measure::directed_links,1,6,true};
 plan.objectives[1]={opt::Measure::viable,2,2,true};
 plan.objectives[2]={opt::Measure::total_learning,40,1,false};
 std::array<opt::ForkOption,3> routes{{
   {{101,1,3,4,opt::RouteKind::digital_signal,2,1},400,1},
   {{102,1,3,4,opt::RouteKind::digital_signal,3,1},700,2},
   {{103,1,3,4,opt::RouteKind::digital_signal,300,100},900,100}
 }};
 opt::Sw5ForkOptimizationService optimizer(atlas,position);
 opt::ForkSelection selected{};
 assert(optimizer.analyze(initial,7,hash,plan,routes,selected)==opt::ForkStatus::ok);
 assert(selected.option.route.id==102);
 assert(selected.option.route.guidance_weight==700);
 assert(selected.baseline_forecast.links==0);
 assert(selected.candidate_forecast.links==1);
 assert(selected.baseline_forecast.viable==2&&selected.candidate_forecast.viable==2);
 assert(selected.score>0);
 assert(seed.world().link_count==0&&seed.world().intent_count==0); // no trial side effects
 auto dir=fs::path(argv[2]);fs::remove_all(dir);
 opt::DigitalRouteLedger before{};
 {
   opt::DurableRouteLedger store;
   assert(store.open(dir,atlas)==opt::StoreResult::ok);
   opt::DurableDigitalRouteService controller(store,atlas,position);
   assert(controller.initialize(initial,7,before)==opt::StoreResult::ok);
   assert(before.revision==0&&before.event_acked);
   std::uint64_t id=0;
   auto wrong=selected;
   wrong.initial_world_sha256=std::string(64,'0');
   assert(!controller.commit_selected(wrong,id));
   assert(id==0);
   wrong=selected;wrong.snapshot_revision=8;
   assert(!controller.commit_selected(wrong,id));
   assert(id==0);
   // Prior write fails only after committing immutable unreferenced data.
   store.fault_after_object_once();
   assert(!controller.commit_selected(selected,id));
   assert(id==0);
   opt::DigitalRouteLedger unchanged{};
   assert(controller.read(unchanged)==opt::StoreResult::ok);
   assert(unchanged.revision==0&&unchanged.current_world.intent_count==0);
   assert(controller.commit_selected(selected,id)&&id==1);
   assert(controller.read(unchanged)==opt::StoreResult::ok);
   assert(unchanged.revision==1&&unchanged.active&&unchanged.change_id==1);
   assert(!unchanged.event_acked&&unchanged.event_sequence==1);
   assert(unchanged.current_world.intent_count==1);
   assert(unchanged.current_world.intents[0].guidance_weight==700);
   assert(!controller.commit_selected(selected,id)); // single active and unacked receipt
   assert(!controller.advance(plan.trial_context,18,position));
   RecordingSink failing;failing.allow=false;
   assert(!controller.deliver(failing));
   assert(controller.read(unchanged)==opt::StoreResult::ok&&!unchanged.event_acked);
 }
 {
   opt::DurableRouteLedger reopened;
   assert(reopened.open(dir,atlas)==opt::StoreResult::ok);
   opt::DurableDigitalRouteService controller(reopened,atlas,position);
   opt::DigitalRouteLedger current{};
   assert(controller.read(current)==opt::StoreResult::ok);
   assert(!current.event_acked&&current.event_sequence==1);
   RecordingSink downstream;
   assert(controller.deliver(downstream));
   assert(downstream.deliveries==1&&downstream.seen==1);
   assert(controller.read(current)==opt::StoreResult::ok&&current.event_acked);
   assert(controller.advance(plan.trial_context,18,position));
   assert(controller.read(current)==opt::StoreResult::ok);
   assert(current.current_world.tick==18&&current.current_world.link_count==1);
   assert(current.event_kind==opt::RouteEventKind::simulation_advanced&&!current.event_acked);
   assert(controller.deliver(downstream)&&downstream.seen==2);
   assert(!optimizer.should_rollback(selected,plan,current.current_world));
   // Synthetic severe scenario is only a stress test of rollback logic;
   // not a physical care or stimulation instruction.
   cells::Context adverse{0,1000,0,false};
   assert(controller.advance(adverse,40,position));
   assert(controller.read(current)==opt::StoreResult::ok);
   assert(current.current_world.tick==58);
   assert(optimizer.should_rollback(selected,plan,current.current_world));
   assert(controller.deliver(downstream)&&downstream.seen==3);
   assert(controller.revert(1));
   assert(controller.read(current)==opt::StoreResult::ok);
   assert(current.current_world.tick==58); // rollback did not rewind history
   assert(current.current_world.intent_count==0); // route undone exactly
   assert(!current.active&&current.event_kind==opt::RouteEventKind::route_reverted);
   assert(!controller.revert(1));
   assert(controller.deliver(downstream)&&downstream.seen==4);
 }
 {
   opt::DurableRouteLedger last;
   assert(last.open(dir,atlas)==opt::StoreResult::ok);
   opt::DigitalRouteLedger state;
   assert(last.read(state)==opt::StoreResult::ok);
   assert(!state.active&&state.event_acked&&state.current_world.tick==58);
   assert(state.revision>=8);
   assert(state.initial_world.tick==0);
 }
 // A tampered HEAD cannot be interpreted as a valid route commit.
 {
   auto path=dir/"HEAD";
   std::ofstream file(path,std::ios::binary|std::ios::trunc);
   file<<"CEVROUTEHEAD/1\nobject not-a-sha\n";
   file.close();
   opt::DurableRouteLedger corrupted;
   assert(corrupted.open(dir,atlas)==opt::StoreResult::ok);
   opt::DigitalRouteLedger state{};
   assert(corrupted.read(state)==opt::StoreResult::corrupt);
 }
 std::cout<<"SW5 forks + multiobjective + atomic route ledger + replay + rollback PASS\n";
 return 0;
}
