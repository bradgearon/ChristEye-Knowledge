// Native segmented storage: bounded recovery, provenance, reader lag and crash cuts.
#include "christeye/living/durable_registry.hpp"
#include "anaiero/infrastructure/windstore_epoch_observation_journal.hpp"
#include "anaiero/infrastructure/windstore_checkpoint_catalog.hpp"
#include "anaiero/infrastructure/epoch_runtime_projection.hpp"
#include "anaiero/infrastructure/async_epoch_journal_worker.hpp"
#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/infrastructure/session_files.hpp"
#include "anaiero/services/epoch_observation_delivery_service.hpp"
#include "anaiero/services/observation_checkpoint_service.hpp"
#include "anaiero/services/runtime_service.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
namespace d=christeye::living::durable;
namespace rt=anaiero::runtime;
namespace fs=std::filesystem;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x <<" line "<<__LINE__<<'\n';std::exit(1);}}while(false)
static void raw(const fs::path& path,std::string_view bytes){
  std::ofstream out(path,std::ios::binary|std::ios::trunc);CHECK(out);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.close();CHECK(out);
}
static d::Event append(d::Registry& db,std::uint64_t tick,std::string payload={}){
  if(payload.empty())payload="payload-"+std::to_string(tick);
  const auto object=db.put(payload);CHECK(object.code==d::Code::ok);
  CHECK(db.publish("MODEL_EVALUATION","long-run",object.hash,"SIMULATED",tick,"step","solver").code==d::Code::ok);
  return db.events().back();
}
static std::size_t files(const fs::path& dir){std::size_t n=0;for(const auto& entry:fs::directory_iterator(dir))if(entry.is_regular_file())++n;return n;}
static void long_run(const fs::path& root){
  const auto started=std::chrono::steady_clock::now();
  d::Registry db;CHECK(db.open(root,d::StorageOptions::sustained(32))==d::Code::ok);
  d::Registry locked;CHECK(locked.open(root)==d::Code::locked);
  d::StorageReservation held;CHECK(db.reserve(64,1024*1024,held));CHECK(db.capacity().reserved_events==64);
  CHECK(db.release(held));CHECK(!db.release(held));
  std::uint64_t expected=0;std::size_t peak=0;
  for(std::uint64_t i=1;i<=4203;++i){const auto e=append(db,i,"shared-payload");expected+=e.source_tick;
    peak=std::max(peak,db.events().size());CHECK(db.events().size()<=32);
    if(i%32==0){CHECK(db.save_replay_checkpoint(std::to_string(expected)));CHECK(db.archive_ready()==d::Code::ok);}
  }
  CHECK(db.event_count()==4203&&peak==32&&db.capacity().archived_segments==130);
  d::Event last;bool found=false;CHECK(db.latest_event("MODEL_EVALUATION","long-run","step",last,found)&&found&&last.source_tick==4203);
  d::Event old;CHECK(db.find_event("MODEL_EVALUATION","long-run","step","solver",1,old,found)&&found&&old.sequence==1);
  CHECK(db.acknowledge("late-reader",old.sequence,old.hash)==d::Code::ok);
  CHECK(db.audit_disk());
  d::ReplayCheckpoint checkpoint;std::string data;CHECK(db.load_replay_checkpoint(checkpoint,data,found)&&found&&checkpoint.sequence==4192);
  auto folded=static_cast<std::uint64_t>(std::stoull(data));std::size_t read=0;
  CHECK(db.visit_events(checkpoint.sequence,[&](const d::Event& e){folded+=e.source_tick;++read;return true;}));
  CHECK(folded==expected&&read==11);
  db.close();CHECK(db.open(root)==d::Code::ok&&db.is_segmented());CHECK(db.event_count()==4203&&db.events().size()==11);
  CHECK(db.read_cursor("late-reader").sequence==1);
  CHECK(db.latest_event("MODEL_EVALUATION","long-run","step",last,found)&&found&&last.source_tick==4203);
  CHECK(db.can_append(20000,32*1024*1024));
  std::size_t replayed=0;CHECK(db.visit_events(0,[&](const d::Event&){++replayed;return true;}));CHECK(replayed==4203);
  const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
  std::cout<<"long-run events=4203 peak_resident="<<peak<<" restart_resident="<<db.events().size()<<" tail_replay="<<read<<" elapsed_ms="<<ms<<'\n';
}
static void retained_references(const fs::path& root){
  d::Registry db;CHECK(db.open(root,d::StorageOptions::sustained(8))==d::Code::ok);
  std::vector<d::Event> events;std::string old_payload;
  const auto child=db.put("checkpoint-child");CHECK(child.code==d::Code::ok);
  const std::array<std::string,1> refs{child.hash};
  const auto parent=db.put_referenced("checkpoint-parent",refs);CHECK(parent.code==d::Code::ok);
  CHECK(db.publish("MODEL_EVALUATION","long-run",parent.hash,"SIMULATED",1,"step","solver").code==d::Code::ok);events.push_back(db.events().back());
  for(std::uint64_t i=2;i<=25;++i){events.push_back(append(db,i));if(i==2)old_payload=events.back().object_hash;}
  const auto orphan=db.put("orphan");const auto pinned=db.put("active-operation-checkpoint");CHECK(orphan.code==d::Code::ok&&pinned.code==d::Code::ok);
  const std::array<std::string,1> pin{pinned.hash};CHECK(db.pin("operation-foreign",pin)==d::Code::ok);
  CHECK(db.save_replay_checkpoint("projection-at-25"));
  CHECK(db.acknowledge("slow",7,events[6].hash)==d::Code::ok);
  CHECK(db.archive_ready()==d::Code::ok&&db.capacity().archived_segments==0);
  d::ArchiveStats stats;CHECK(db.collect_objects(stats)==d::Code::ok);
  CHECK(stats.moved_objects==0&&!fs::exists(root/"objects"/orphan.hash)&&fs::exists(root/"objects"/pinned.hash));
  CHECK(db.acknowledge("slow",17,events[16].hash)==d::Code::ok);
  CHECK(db.archive_ready()==d::Code::ok&&db.capacity().archived_segments==2);
  CHECK(db.collect_objects(stats)==d::Code::ok&&stats.moved_objects>=16);
  CHECK(!fs::exists(root/"objects"/old_payload)&&fs::exists(root/"archive_objects"/old_payload));
  CHECK(fs::exists(root/"archive_objects"/child.hash));
  CHECK(fs::exists(root/"objects"/events[16].object_hash)); // unacknowledged active evidence remains hot
  std::string out;CHECK(db.get(child.hash,out)&&out=="checkpoint-child");CHECK(db.get(pinned.hash,out));
  CHECK(db.unpin("operation-foreign")==d::Code::ok);CHECK(db.collect_objects(stats)==d::Code::ok&&!db.get(pinned.hash,out));
  CHECK(db.audit_disk());
  // Missing transitive references fail closed before reclamation can drop data.
  raw(root/"references"/(child.hash+".refs"),"corrupt");
  CHECK(!db.audit_disk());CHECK(db.collect_objects(stats)==d::Code::corrupt_journal);
}
static void crash_cuts(const fs::path& root){
  const std::array cuts{d::FaultSite::after_segment_prepare,d::FaultSite::after_rotation_commit,
    d::FaultSite::after_journal_sync,d::FaultSite::after_head_temp_sync,d::FaultSite::after_head_commit};
  unsigned case_number=0;
  for(const auto fault:cuts){const auto dir=root/std::to_string(case_number++);d::Registry db;
    CHECK(db.open(dir,d::StorageOptions::sustained(8))==d::Code::ok);
    for(std::uint64_t i=1;i<=8;++i)append(db,i);
    const auto object=db.put("ninth");CHECK(object.code==d::Code::ok);db.inject_next_publish(fault);
    CHECK(db.publish("MODEL_EVALUATION","long-run",object.hash,"SIMULATED",9,"step","solver").code==d::Code::injected_fault);
    CHECK(!db.is_open());db.close();CHECK(db.open(dir)==d::Code::ok);
    const auto count=fault==d::FaultSite::after_head_commit?9u:8u;CHECK(db.event_count()==count&&db.audit_disk());
    CHECK(db.publish("MODEL_EVALUATION","long-run",object.hash,"SIMULATED",9,"step","solver").code==d::Code::ok);
    CHECK(db.event_count()==9&&db.audit_disk());
  }
  for(const auto fault:{d::FaultSite::after_archive_prepare,d::FaultSite::after_archive_commit}){
    const auto dir=root/std::to_string(case_number++);d::Registry db;CHECK(db.open(dir,d::StorageOptions::sustained(8))==d::Code::ok);
    for(std::uint64_t i=1;i<=17;++i)append(db,i);
    CHECK(db.save_replay_checkpoint("at-17"));db.inject_next_publish(fault);CHECK(db.archive_ready()==d::Code::injected_fault);
    db.close();CHECK(db.open(dir)==d::Code::ok&&db.event_count()==17&&db.audit_disk());
    CHECK(db.archive_ready()==d::Code::ok&&db.capacity().archived_segments==2);
    CHECK(files(dir/"segments")==1&&files(dir/"archive")==4);
    std::size_t n=0;CHECK(db.visit_events(0,[&](const d::Event&){++n;return true;}));CHECK(n==17);
  }
  {const auto dir=root/"checkpoint";d::Registry db;CHECK(db.open(dir,d::StorageOptions::sustained(8))==d::Code::ok);append(db,1);
    db.inject_next_publish(d::FaultSite::after_checkpoint_commit);CHECK(!db.save_replay_checkpoint("committed-checkpoint"));
    db.close();CHECK(db.open(dir)==d::Code::ok);d::ReplayCheckpoint checkpoint;std::string bytes;bool found=false;
    CHECK(db.load_replay_checkpoint(checkpoint,bytes,found)&&found&&checkpoint.sequence==1&&bytes=="committed-checkpoint");}
  {const auto dir=root/"object-archive";d::Registry db;CHECK(db.open(dir,d::StorageOptions::sustained(8))==d::Code::ok);
    for(std::uint64_t i=1;i<=17;++i)append(db,i);
    CHECK(db.save_replay_checkpoint("at-17")&&db.archive_ready()==d::Code::ok);d::ArchiveStats stats;
    db.inject_next_publish(d::FaultSite::after_object_archive);CHECK(db.collect_objects(stats)==d::Code::injected_fault);
    db.close();CHECK(db.open(dir)==d::Code::ok&&db.audit_disk());CHECK(db.collect_objects(stats)==d::Code::ok&&db.audit_disk());}
}
static rt::Observation observation(std::uint32_t source,std::uint64_t epoch,std::uint64_t sequence){
  rt::Observation o;o.colony=1;o.animon=2;o.source=source;o.source_epoch=epoch;o.sequence=sequence;o.clock_domain=3;
  o.metric=sequence%2?rt::Metric::membrane_voltage:rt::Metric::energy;o.lane=rt::Lane::health;o.origin=rt::Origin::simulated;
  o.value=static_cast<std::int32_t>(sequence)-65000;o.acquired_ns=100000*epoch+sequence;o.evidence_tag=source*1000+sequence;return o;
}
static std::string canonical_state(const rt::ColonyState& state){rt::ObservationReplayCheckpoint checkpoint;checkpoint.state=state;
  checkpoint.count=state.sources;std::string wire;CHECK(rt::ObservationCheckpointCodec::encode(checkpoint,wire));return wire;}
