#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/services/runtime_service.hpp"
// anaiero optimize fork: actual isolated digital SW5 scenarios + durable policy event.
#include "anaiero/services/sw5_fork_optimization_service.hpp"
#include "anaiero/services/durable_digital_route_service.hpp"
#include "anaiero/infrastructure/durable_route_ledger.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/services/route_receipt_delivery_service.hpp"
#include "anaiero/infrastructure/route_receipt_codec.hpp"
#include "anaiero/infrastructure/windstore_route_journal.hpp"
#include "anaiero/infrastructure/route_runtime_projection.hpp"
#include <charconv>
#include <cstdint>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string_view>
namespace cell=anaiero::cells;
namespace opt=anaiero::optimization;
namespace fs=std::filesystem;
namespace pl=christeye::living::plans;
static int error(const char* what){std::cerr<<"anaiero optimize fork: "<<what<<'\n';return 1;}
static void describe(const opt::DigitalRouteLedger& state){
 std::cout<<"{\"schema\":\"anaiero.digital-forks/1\",\"origin\":\"SYNTHETIC_ONLY\","
          <<"\"revision\":"<<state.revision<<",\"tick\":"<<state.current_world.tick
          <<",\"active\":"<<(state.active?"true":"false")
          <<",\"route\":"<<state.selected.id
          <<",\"candidate_links\":"<<state.current_world.link_count
          <<",\"outbox_sequence\":"<<state.event_sequence
          <<",\"outbox_acked\":"<<(state.event_acked?"true":"false")
          <<",\"txn_sha256\":\""<<state.head_hash<<"\"}\n";
}
struct StdoutTestSink final:opt::IRouteReceiptSink {
 bool publish(const opt::RouteReceipt& e) noexcept override {
   std::cout<<"{\"event\":\"DIGITAL_ROUTE_RECEIPT\",\"sequence\":"<<e.sequence
            <<",\"change_id\":"<<e.change_id<<",\"revision\":"<<e.revision
            <<",\"state_sha256\":\""<<e.state_sha256<<"\"}\n";
   return std::cout.good();
 }
};
int run_fork(int argc,char**argv){
 if(argc==8&&std::string_view(argv[2])=="sync"){
   cell::Atlas atlas{};
   if(cell::files::load_checked_atlas(argv[3],atlas)!=cell::Status::ok)
     return error("atlas/evidence invalid");
   std::uint32_t source=0;
   const std::string_view raw{argv[7]};
   auto [end,ec]=std::from_chars(raw.data(),raw.data()+raw.size(),source);
   if(ec!=std::errc{}||end!=raw.data()+raw.size()||source==0)
     return error("invalid local source slot");
   opt::DurableRouteLedger ledger;
   if(ledger.open(argv[4],atlas)!=opt::StoreResult::ok)return error("route ledger lock");
   cell::BoundedPositionPort spatial;
   opt::DurableDigitalRouteService controller(ledger,atlas,spatial);
   // The controller should not mutate the culture while delivering receipts.
   opt::DigitalRouteLedger state{};
   if(controller.read(state)!=opt::StoreResult::ok||state.selected.colony==0)
     return error("no digital route event");
   christeye::living::durable::Registry store;
   if(store.open(argv[5])!=christeye::living::durable::Code::ok)return error("WindStore open");
   anaiero::runtime::WeightedQueue<> runtime_queue;
   anaiero::runtime::RuntimeService runtime(runtime_queue);
   opt::RouteReceiptWindPacketCodec codec;
   opt::WindStoreRouteReceiptJournal journal(store,argv[6]);
   opt::RouteRuntimeProjectionPort projector(runtime,source,state.selected.colony);
   opt::RouteReceiptDeliveryService delivery(codec,journal,projector,source);
   if(!delivery.recover()||!controller.deliver(delivery))
     return error("durable route delivery/projection failed");
   if(controller.read(state)!=opt::StoreResult::ok)return error("ledger reread");
   const auto latest=runtime.latest(state.event_sequence*1000000ULL,10000000ULL);
   std::cout<<"{\"schema\":\"anaiero.route.delivery/1\",\"session\":\""
            <<argv[6]<<"\",\"sequence\":"<<state.event_sequence
            <<",\"acked\":"<<(state.event_acked?"true":"false")
            <<",\"projector_revision\":"<<latest.revision
            <<",\"journal_events\":"<<store.event_count()
            <<",\"active\":"<<(state.active?"true":"false")<<"}\n";
   return state.event_acked?0:1;
 }
 if(argc!=5 || std::string_view(argv[2])!="demo"){
   std::cerr<<"usage: anaiero optimize fork demo <atlas.cevatlas> <session-dir>\n"
             <<"       anaiero optimize fork sync <atlas.cevatlas> <session-dir> <windstore-dir> <session-id> <source-slot>\n";
   return 2;
 }
 cell::Atlas atlas{};
 if(cell::files::load_checked_atlas(argv[3],atlas)!=cell::Status::ok)
   return error("atlas or evidence check failed");
 cell::BoundedPositionPort spatial;
 opt::DurableRouteLedger store;
 if(store.open(argv[4],atlas)!=opt::StoreResult::ok)return error("session writer locked");
 opt::DurableDigitalRouteService controller(store,atlas,spatial);
 opt::DigitalRouteLedger ledger{};
 if(controller.read(ledger)==opt::StoreResult::ok){
   describe(ledger);return 0; // restart does not duplicate a committed route
 }
 cell::CellBehaviorService init(atlas,spatial);
 cell::Cell first{};first.id=3;first.kind=cell::Kind::central_neuron;
 first.position={20,17,2};first.energy=100;
 cell::Cell other=first;other.id=4;other.position={23,17,2};
 if(init.add(first)!=cell::Status::ok||init.add(other)!=cell::Status::ok)
   return error("cannot create synthetic starter cells");
 const auto world=init.world();
 opt::ForkPlan plan{};plan.horizon=18;
 plan.trial_context={75,0,64,true};plan.objective_count=3;
 plan.objectives[0]={opt::Measure::directed_links,1,6,true};
 plan.objectives[1]={opt::Measure::viable,2,2,true};
 plan.objectives[2]={opt::Measure::total_learning,40,1,false};
 std::array<opt::ForkOption,3> candidates{{
   {{101,1,3,4,opt::RouteKind::digital_signal,2,1},400,1},
   {{102,1,3,4,opt::RouteKind::digital_signal,3,1},700,2},
   {{103,1,3,4,opt::RouteKind::digital_signal,300,100},900,100}
 }};
 opt::Sw5ForkOptimizationService evaluator(atlas,spatial);
 opt::ForkSelection chosen{};
 const auto hash=pl::sha256_hex(cell::files::serialize_world(world,atlas));
 if(evaluator.analyze(world,1,hash,plan,candidates,chosen)!=opt::ForkStatus::ok)
   return error("no eligible improvement");
 if(controller.initialize(world,1,ledger)!=opt::StoreResult::ok)
   return error("transaction initialization failure");
 std::uint64_t change_id=0;
 if(!controller.commit_selected(chosen,change_id)||!change_id)
   return error("durable digital route change rejected");
 if(controller.read(ledger)!=opt::StoreResult::ok)return error("recovery read failed");
 std::cout<<"{\"experiment\":\"SW5_BRANCHES\",\"baseline_links\":"
          <<chosen.baseline_forecast.links<<",\"predicted_links\":"
          <<chosen.candidate_forecast.links<<",\"winner\":"
          <<chosen.option.route.id<<",\"score\":"<<chosen.score<<"}\n";
 describe(ledger);
 // The pending event is intentionally not acknowledged by the demo; it remains
 // recoverable for a future authenticated delivery adapter.
 return 0;
}
