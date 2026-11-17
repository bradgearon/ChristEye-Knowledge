// SW6.2 native Windows/Linux general observation epoch and recovery tests.
#include "anaiero/infrastructure/windstore_epoch_observation_journal.hpp"
#include "anaiero/infrastructure/epoch_runtime_projection.hpp"
#include "anaiero/services/epoch_observation_delivery_service.hpp"
#include "anaiero/infrastructure/windbus_adapter.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x <<" line "<<__LINE__<<"\n";std::abort();}}while(false)
namespace rt=anaiero::runtime;
namespace dur=christeye::living::durable;
namespace fs=std::filesystem;
static rt::Observation make(std::uint32_t src,std::uint64_t epoch,
  std::uint64_t sequence,rt::Metric metric,std::int32_t value) {
  rt::Observation o{};
  o.source=src;o.source_epoch=epoch;o.sequence=sequence;
  o.colony=1;o.animon=3;o.clock_domain=3;
  o.origin=rt::Origin::simulated;
  o.metric=metric;o.lane=rt::health_metric(metric)?rt::Lane::health:rt::Lane::evidence;
  o.value=value;o.urgency=100;o.importance=100;
  o.acquired_ns=epoch*10000000ULL+sequence*1000000ULL;
  o.evidence_tag=epoch*100+sequence;
  return o;
}
struct Subscriber {
  int delivered=0;
  static void on(void* self,const dur::Event& event) noexcept {
    if(event.kind=="OBSERVATION_RECORDED")
      ++static_cast<Subscriber*>(self)->delivered;
  }
};
static void check_latest(const rt::ColonyState& latest) {
  CHECK(latest.revision==4&&latest.count==4&&latest.sources==3);
  unsigned stale=0,current=0;
  for(std::size_t i=0;i<latest.count;++i) {
    const auto& v=latest.values[i];
    if(v.source==71&&v.source_epoch==1)CHECK(v.quality==rt::Quality::stale);
    if(v.quality==rt::Quality::stale)++stale;
    if(v.quality==rt::Quality::current)++current;
  }
  CHECK(stale==2&&current==2);
}
int main(int argc,char**argv) {
  CHECK(argc==2);
  const auto root=fs::path(argv[1]);fs::remove_all(root);
  rt::EpochObservationCodec codec;
  auto a=make(71,1,1,rt::Metric::energy,100);
  std::array<std::byte,rt::kEpochObservationFrame> wire{};
  CHECK(codec.encode(a,wire));
  rt::Observation decoded{};
  CHECK(codec.decode(wire,decoded));
  CHECK(decoded.source_epoch==1&&decoded.sequence==1&&decoded.value==100);
  std::array<std::byte,rt::kObservationFrame> legacy{};
  std::size_t written=0;
  CHECK(!rt::encode_observation(a,legacy,written)); // never lose epoch silently
  a.source_epoch=0;
  CHECK(rt::encode_observation(a,legacy,written));
  CHECK(rt::decode_observation(legacy,decoded)==rt::Status::ok);
  CHECK(decoded.source_epoch==0);
  a.source_epoch=1;
  auto damaged=wire;
  damaged[56+60]=std::byte{1}; // reserved
  CHECK(!codec.decode(damaged,decoded));
  damaged=wire;
  damaged[24]=std::byte{2}; // header sequence mismatch
  CHECK(!codec.decode(damaged,decoded));

  const auto store_path=root/"events";
  {
    dur::Registry store;CHECK(store.open(store_path)==dur::Code::ok);
    rt::RuntimeService runtime;
    rt::EpochRuntimeProjectionPort view(runtime);
    CHECK(view.enroll({71,3,rt::Origin::simulated,true,false,false,0}));
    CHECK(view.enroll({72,3,rt::Origin::simulated,true,false,false,0}));
    rt::WindStoreEpochObservationJournal journal(store,"multi-source");
    rt::EpochObservationDeliveryService service(codec,journal,view);
    CHECK(service.recover());
    CHECK(!service.submit(make(71,1,2,rt::Metric::energy,99))); // gap
    CHECK(store.event_count()==0);
    CHECK(service.submit(make(71,1,1,rt::Metric::energy,100)));
    CHECK(service.submit(make(72,1,1,rt::Metric::activity,40)));
    CHECK(service.submit(make(71,1,2,rt::Metric::digital_learning,7)));
    CHECK(service.submit(make(71,2,1,rt::Metric::energy,130)));
    CHECK(store.event_count()==4&&service.source_epochs()==3);
    CHECK(service.submit(make(71,2,1,rt::Metric::energy,130))); // idempotent
    CHECK(store.event_count()==4&&view.latest().revision==4);
    CHECK(!service.submit(make(71,2,1,rt::Metric::energy,131))); // ID collision
    CHECK(store.event_count()==4);
    auto bad=make(71,2,2,rt::Metric::digital_learning,9);
    bad.origin=rt::Origin::external_unverified_readonly;
    CHECK(!service.submit(bad)); // permission rejection BEFORE journal
    CHECK(store.event_count()==4);
    check_latest(view.latest());
    Subscriber observed;
    CHECK(store.subscribe(&Subscriber::on,&observed,0,"multi-source",
      "OBSERVATION_RECORDED")==dur::Code::ok);
    CHECK(observed.delivered==4);
    const auto& events=store.events();
    CHECK(store.acknowledge("display",events.back().sequence,events.back().hash)==dur::Code::ok);
    CHECK(store.read_cursor("display").sequence==4);
    CHECK(store.audit_disk());
  }
  {
    dur::Registry store;CHECK(store.open(store_path)==dur::Code::ok);
    rt::RuntimeService runtime;
    rt::EpochRuntimeProjectionPort view(runtime);
    CHECK(view.enroll({71,3,rt::Origin::simulated,true,false,false,0}));
    CHECK(view.enroll({72,3,rt::Origin::simulated,true,false,false,0}));
    rt::WindStoreEpochObservationJournal journal(store,"multi-source");
    rt::EpochObservationDeliveryService service(codec,journal,view);
    CHECK(service.recover()&&service.source_epochs()==3);
    check_latest(view.latest());
    CHECK(store.read_cursor("display").sequence==4);
    CHECK(store.event_count()==4&&store.audit_disk());
    Subscriber resumed;
    CHECK(store.resume(&Subscriber::on,&resumed,"display",
      "multi-source","OBSERVATION_RECORDED")==dur::Code::ok);
    CHECK(resumed.delivered==0);
    CHECK(service.submit(make(72,1,2,rt::Metric::activity,50)));
    CHECK(store.event_count()==5);
    CHECK(view.latest().revision==5);
  }
  std::cout<<"SW6.2 multi-source epochs, WindStore, replay, authorization PASS\n";
}
