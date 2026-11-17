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
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
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
static rt::Observation observation(std::uint64_t sequence){
  auto value=observation();value.sequence=sequence;
  value.acquired_ns=sequence*1000000;
  value.value=static_cast<std::int32_t>(sequence);
  return value;
}
// Completion ordering is controlled explicitly; no scheduling sleeps make
// these service failure/retry tests pass or fail.
struct ControlledStore final:rt::IAsyncObservationStore {
  std::array<rt::AsyncCommitReceipt,512> work{},ready{};
  std::size_t submitted=0,produced=0,consumed=0;
  bool reject=false;
  bool enqueue(const rt::Observation& event,std::uint64_t& ticket)noexcept override {
    ticket=0;
    if(reject||submitted==work.size())return false;
    ticket=static_cast<std::uint64_t>(submitted+1);
    work[submitted++]={ticket,event,false};return true;
  }
  bool try_receive(rt::AsyncCommitReceipt& receipt)noexcept override {
    if(consumed==produced)return false;
    receipt=ready[consumed++];return true;
  }
  std::size_t queued()const noexcept override {return submitted-consumed;}
  void complete(std::size_t index,bool durable=true,
                rt::AsyncCommitOutcome outcome=rt::AsyncCommitOutcome::unspecified){
    CHECK(index<submitted&&produced<ready.size());
    auto receipt=work[index];receipt.durable=durable;receipt.outcome=outcome;
    ready[produced++]=receipt;
  }
};
struct ControlledProjection final:rt::IObservationEpochProjection {
  std::array<rt::Observation,512> applied{};
  std::size_t count=0,attempts=0,fail_next=0;
  bool enroll(rt::SourcePermission)noexcept override{return true;}
  bool can_accept(const rt::Observation& event)const noexcept override {
    return rt::valid_observation(event)&&event.source_epoch!=0;
  }
  bool apply(const rt::Observation& event)noexcept override {
    ++attempts;
    if(fail_next){--fail_next;return false;}
    if(count==applied.size())return false;
    applied[count++]=event;return true;
  }
  rt::ColonyState latest()const noexcept override {
    rt::ColonyState state{};state.revision=count;return state;
  }
};
struct EvidenceHook final:anaiero::contracts::IServiceExtension {
  std::array<anaiero::contracts::ServiceEvent,64> events{};
  std::size_t count=0;
  void on_service_event(const anaiero::contracts::ServiceEvent& event)noexcept override {
    CHECK(count<events.size());events[count++]=event;
  }
};
static void service_order_and_retry(){
  ControlledStore store;ControlledProjection projection;EvidenceHook hook;
  rt::AsyncObservationCommitService service(store,projection,3,&hook);
  CHECK(service.submit(observation(1)));
  CHECK(!service.submit(observation(3))); // cannot skip a reserved source step
  CHECK(service.submit(observation(2))&&service.submit(observation(3)));
  CHECK(!service.submit(observation(4))&&service.outstanding()==3);
  CHECK(!service.poll()&&projection.count==0); // enqueue is not durability
  store.complete(2);
  CHECK(!service.poll()&&projection.count==0); // later receipt cannot overtake
  CHECK(service.status().awaiting==2&&service.status().durable_ready==1);
  store.complete(0);projection.fail_next=1;
  CHECK(!service.poll()&&projection.count==0);
  CHECK(service.failure_kind()==rt::AsyncObservationFailure::projection_retry);
  rt::AsyncCommitReceipt failure{};
  CHECK(service.failure_receipt(failure)&&failure.durable&&failure.ticket==1);
  CHECK(!service.failed()&&service.outstanding()==3);
  store.complete(1);service.close();
  CHECK(!service.submit(observation(4)));
  CHECK(service.poll()&&projection.count==3&&!service.pending());
  CHECK(store.submitted==3&&projection.attempts==4); // retry never recommits
  for(std::size_t i=0;i<3;++i)CHECK(projection.applied[i].sequence==i+1);
  CHECK(!service.failure_receipt(failure));
  const auto status=service.status();
  CHECK(status.closed&&!status.failed&&status.submitted==3&&status.projected==3);
  CHECK(status.durable==3&&status.projection_retries==1&&status.rejected==3);
  std::uint64_t committed=0;
  for(std::size_t i=0;i<hook.count;++i){
    CHECK(hook.events[i].service==anaiero::contracts::ServiceId::evidence);
    if(hook.events[i].phase==anaiero::contracts::ServicePhase::committed)
      CHECK(hook.events[i].revision==++committed);
  }
  CHECK(committed==3);
}
static void service_failure_retention(){
  ControlledStore store;ControlledProjection projection;
  rt::AsyncObservationCommitService service(store,projection,3);
  for(std::uint64_t sequence=1;sequence<=3;++sequence)
    CHECK(service.submit(observation(sequence)));
  store.complete(0);
  store.complete(1,false,rt::AsyncCommitOutcome::persistence_failed);
  store.complete(2,false,rt::AsyncCommitOutcome::blocked_by_previous_failure);
  CHECK(service.poll()&&projection.count==1);
  CHECK(service.failed()&&service.pending()&&service.outstanding()==2);
  CHECK(service.failure_kind()==rt::AsyncObservationFailure::persistence);
  CHECK(!service.submit(observation(4)));
  rt::AsyncCommitReceipt failed{};bool received=false;
  CHECK(service.failure_receipt(failed)&&failed.ticket==2&&!failed.durable);
  CHECK(service.inspect(0,failed,received)&&received&&failed.event.sequence==2);
  CHECK(service.inspect(1,failed,received)&&received&&failed.event.sequence==3);
  CHECK(failed.outcome==rt::AsyncCommitOutcome::blocked_by_previous_failure);
  CHECK(service.status().failed_entries==2&&service.status().persistence_failures==2);
  CHECK(!service.poll()&&projection.count==1); // failed entries are never projected
}
static void service_invalid_receipts_and_configuration(){
  {
    ControlledStore store;ControlledProjection projection;
    rt::AsyncObservationCommitService service(store,projection,2);
    CHECK(service.submit(observation(1))&&service.submit(observation(2)));
    store.complete(0);store.ready[0].event.value=999;
    store.complete(1);
    CHECK(!service.poll()&&service.failed()&&projection.count==0);
    CHECK(service.failure_kind()==rt::AsyncObservationFailure::receipt_protocol);
    CHECK(service.outstanding()==2&&service.status().protocol_failures==1);
    rt::AsyncCommitReceipt pending{};bool received=false;
    CHECK(service.inspect(0,pending,received)&&!received&&pending.event.value==1);
  }
  {
    ControlledStore store;ControlledProjection projection;
    rt::AsyncObservationCommitService service(store,projection);
    CHECK(service.submit(observation(1))&&!service.submit(observation(2)));
    store.complete(0);store.complete(0); // duplicate receipt is not a second ACK
    CHECK(!service.poll()&&service.failed()&&projection.count==0);
  }
  {
    ControlledStore store;ControlledProjection projection;
    rt::AsyncObservationCommitService zero(store,projection,0),large(store,projection,257);
    CHECK(!zero.submit(observation(1))&&!large.submit(observation(1)));
    CHECK(!zero.status().configuration_valid&&!large.status().configuration_valid);
    CHECK(store.submitted==0);
  }
  {
    ControlledStore store;ControlledProjection projection;store.reject=true;
    rt::AsyncObservationCommitService service(store,projection,4);
    CHECK(!service.submit(observation(1))&&!service.pending());
    CHECK(service.status().rejected==1&&!service.failed());
  }
}
struct CompletionSignal final:rt::IAsyncCompletionSignal {
  std::mutex mutex;std::condition_variable cv;std::uint64_t generation=0;
  rt::IAsyncObservationStore* store=nullptr;
  void notify()noexcept override {
    // Safe read proves callbacks are outside the worker's own mutex.
    if(store)(void)store->queued();
    {std::lock_guard<std::mutex> lock(mutex);++generation;}
    cv.notify_all();
  }
  bool wait(std::uint64_t wanted){
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock,std::chrono::seconds(5),[&]{return generation>=wanted;});
  }
};
struct GatedJournal final:rt::IObservationEpochJournal {
  std::mutex mutex;std::condition_variable cv;
  std::array<rt::Observation,8> committed{};
  bool entered=false,released=false,succeed=true;
  std::size_t calls=0;
  bool commit(const rt::Observation& event,std::span<const std::byte>)noexcept override {
    std::unique_lock<std::mutex> lock(mutex);
    entered=true;cv.notify_all();
    cv.wait(lock,[&]{return released;});
    committed[calls++]=event;return succeed;
  }
  bool replay(rt::IObservationEpochReplay&)noexcept override{return false;}
  bool wait_entered(){
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock,std::chrono::seconds(5),[&]{return entered;});
  }
  void release(){
    {std::lock_guard<std::mutex> lock(mutex);released=true;}
    cv.notify_all();
  }
};
static void worker_notification_backpressure_shutdown(){
  rt::EpochObservationCodec codec;GatedJournal journal;CompletionSignal signal;
  rt::AsyncEpochJournalWorker<3> worker(journal,codec,&signal);signal.store=&worker;
  std::uint64_t ticket=0;
  CHECK(worker.enqueue(observation(1),ticket)&&ticket==1);
  CHECK(journal.wait_entered());
  CHECK(worker.enqueue(observation(2),ticket)&&worker.enqueue(observation(3),ticket));
  CHECK(!worker.enqueue(observation(4),ticket)&&ticket==0);
  auto status=worker.status();
  CHECK(status.outstanding==3&&status.active==1&&status.queued==2&&status.receipts==0);
  CHECK(!worker.wait_for_completion(std::chrono::milliseconds(0)));
  worker.request_stop();
  CHECK(!worker.enqueue(observation(4),ticket));
  journal.release();
  CHECK(signal.wait(3)); // real completion events; receipt need not be polled
  CHECK(worker.wait_for_completion(std::chrono::milliseconds(0)));
  worker.shutdown(); // no receipt consumption needed to let shutdown finish
  CHECK(signal.wait(4)); // three receipts and a stopped notification
  status=worker.status();
  CHECK(status.stopped&&status.stopping&&status.accepted==3&&status.durable==3);
  CHECK(status.receipts==3&&status.outstanding==3&&status.active==0&&status.queued==0);
  CHECK(journal.calls==3&&worker.dropped_completions()==0);
  for(std::uint64_t expected=1;expected<=3;++expected){
    rt::AsyncCommitReceipt receipt{};
    CHECK(worker.try_receive(receipt)&&receipt.ticket==expected&&receipt.durable);
    CHECK(receipt.event.sequence==expected&&receipt.outcome==rt::AsyncCommitOutcome::durable);
  }
  CHECK(worker.status().outstanding==0);
  CHECK(!worker.wait_for_completion(std::chrono::seconds(5))); // stopped, returns promptly
}
static void worker_write_failure_blocks_following_work(){
  rt::EpochObservationCodec codec;GatedJournal journal;CompletionSignal signal;
  journal.succeed=false;
  rt::AsyncEpochJournalWorker<3> worker(journal,codec,&signal);signal.store=&worker;
  std::uint64_t ticket=0;
  CHECK(worker.enqueue(observation(1),ticket)&&journal.wait_entered());
  CHECK(worker.enqueue(observation(2),ticket)&&worker.enqueue(observation(3),ticket));
  journal.release();CHECK(signal.wait(3));worker.shutdown();
  CHECK(journal.calls==1); // never append after the failed first observation
  CHECK(worker.status().write_failed&&worker.status().failed==3);
  CHECK(!worker.enqueue(observation(4),ticket));
  for(std::uint64_t expected=1;expected<=3;++expected){
    rt::AsyncCommitReceipt receipt{};
    CHECK(worker.try_receive(receipt)&&!receipt.durable&&receipt.ticket==expected);
    CHECK(receipt.event.sequence==expected);
    CHECK(receipt.outcome==(expected==1?rt::AsyncCommitOutcome::persistence_failed:
      rt::AsyncCommitOutcome::blocked_by_previous_failure));
  }
  CHECK(worker.dropped_completions()==0);
}
static void durable_batch_replay(const fs::path& path){
  {
    db::Registry registry;CHECK(registry.open(path)==db::Code::ok);
    rt::EpochObservationCodec codec;
    rt::WindStoreEpochObservationJournal journal(registry,"async-batch");
    rt::WeightedQueue<> queue;rt::RuntimeService runtime(queue);
    rt::EpochRuntimeProjectionPort projection(runtime);
    CHECK(projection.enroll({61,3,rt::Origin::simulated,true,false,false,0}));
    rt::AsyncEpochJournalWorker<4> worker(journal,codec);
    rt::AsyncObservationCommitService service(worker,projection,4);
    for(std::uint64_t sequence=1;sequence<=4;++sequence)
      CHECK(service.submit(observation(sequence)));
    CHECK(service.outstanding()==4);
    while(service.pending()){
      CHECK(worker.wait_for_completion(std::chrono::seconds(5)));
      (void)service.poll();CHECK(!service.failed());
    }
    worker.shutdown();
    CHECK(service.status().projected==4&&runtime.latest(0,UINT64_MAX).revision==4);
    CHECK(registry.event_count()==4&&registry.audit_disk());
  }
  {
    db::Registry registry;CHECK(registry.open(path)==db::Code::ok);
    rt::EpochObservationCodec codec;
    rt::WindStoreEpochObservationJournal journal(registry,"async-batch");
    rt::WeightedQueue<> queue;rt::RuntimeService runtime(queue);
    rt::EpochRuntimeProjectionPort projection(runtime);
    CHECK(projection.enroll({61,3,rt::Origin::simulated,true,false,false,0}));
    rt::EpochObservationDeliveryService replay(codec,journal,projection);
    CHECK(replay.recover());
    const auto state=runtime.latest(0,UINT64_MAX);
    CHECK(state.revision==4&&state.values[0].sequence==4&&state.sequence_gaps==0);
    CHECK(registry.event_count()==4&&registry.audit_disk());
  }
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
  bounded.epoch=1;bounded.value=70;
  for(auto* q:{&before,&inflow,&outflow,&bounded}){
    q->model_sha256.fill('a');q->model_sha256.back()='\0';
  }
  CHECK(unit.conservation({before,inflow,outflow,bounded,0}));
  bounded.model_sha256[0]='b';
  CHECK(!unit.conservation({before,inflow,outflow,bounded,0}));
  sci::ConservationContext context{};
  const std::array<const sci::Quantity*,4> terms{{&before,&inflow,&outflow,&bounded}};
  for(std::size_t i=0;i<terms.size();++i)
    context.terms[i]={terms[i]->source,terms[i]->epoch,terms[i]->model_sha256};
  context.transform_sha256.fill('c');context.transform_sha256.back()='\0';
  CHECK(unit.conservation({before,inflow,outflow,bounded,0},&context));
  context.terms[3].epoch=2;
  CHECK(!unit.conservation({before,inflow,outflow,bounded,0},&context));
  context.terms[3].epoch=1;context.transform_sha256[0]='\0';
  CHECK(!unit.conservation({before,inflow,outflow,bounded,0},&context));
}
int main(int argc,char**argv){
  CHECK(argc==2);
  units();
  service_order_and_retry();
  service_failure_retention();
  service_invalid_receipts_and_configuration();
  worker_notification_backpressure_shutdown();
  worker_write_failure_blocks_following_work();
  const auto root=fs::path(argv[1]);fs::remove_all(root);
  durable_batch_replay(root/"batch");
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
    CHECK(worker.wait_for_completion(std::chrono::seconds(5)));
    const bool completed=service.poll();
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
