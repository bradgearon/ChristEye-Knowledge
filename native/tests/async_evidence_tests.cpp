// Nonblocking durable commit, replay, unit safety and snapshot index.
#include "anaiero/infrastructure/async_epoch_journal_worker.hpp"
#include "anaiero/services/async_observation_commit_service.hpp"
#include "anaiero/infrastructure/windstore_epoch_observation_journal.hpp"
#include "anaiero/infrastructure/epoch_runtime_projection.hpp"
#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/services/runtime_service.hpp"
#include "anaiero/services/epoch_observation_delivery_service.hpp"
#include "anaiero/services/unit_conversion_service.hpp"
#include "anaiero/services/checkpoint_catalog_service.hpp"
#include "anaiero/services/journal_retention_review_service.hpp"
#include "anaiero/infrastructure/windstore_checkpoint_catalog.hpp"
#include "christeye/living/plan_hash.hpp"
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#define CHECK(e) do{if(!(e)){std::cerr<<"FAILED " #e <<" at "<<__LINE__<<"\n";std::abort();}}while(false)
namespace rt=anaiero::runtime;
namespace eng=anaiero::engine;
namespace sci=anaiero::science;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
static rt::Observation observation(){
  rt::Observation o{};o.colony=1;o.animon=3;o.source=61;
  o.clock_domain=3;o.source_epoch=1;o.sequence=1;
  o.metric=rt::Metric::energy;o.lane=rt::Lane::health;
  o.origin=rt::Origin::simulated;o.value=100;o.acquired_ns=1000000;
  return o;
}
static void units(){
  sci::UnitConversionService unit;
  sci::Quantity a{};a.value=65;a.uncertainty=1;
  a.unit=sci::Unit::millivolt;a.source=1;a.epoch=1;
  sci::Quantity b{};
  CHECK(unit.convert(a,sci::Unit::microvolt,b)&&
        b.value==65000&&b.uncertainty==1000);
  CHECK(!unit.convert(a,sci::Unit::millimolar,b));
  CHECK(!unit.convert(a,sci::Unit::dimensionless_proxy,b));
  sci::Quantity fine=a;fine.value=651;fine.unit=sci::Unit::microvolt;
  CHECK(!unit.convert(fine,sci::Unit::millivolt,b));
  auto before=a,bounded=a,inflow=a,outflow=a;
  before.value=65;inflow.value=10;outflow.value=5;bounded.value=70;
  CHECK(unit.conservation({before,inflow,outflow,bounded,0}));
  bounded.value=80;
  CHECK(!unit.conservation({before,inflow,outflow,bounded,1}));
  bounded.epoch=2;
  CHECK(!unit.conservation({before,inflow,outflow,bounded,100}));
}
int main(int argc,char**argv){
  CHECK(argc==2);
  units();
  const auto root=fs::path(argv[1]);fs::remove_all(root);
  const auto store_path=root/"journal";
  {
    db::Registry registry;CHECK(registry.open(store_path)==db::Code::ok);
    rt::EpochObservationCodec codec;
    rt::WindStoreEpochObservationJournal journal(registry,"async-session");
    rt::WeightedQueue<> queue;
    rt::RuntimeService runtime(queue);
    rt::EpochRuntimeProjectionPort projection(runtime);
    CHECK(projection.enroll({61,3,rt::Origin::simulated,true,false,false,0}));
    rt::AsyncEpochJournalWorker<4> worker(journal,codec);
    rt::AsyncObservationCommitService service(worker,projection);
    const auto e=observation();
    CHECK(service.submit(e));
    CHECK(!service.submit(e)); // single in-flight durable commit
    bool completed=false;
    for(int i=0;i<1000&&!completed;++i){
      completed=service.poll();
      if(!completed)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(completed&&!service.pending()&&!service.failed());
    CHECK(registry.event_count()==1);
    CHECK(runtime.latest(0,UINT64_MAX).revision==1);
    CHECK(worker.dropped_completions()==0);
    worker.shutdown();
    CHECK(registry.audit_disk());
  }
  {
    db::Registry registry;CHECK(registry.open(store_path)==db::Code::ok);
    rt::WeightedQueue<> queue;rt::RuntimeService runtime(queue);
    rt::EpochRuntimeProjectionPort projection(runtime);
    CHECK(projection.enroll({61,3,rt::Origin::simulated,true,false,false,0}));
    rt::EpochObservationCodec codec;
    rt::WindStoreEpochObservationJournal journal(registry,"async-session");
    rt::EpochObservationDeliveryService replay(codec,journal,projection);
    CHECK(replay.recover());
    CHECK(runtime.latest(0,UINT64_MAX).revision==1);
    CHECK(registry.event_count()==1);
    CHECK(registry.audit_disk());
    eng::WindStoreCheckpointCatalog catalog(registry);
    eng::CheckpointCatalogService service(catalog);
    const std::string snapshot="CEVCELLSTATE/1\nsynthetic immutable sample\n";
    const auto digest=christeye::living::plans::sha256_hex(snapshot);
    const eng::CheckpointRecord record{101,61,1,1,std::string(64,'a'),digest};
    CHECK(service.commit(record,snapshot));
    CHECK(!service.commit(record,snapshot)); // immutable revision collision
    std::string bytes;eng::CheckpointRecord restored{};
    CHECK(service.restore(101,1,restored,bytes));
    CHECK(bytes==snapshot&&restored.checkpoint_sha256==digest);
    CHECK(registry.audit_disk());
    eng::JournalRetentionReviewService policy;
    const std::array<std::uint64_t,2> readers{{0,2}};
    CHECK(!policy.review(2,readers,2).eligible);
    const std::array<std::uint64_t,2> healthy{{2,3}};
    const auto candidate=policy.review(2,healthy,2);
    CHECK(candidate.eligible&&candidate.safe_through==2);
    CHECK(registry.event_count()==2); // no destructive compaction
  }
  {
    db::Registry registry;CHECK(registry.open(store_path)==db::Code::ok);
    eng::WindStoreCheckpointCatalog catalog(registry);
    eng::CheckpointCatalogService service(catalog);
    eng::CheckpointRecord meta{};std::string bytes;
    CHECK(service.restore(101,1,meta,bytes));
    CHECK(bytes=="CEVCELLSTATE/1\nsynthetic immutable sample\n");
    CHECK(registry.audit_disk());
  }
  std::cout<<"async durable journal, source replay, unit safety, snapshot index PASS\n";
}