struct CountReplay:rt::IObservationEpochReplay {std::size_t count=0;bool accept(std::span<const std::byte>)noexcept override{++count;return true;}};
struct CountingJournal:rt::IObservationEpochJournal {
  rt::WindStoreEpochObservationJournal& backing;std::size_t accepted=0;bool restored=false;
  explicit CountingJournal(rt::WindStoreEpochObservationJournal& b):backing(b){}
  bool commit(const rt::Observation& o,std::span<const std::byte> frame)noexcept override{return backing.commit(o,frame);}
  bool replay(rt::IObservationEpochReplay& consumer)noexcept override{
    struct Spy:rt::IObservationEpochReplay {rt::IObservationEpochReplay& consumer;std::size_t& accepted;bool& restored;
      Spy(rt::IObservationEpochReplay& c,std::size_t& n,bool& r):consumer(c),accepted(n),restored(r){}
      bool supports_checkpoint()const noexcept override{return consumer.supports_checkpoint();}
      bool restore(const rt::ObservationReplayCheckpoint& c)noexcept override{restored=true;return consumer.restore(c);}
      bool accept(std::span<const std::byte> frame)noexcept override{++accepted;return consumer.accept(frame);}
    }spy(consumer,accepted,restored);return backing.replay(spy);
  }
};
static void projection_recovery(const fs::path& root){
  rt::EpochObservationCodec codec;std::string expected;
  // Narrow enum casts must not turn an unsupported wire metric into voltage.
  std::array<std::byte,rt::kEpochObservationFrame> canonical{},forged{};
  CHECK(codec.encode(observation(71,1,1),canonical));christeye::wind::View packet;
  CHECK(christeye::wind::decode(canonical,packet)==christeye::wind::Error::ok);
  std::array<std::byte,rt::kEpochObservationPayload> payload{};std::copy(packet.payload.begin(),packet.payload.end(),payload.begin());
  rt::wire_detail::u32(payload,16,267);std::size_t length=0;
  CHECK(christeye::wind::encode(packet.header,{},payload,forged,length)==christeye::wind::Error::ok);
  rt::Observation rejected;CHECK(!codec.decode(forged,rejected));
  {d::Registry db;CHECK(db.open(root,d::StorageOptions::sustained(8))==d::Code::ok);
    rt::WeightedQueue<> queue;rt::RuntimeService runtime(queue);rt::EpochRuntimeProjectionPort view(runtime);
    CHECK(view.enroll({71,3,rt::Origin::simulated,true,false,false,0}));CHECK(view.enroll({72,3,rt::Origin::simulated,true,false,false,0}));
    rt::ObservationCheckpointService mirror;rt::WindStoreEpochObservationJournal journal(db,"session",&mirror);
    rt::EpochObservationDeliveryService delivery(codec,journal,view);CHECK(delivery.recover());
    for(std::uint64_t i=1;i<=17;++i){CHECK(delivery.submit(observation(71,1,i)));CHECK(delivery.submit(observation(72,1,i)));}
    CHECK(delivery.submit(observation(71,2,1)));CHECK(view.latest().revision==35);
    CHECK(db.events().size()==3&&db.capacity().archived_segments==3);
    expected=canonical_state(view.latest());CHECK(canonical_state(mirror.snapshot().state)==expected);
    CHECK(!delivery.submit(observation(71,2,3)));CHECK(db.event_count()==35);CHECK(db.audit_disk());
  }
  {d::Registry db;CHECK(db.open(root)==d::Code::ok);rt::WeightedQueue<> queue;rt::RuntimeService runtime(queue);rt::EpochRuntimeProjectionPort view(runtime);
    CHECK(view.enroll({71,3,rt::Origin::simulated,true,false,false,0}));CHECK(view.enroll({72,3,rt::Origin::simulated,true,false,false,0}));
    rt::ObservationCheckpointService mirror;rt::WindStoreEpochObservationJournal journal(db,"session",&mirror);CountingJournal spy(journal);
    rt::EpochObservationDeliveryService delivery(codec,spy,view);CHECK(delivery.recover());
    CHECK(spy.restored&&spy.accepted==3&&view.latest().revision==35&&canonical_state(view.latest())==expected);
    CHECK(canonical_state(mirror.snapshot().state)==expected);
    CHECK(delivery.submit(observation(71,2,1)));auto conflict=observation(71,2,1);++conflict.value;CHECK(!delivery.submit(conflict));
    CHECK(delivery.submit(observation(71,2,2))&&view.latest().revision==36&&db.event_count()==36);
  }
  {d::Registry db;CHECK(db.open(root)==d::Code::ok);rt::WindStoreEpochObservationJournal journal(db,"session");CountReplay all;
    CHECK(journal.replay(all)&&all.count==36); // older readers retain full archive replay
    d::ReplayCheckpoint cp;std::string bytes;bool found=false;CHECK(db.load_replay_checkpoint(cp,bytes,found)&&found);
    rt::ObservationReplayCheckpoint decoded;CHECK(rt::ObservationCheckpointCodec::decode(bytes,decoded,"session"));
    CHECK(!rt::ObservationCheckpointCodec::decode(bytes,decoded,"another-session"));
    ++decoded.state.cursors[0].last_sequence;rt::ObservationCheckpointService invalid;CHECK(!invalid.restore(decoded));
    raw(root/"objects"/cp.object_hash,"broken-checkpoint");db.close();CHECK(db.open(root)!=d::Code::ok);
  }
}
static void legacy_migration(const fs::path& root){
  d::Registry db;CHECK(db.open(root)==d::Code::ok);std::vector<d::Event> old;
  for(std::uint64_t i=1;i<=19;++i)old.push_back(append(db,i));
  const auto opaque=db.put("legacy-opaque-nested-checkpoint");CHECK(opaque.code==d::Code::ok);
  CHECK(db.acknowledge("reader",10,old[9].hash)==d::Code::ok);db.close();
  CHECK(db.open(root,d::StorageOptions::sustained(8))==d::Code::ok&&db.event_count()==19&&db.events().size()==3);
  CHECK(fs::exists(root/"events.log")&&fs::exists(root/"HEAD")&&db.audit_disk());
  std::size_t n=0;CHECK(db.visit_events(0,[&](const d::Event& event){return event.hash==old[n++].hash;}));CHECK(n==old.size());
  CHECK(db.read_cursor("reader").sequence==10);CHECK(db.save_replay_checkpoint("migration-19")&&db.archive_ready()==d::Code::ok);
  d::ArchiveStats stats;CHECK(db.collect_objects(stats)==d::Code::ok);std::string bytes;CHECK(db.get(opaque.hash,bytes));
  append(db,20);db.close();CHECK(db.open(root)==d::Code::ok&&db.event_count()==20&&db.audit_disk());
  // Valid legacy logs may contain repeated source identities. Import preserves
  // committed global events verbatim instead of applying publish deduplication.
  const auto repeated=root/"repeated";fs::create_directories(repeated/"objects");
  const auto hash=christeye::living::plans::sha256_hex("same");raw(repeated/"objects"/hash,"same");
  d::Event first{1,std::string(d::kZeroDigest),"MODEL_EVALUATION","repeat",hash,"SIMULATED",1,"step","solver",{}};
  first.hash=d::event_hash(first);auto second=first;second.sequence=2;second.previous_hash=first.hash;second.hash=d::event_hash(second);
  raw(repeated/"events.log","CEWREG/1\n"+d::serialize_event(first)+d::serialize_event(second));
  d::Registry import;CHECK(import.open(repeated,d::StorageOptions::sustained(8))==d::Code::ok&&import.event_count()==2);
  CHECK(import.events()[1].hash==second.hash&&import.audit_disk());
}
static void corruption(const fs::path& root){
  d::Registry db;CHECK(db.open(root,d::StorageOptions::sustained(8))==d::Code::ok);
  for(std::uint64_t i=1;i<=25;++i)append(db,i);
  CHECK(db.save_replay_checkpoint("at-25")&&db.archive_ready()==d::Code::ok);
  fs::remove(root/"archive"/"00000000000000000001.log");
  CHECK(!db.audit_disk());std::size_t seen=0;CHECK(!db.visit_events(0,[&](const d::Event&){++seen;return true;}));CHECK(seen==0);
  db.close(); // Indexed open intentionally checks tail/anchors; explicit audit verifies full retained archive.
  CHECK(db.open(root)==d::Code::ok&&!db.audit_disk());
}
static void nonmonotonic(const fs::path& root){
  d::Registry db;CHECK(db.open(root,d::StorageOptions::sustained(8))==d::Code::ok);
  const auto first=append(db,10,"a");append(db,2,"b");d::Event found;bool exists=false;
  CHECK(db.find_event("MODEL_EVALUATION","long-run","step","solver",10,found,exists)&&exists&&found.hash==first.hash);
  const auto other=append(db,10,"c");(void)other;
  CHECK(db.publish("MODEL_EVALUATION","long-run",first.object_hash,"SIMULATED",10,"step","solver").hash==first.hash&&db.event_count()==3);
  CHECK(db.audit_disk());
}
static void worker_admission(){
  struct Journal:rt::IObservationEpochJournal {std::size_t commits=0;
    bool commit(const rt::Observation&,std::span<const std::byte>)noexcept override{++commits;return true;}
    bool replay(rt::IObservationEpochReplay&)noexcept override{return true;}
    rt::ObservationJournalCapacity storage_capacity()const noexcept override{
      rt::ObservationJournalCapacity c;c.known=true;c.available_events=1-commits;c.available_bytes=1024;c.append_reservation_bytes=1024;return c;}
  }journal;
  rt::EpochObservationCodec codec;rt::AsyncEpochJournalWorker<> worker(journal,codec);std::uint64_t ticket=0;
  CHECK(worker.has_capacity(1,1024)&&!worker.has_capacity(2,1024));CHECK(worker.enqueue(observation(71,1,1),ticket));
  CHECK(!worker.enqueue(observation(71,1,2),ticket));CHECK(worker.wait_for_completion(std::chrono::seconds(2)));
  rt::AsyncCommitReceipt receipt;CHECK(worker.try_receive(receipt)&&receipt.durable);CHECK(!worker.has_capacity(1,1));worker.shutdown();
}
static anaiero::session::Record metadata_record(){
  namespace se=anaiero::session;namespace en=anaiero::engine;
  se::Record r;r.configuration_sha256=std::string(64,'a');r.coordinator.initialized=true;r.coordinator.engine_count=1;
  auto& e=r.coordinator.engines[0];e.id=1;e.source=71;e.source_epoch=1;e.model_digest=std::string(64,'b');e.kind=en::Kind::sim3;
  e.last.id=1;e.last.source_epoch=1;std::fill_n(e.last.state_sha256.begin(),64,'c');e.last_status=en::Status::ok;
  r.contracts[0]={"anaiero-sim3/2",2,2,1,202,0,{},true,true,false,false};
  CHECK(se::files::valid_record(r));return r;
}
static void metadata_maintenance(const fs::path& root){
  namespace sf=anaiero::session::files;
  d::Registry db;CHECK(db.open(root,d::StorageOptions::sustained(8))==d::Code::ok);
  sf::WindStoreRecordStore store(db,"session-test");auto record=metadata_record();bool found=false;
  CHECK(store.load(record,found)&&!found);
  for(unsigned i=0;i<41;++i){++record.target_cycles;CHECK(store.save(record));}
  CHECK(record.revision==41&&db.capacity().archived_segments==4&&files(root/"segments")<=3);
  CHECK(files(root/"indexes")==1);
  auto changed=record;changed.contracts[0].clock_domain=999;CHECK(!store.save(changed)&&record.revision==41);
  const auto pending=db.put("foreign-operation-checkpoint");CHECK(pending.code==d::Code::ok);
  const std::array<std::string,1> roots{pending.hash};CHECK(db.pin("foreign-operation",roots)==d::Code::ok);
  CHECK(store.checkpoint_now());d::ArchiveStats stats;CHECK(db.collect_objects(stats)==d::Code::ok&&stats.moved_objects>=40);
  std::string bytes;CHECK(db.get(pending.hash,bytes));CHECK(files(root/"segments")==1&&db.audit_disk());
  db.close();CHECK(db.open(root)==d::Code::ok);
  sf::WindStoreRecordStore restored(db,"session-test");auto loaded=metadata_record();CHECK(restored.load(loaded,found)&&found&&loaded.revision==41);
  for(unsigned i=0;i<6;++i)CHECK(restored.save(loaded));
  CHECK(loaded.revision==47);
  // The next global event closes a maintenance interval after the explicit cut
  // at 41. Failure follows durable manifest and checkpoint publication.
  CHECK(restored.save(loaded)&&loaded.revision==48);
  db.inject_next_publish(d::FaultSite::after_checkpoint_commit);CHECK(!restored.save(loaded)&&loaded.revision==49);
  db.close();CHECK(db.open(root)==d::Code::ok&&db.event_count()==49);
  sf::WindStoreRecordStore resumed(db,"session-test");CHECK(resumed.load(loaded,found)&&found&&loaded.revision==49);
  CHECK(resumed.checkpoint_now()&&db.audit_disk());
}
int main(int argc,char** argv){
  CHECK(argc==2);const fs::path root(argv[1]);fs::remove_all(root);fs::create_directories(root);
  long_run(root/"long-run");retained_references(root/"retention");crash_cuts(root/"crashes");
  projection_recovery(root/"observations");legacy_migration(root/"legacy");corruption(root/"corrupt");nonmonotonic(root/"nonmonotonic");worker_admission();metadata_maintenance(root/"metadata");
  std::cout<<"Segmented storage: rotations, bounded recovery, checkpoint tail, references, reader lag, crash cuts PASS\n";
}
