#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/services/runtime_service.hpp"
// SW6.1 integration: persistent route -> typed WindPacket -> WindStore -> projector.
#include "anaiero/services/durable_digital_route_service.hpp"
#include "anaiero/services/route_receipt_delivery_service.hpp"
#include "anaiero/infrastructure/durable_route_ledger.hpp"
#include "anaiero/infrastructure/windstore_route_journal.hpp"
#include "anaiero/infrastructure/route_runtime_projection.hpp"
#include "anaiero/infrastructure/route_receipt_codec.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#define CHECK(e) do {if(!(e)){std::cerr<<"FAILED " #e <<" at "<<__LINE__<<"\n";std::abort();}}while(false)
namespace opt=anaiero::optimization;
namespace rt=anaiero::runtime;
namespace cells=anaiero::cells;
namespace fs=std::filesystem;
namespace pl=christeye::living::plans;
namespace dur=christeye::living::durable;
static std::uint64_t projected(const rt::RuntimeService& runtime,int active){
  const auto state=runtime.latest(100000000ULL,1000000000ULL);
  for(std::size_t i=0;i<state.count;++i){
    const auto& v=state.values[i];
    if(v.source==31&&v.metric==rt::Metric::digital_route_active&&
       v.value==active&&v.quality==rt::Quality::current)return v.sequence;
  }
  return 0;
}
struct DurableSubscriber {
  int received=0;
  static void callback(void* context,const dur::Event& event) noexcept {
    if(event.kind=="DIGITAL_ROUTE_RECEIPT")
      ++static_cast<DurableSubscriber*>(context)->received;
  }
};
struct LiveSubscriber final:rt::IUpdateObserver {
  int received=0;
  void on_revision(std::uint64_t revision,const rt::Observation& o) noexcept override {
    CHECK(revision>0);
    if(o.metric==rt::Metric::digital_route_active)++received;
  }
};
int main(int argc,char**argv){
  CHECK(argc==3);
  cells::Atlas atlas{};
  CHECK(cells::files::load_checked_atlas(argv[1],atlas)==cells::Status::ok);
  cells::BoundedPositionPort spatial;
  cells::CellBehaviorService seeds(atlas,spatial);
  cells::Cell a{};a.id=3;a.kind=cells::Kind::central_neuron;
  a.position={20,17,2};a.energy=100;
  cells::Cell b=a;b.id=4;b.position={23,17,2};
  CHECK(seeds.add(a)==cells::Status::ok&&seeds.add(b)==cells::Status::ok);
  const auto initial=seeds.world();
  const auto root=fs::path(argv[2]);fs::remove_all(root);
  const auto route_dir=root/"route",store_dir=root/"store";
  constexpr std::uint32_t source=31;
  std::uint64_t change=0;
  {
    opt::DurableRouteLedger ledger;
    CHECK(ledger.open(route_dir,atlas)==opt::StoreResult::ok);
    opt::DurableDigitalRouteService controller(ledger,atlas,spatial);
    opt::DigitalRouteLedger state{};
    CHECK(controller.initialize(initial,7,state)==opt::StoreResult::ok);
    opt::Decision decision{};decision.status=opt::Status::ok;
    decision.snapshot_revision=7;decision.score=200;
    decision.source_world_sha256=pl::sha256_hex(cells::files::serialize_world(initial,atlas));
    decision.selected={102,1,3,4,opt::RouteKind::digital_signal,3,1,700};
    CHECK(controller.commit(decision,change)&&change==1);
    dur::Registry wind;
    CHECK(wind.open(store_dir)==dur::Code::ok);
    DurableSubscriber durable;
    CHECK(wind.subscribe(&DurableSubscriber::callback,&durable,0,
      "crest-session","DIGITAL_ROUTE_RECEIPT")==dur::Code::ok);
    rt::WeightedQueue<> runtime_queue;rt::RuntimeService runtime(runtime_queue);
    LiveSubscriber live;runtime.set_observer(&live);
    opt::RouteReceiptWindPacketCodec codec;
    opt::WindStoreRouteReceiptJournal journal(wind,"crest-session");
    opt::RouteRuntimeProjectionPort projection(runtime,source,1);
    opt::RouteReceiptDeliveryService bridge(codec,journal,projection,source);
    CHECK(!controller.deliver(bridge)); // no implicit ACK before replay
    CHECK(bridge.recover());
    CHECK(controller.deliver(bridge));
    CHECK(controller.read(state)==opt::StoreResult::ok&&state.event_acked);
    CHECK(projected(runtime,1)==1&&wind.event_count()==1);
    CHECK(durable.received==1&&live.received==1);
    CHECK(controller.deliver(bridge)&&live.received==1&&wind.event_count()==1);

    opt::RouteReceipt receipt{opt::RouteEventKind::route_committed,1,change,1,
      decision.selected,decision.source_world_sha256};
    std::array<std::byte,opt::kRouteReceiptPacketBytes> bytes{};
    CHECK(codec.encode(receipt,source,bytes));
    opt::RouteReceipt decoded{};
    CHECK(codec.decode(bytes,source,decoded));
    CHECK(opt::same_route_receipt(receipt,decoded));
    CHECK(!codec.decode(bytes,source+1,decoded));
    auto changed=bytes;changed[24]=std::byte{0};
    CHECK(!codec.decode(changed,source,decoded));
    changed=bytes;changed[60]=std::byte{255};
    CHECK(!codec.decode(changed,source,decoded));
    receipt.state_sha256=std::string(64,'0');
    CHECK(codec.encode(receipt,source,bytes));
    CHECK(!journal.append(receipt,bytes)); // collision with same session/seq

    CHECK(controller.advance({75,0,64,true},18,spatial));
    ledger.fault_after_object_once(); // after publish, before ledger ACK HEAD
    CHECK(!controller.deliver(bridge));
    CHECK(controller.read(state)==opt::StoreResult::ok&&!state.event_acked);
    CHECK(wind.event_count()==2&&projected(runtime,1)==2&&live.received==2);
    CHECK(controller.deliver(bridge)); // same receipt replayed once
    CHECK(wind.event_count()==2&&live.received==2);
    CHECK(controller.read(state)==opt::StoreResult::ok&&state.event_acked);
    const auto tick=state.current_world.tick;
    CHECK(controller.revert(change));
    CHECK(controller.deliver(bridge));
    CHECK(controller.read(state)==opt::StoreResult::ok);
    CHECK(!state.active&&state.event_acked&&state.current_world.tick==tick);
    CHECK(projected(runtime,0)==3&&wind.event_count()==3);
    CHECK(durable.received==3&&live.received==3);
    const auto& events=wind.events();
    CHECK(wind.acknowledge("viewer",events.back().sequence,events.back().hash)==dur::Code::ok);
    CHECK(wind.read_cursor("viewer").sequence==3);
    DurableSubscriber resumed;
    CHECK(wind.resume(&DurableSubscriber::callback,&resumed,"viewer",
      "crest-session","DIGITAL_ROUTE_RECEIPT")==dur::Code::ok);
    CHECK(resumed.received==0);
  }
  {
    opt::DurableRouteLedger ledger;
    CHECK(ledger.open(route_dir,atlas)==opt::StoreResult::ok);
    opt::DurableDigitalRouteService controller(ledger,atlas,spatial);
    dur::Registry wind;
    CHECK(wind.open(store_dir)==dur::Code::ok);
    rt::WeightedQueue<> runtime_queue;rt::RuntimeService runtime(runtime_queue);
    LiveSubscriber live;runtime.set_observer(&live);
    opt::RouteReceiptWindPacketCodec codec;
    opt::WindStoreRouteReceiptJournal journal(wind,"crest-session");
    opt::RouteRuntimeProjectionPort projection(runtime,source,1);
    opt::RouteReceiptDeliveryService bridge(codec,journal,projection,source);
    CHECK(bridge.recover()&&bridge.acknowledged_sequence()==3);
    CHECK(projected(runtime,0)==3&&live.received==3);
    CHECK(controller.deliver(bridge)&&wind.event_count()==3&&live.received==3);
    CHECK(wind.read_cursor("viewer").sequence==3&&wind.audit_disk());
  }
  std::cout<<"SW6.1 WindPacket, durable cursor, projector ACK, replay PASS\n";
}
