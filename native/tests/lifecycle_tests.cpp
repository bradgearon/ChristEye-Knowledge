#include "anaiero/sdk/lifecycle.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>

namespace sdk=anaiero::sdk;
namespace ev=sdk::evidence;
namespace lc=sdk::lifecycle;
namespace fs=std::filesystem;
#define CHECK(value) do{if(!(value)){std::cerr<<"FAILED " #value " line "<<__LINE__<<'\n';std::abort();}}while(false)
static void require(const sdk::Result& result) {
  if(!result.ok()){std::cerr<<sdk::code_name(result.code)<<": "<<result.message<<'\n';std::abort();}
}
static std::string utf8(const fs::path& path){const auto bytes=path.generic_u8string();return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};}
static sdk::EngineIdentity identity(std::string source="source",std::uint64_t epoch=1){
  return {"engine-"+source,"test.engine/1",sdk::sha256("module"),sdk::sha256("model"),
    "test.behavior/1",std::move(source),"logical",epoch,0};
}
static sdk::EngineState initial_state(){return {0,0,sdk::Phase::ready,{"test.checkpoint/1","initial"},{"test.configuration/1","configuration"}};}
struct Fixture {
  struct Clock:ev::IAdmissionClock {std::uint64_t now=100;std::uint64_t unix_seconds()const noexcept override{return now;}} clock;
  fs::path path;
  std::unique_ptr<ev::ICommitStore> store;
  std::unique_ptr<lc::Service> service;
  lc::Options options;
  lc::IArchiveStore* archive=nullptr;
  explicit Fixture(fs::path value,lc::Options limits={},lc::IArchiveStore* archives=nullptr):path(std::move(value)),options(limits),archive(archives){reopen();}
  void reopen(){
    service.reset();store.reset();ev::StoreOptions storage;storage.admission_clock=&clock;
    require(ev::open_native_store(path.string(),storage,store));
    if(!store->info().index_ready)require(store->repair_index());
    service=std::make_unique<lc::Service>(*store,options,archive);require(service->open());
  }
  sdk::PreparedTransition prepare(const sdk::EngineIdentity& id,const sdk::EngineState& before,std::string operation,bool attach=false){
    sdk::PreparedTransition transition{id,operation,sdk::sha256(operation),sdk::state_digest(before),before.revision,before,{},{}};
    if(!attach){
      ++transition.target.revision;++transition.target.tick;transition.target.phase=sdk::Phase::running;
      transition.target.checkpoint.payload="state-"+std::to_string(transition.target.revision);
      transition.observations.push_back({"test.observation/1","value","test.quantity","count",
        static_cast<std::int64_t>(transition.target.revision),transition.target.tick});
    }
    return transition;
  }
  ev::CommitBatch stage(const sdk::PreparedTransition& transition){
    ev::CommitBatch batch;batch.operation_id=transition.operation_id;batch.request_digest=transition.request_sha256;
    // Exercise the shared runtime's already-staged state/transition deduplication.
    batch.blobs.push_back({sdk::checkpoint_digest(transition.identity,transition.target),sdk::encode_checkpoint(transition.identity,transition.target),{}});
    const auto bytes=sdk::encode_transition(transition);batch.blobs.push_back({sdk::sha256(bytes),bytes,{}});
    require(service->stage_commit(batch,transition));return batch;
  }
  void commit(const ev::CommitBatch& batch){ev::CommitReceipt receipt;require(store->commit(batch,receipt));}
  sdk::EngineState start(const sdk::EngineIdentity& id,const std::string& prefix){
    require(service->register_engine(id,"owner",prefix+"-register"));
    require(service->activate({id.source_uid,id.epoch},prefix+"-activate"));
    const auto initial=initial_state();commit(stage(prepare(id,initial,prefix+"-attach",true)));return initial;
  }
  sdk::EngineState step(const sdk::EngineIdentity& id,const sdk::EngineState& before,const std::string& op){
    const auto transition=prepare(id,before,op);commit(stage(transition));return transition.target;
  }
  lc::Source source(const sdk::EngineIdentity& id){lc::Source source;require(service->source({id.source_uid,id.epoch},source));return source;}
  void retire(const sdk::EngineIdentity& id,const std::string& prefix){
    require(service->begin_drain({id.source_uid,id.epoch},prefix+"-drain"));const auto tip=source(id);
    require(service->seal(tip.id,{tip.last_sequence,tip.last_payload_sha256},prefix+"-seal"));
    require(service->retire(tip.id,prefix+"-retire"));
  }
};

static void unified_source_authority(const fs::path& root){
  Fixture fixture(root/"unified-authority");
  const auto batch_for=[](std::string operation){ev::CommitBatch batch;batch.operation_id=operation;batch.request_digest=sdk::sha256(operation);return batch;};
  auto relay=batch_for("relay-reserve");
  require(fixture.service->stage_source_authority(relay,{"relay-uid","peer","logical",lc::SourceAuthorityKind::relay}));
  fixture.commit(relay);
  lc::SourceAuthority retained;require(fixture.service->source_authority("relay-uid",retained));
  CHECK(retained.owner=="peer"&&retained.kind==lc::SourceAuthorityKind::relay);
  CHECK(fixture.service->register_engine(identity("relay-uid"),"owner","cannot-local-register-relay").code==sdk::Code::unauthorized);
  auto changed=batch_for("change-relay-owner");
  CHECK(fixture.service->stage_source_authority(changed,{"relay-uid","different","logical",lc::SourceAuthorityKind::relay}).code==sdk::Code::unauthorized);
  changed=batch_for("change-relay-clock");
  CHECK(fixture.service->stage_source_authority(changed,{"relay-uid","peer","different",lc::SourceAuthorityKind::relay}).code==sdk::Code::unauthorized);

  require(fixture.service->register_engine(identity("local-uid"),"owner","local-register"));
  auto local_collision=batch_for("relay-after-local");
  CHECK(fixture.service->stage_source_authority(local_collision,{"local-uid","peer","logical",lc::SourceAuthorityKind::relay}).code==sdk::Code::unauthorized);
  // Both actors can prepare before either commits, but only one can win the
  // shared absent-key guard. The losing actor cannot create a parallel owner.
  auto staged_relay=batch_for("race-relay");
  require(fixture.service->stage_source_authority(staged_relay,{"race-uid","peer","logical",lc::SourceAuthorityKind::relay}));
  require(fixture.service->register_engine(identity("race-uid"),"owner","race-local"));
  ev::CommitReceipt receipt;CHECK(fixture.store->commit(staged_relay,receipt).code==sdk::Code::conflict);

  // Retained pre-ownership witnesses also prevent re-enrollment. Their bytes
  // need not be interpreted to know the UID cannot change its authority role.
  auto legacy=batch_for("legacy-witnesses");
  legacy.records.push_back({"mesh.receive.owner",sdk::sha256("legacy-relay"),"historical relay witness",{},false});
  legacy.records.push_back({"lifecycle.fence",sdk::sha256("legacy-local"),"historical local witness",{},false});
  fixture.commit(legacy);
  CHECK(fixture.service->register_engine(identity("legacy-relay"),"owner","legacy-local-collision").code==sdk::Code::unauthorized);
  auto legacy_collision=batch_for("legacy-relay-collision");
  CHECK(fixture.service->stage_source_authority(legacy_collision,{"legacy-local","peer","logical",lc::SourceAuthorityKind::relay}).code==sdk::Code::unauthorized);
  fixture.reopen();require(fixture.service->source_authority("relay-uid",retained));
  CHECK(retained.owner=="peer"&&retained.clock_domain=="logical");
}

static void source_churn(const fs::path& root){
  constexpr std::uint64_t epochs=12; // Six times the declared live-source cap.
  lc::Options limits;limits.max_active_sources=2;limits.max_metrics=2;
  Fixture fixture(root/"churn",limits);
  for(std::uint64_t epoch=1;epoch<=epochs;++epoch){
    const auto id=identity("churn",epoch);const auto prefix="epoch-"+std::to_string(epoch);
    const auto initial=fixture.start(id,prefix);(void)fixture.step(id,initial,prefix+"-step");fixture.retire(id,prefix);
    lc::Snapshot snapshot;require(fixture.service->snapshot(snapshot));
    CHECK(snapshot.schema==2&&snapshot.sources.empty()&&snapshot.metrics.empty());
    CHECK(snapshot.lifetime_observations==epoch);
    CHECK(fixture.source(id).phase==lc::SourcePhase::retired);
  }
  fixture.reopen();lc::Snapshot restored;require(fixture.service->snapshot(restored));
  CHECK(restored.sources.empty()&&restored.lifetime_observations==epochs);
  ev::ObservationClaim first_claim,second_claim;
  require(fixture.store->lookup_observation("churn",1,1,first_claim));
  require(fixture.store->lookup_observation("churn",2,1,second_claim));
  CHECK(first_claim.payload_digest!=second_claim.payload_digest);
  sdk::Bytes observation_bytes;require(fixture.store->get_blob(first_claim.payload_digest,observation_bytes));
  lc::SourceRef observed_source;std::uint64_t observed_sequence=0;sdk::Observation observed;
  require(lc::decode_source_observation(observation_bytes,observed_source,observed_sequence,observed));
  CHECK(observed_source.uid=="churn"&&observed_source.epoch==1&&observed_sequence==1);
  CHECK(fixture.service->register_engine(identity("churn",1),"owner","reopen-closed-epoch").code==sdk::Code::conflict);
  CHECK(fixture.service->register_engine(identity("churn",epochs+1),"another-owner","replace-owner").code==sdk::Code::unauthorized);
  const auto id=identity("churn",epochs+1);auto initial=fixture.start(id,"after-restart");
  auto staged=fixture.stage(fixture.prepare(id,initial,"late-step"));
  require(fixture.service->begin_drain({id.source_uid,id.epoch},"drain-barrier"));
  ev::CommitReceipt receipt;CHECK(fixture.store->commit(staged,receipt).code==sdk::Code::conflict);
  CHECK(fixture.source(id).last_sequence==0);
  CHECK(fixture.service->stage_commit(staged,fixture.prepare(id,initial,"late-step")).code==sdk::Code::expired);
  require(fixture.service->seal({id.source_uid,id.epoch},{},"empty-seal"));
  require(fixture.service->retire({id.source_uid,id.epoch},"empty-retire"));
  fixture.reopen();require(fixture.service->snapshot(restored));CHECK(restored.sources.empty()&&restored.lifetime_observations==epochs);
  ev::reset_native_index_statistics(*fixture.store);
  CHECK(fixture.source(identity("churn",1)).phase==lc::SourcePhase::retired);
  const auto stats=ev::native_index_statistics(*fixture.store);
  CHECK(stats.nodes_read<100); // Historical point lookup must not scan all generations.
}

static void subscription_generations(const fs::path& root){
  Fixture fixture(root/"subscriptions");const auto id=identity();auto state=fixture.start(id,"source");
  state=fixture.step(id,state,"step-1");const auto first=fixture.source(id);const lc::Cursor one{1,first.last_payload_sha256};
  state=fixture.step(id,state,"step-2");const auto second=fixture.source(id);const lc::Cursor two{2,second.last_payload_sha256};
  lc::Peer peer{"peer",sdk::sha256("cert-1"),1,1000,lc::PeerPhase::active};
  require(fixture.service->upsert_peer(peer,0,100,"enroll-peer"));
  lc::Subscription sub{"sub","peer",{id.source_uid,id.epoch},1,900,lc::SubscriptionPhase::active,{},{},false};
  require(fixture.service->open_subscription(sub,100,"subscribe"));
  CHECK(fixture.service->acknowledge("sub",1,two,100,"unoffered-ack").code==sdk::Code::conflict);
  CHECK(fixture.service->offer("sub",1,{1,sdk::sha256("wrong")},100,"wrong-offer").code==sdk::Code::conflict);
  require(fixture.service->offer("sub",1,one,100,"offer-one"));
  CHECK(fixture.service->offer("sub",1,two,100,"replace-offer").code==sdk::Code::busy);
  require(fixture.service->acknowledge("sub",1,one,100,"ack-one"));
  sub.generation=2;sub.acknowledged=one;require(fixture.service->open_subscription(sub,101,"renew-sub"));
  CHECK(fixture.service->acknowledge("sub",1,one,101,"stale-ack").code==sdk::Code::conflict);
  require(fixture.service->offer("sub",2,two,101,"offer-two"));
  fixture.reopen();require(fixture.service->acknowledge("sub",2,two,101,"ack-two"));
  require(fixture.service->acknowledge("sub",2,two,103,"ack-two"));
  CHECK(fixture.service->acknowledge("sub",2,one,101,"ack-two").code==sdk::Code::conflict);
  CHECK(fixture.service->acknowledge("sub",2,two,100,"rolled-back-clock").code==sdk::Code::expired);
  peer.generation=2;peer.certificate_sha256=sdk::sha256("cert-2");require(fixture.service->upsert_peer(peer,1,102,"rotate-peer"));
  CHECK(fixture.service->acknowledge("sub",2,two,102,"pre-rotation-ack").code==sdk::Code::expired);
  lc::Subscription restored;require(fixture.service->subscription("sub",restored));CHECK(restored.phase==lc::SubscriptionPhase::suspended);
  sub.generation=3;sub.acknowledged=two;sub.lease_until_unix_s=105;require(fixture.service->open_subscription(sub,102,"renew-after-rotation"));
  CHECK(fixture.service->offer("sub",3,two,105,"expired-offer").code==sdk::Code::expired);
  require(fixture.service->expire_subscriptions(105,"expire-leases"));
  lc::Snapshot snapshot;require(fixture.service->snapshot(snapshot));CHECK(snapshot.active_subscriptions==0);
  sub.generation=2;sub.lease_until_unix_s=900;sub.acknowledged=two;
  require(fixture.service->open_subscription(sub,106,"renew-sub"));
  require(fixture.service->snapshot(snapshot));CHECK(snapshot.active_subscriptions==0);
  require(fixture.service->expire_peer_leases(1000,"expire-peer-lease"));
  require(fixture.service->snapshot(snapshot));CHECK(snapshot.active_peers==0);
  peer.generation=1;peer.certificate_sha256=sdk::sha256("cert-1");
  require(fixture.service->upsert_peer(peer,0,2000,"enroll-peer")); // Retrieve original outcome after expiry.
  require(fixture.service->snapshot(snapshot));CHECK(snapshot.active_peers==0);
}

static void retention_and_bootstrap(const fs::path& root){
  std::unique_ptr<lc::IArchiveStore> archive;require(lc::open_native_archive((root/"archive").string(),{},archive));
  Fixture fixture(root/"retention",{},archive.get());const auto id=identity();auto state=fixture.start(id,"source");
  state=fixture.step(id,state,"step-1");const auto first=fixture.source(id);const auto payload=first.last_payload_sha256;
  state=fixture.step(id,state,"step-2");fixture.retire(id,"source");
  lc::MaintenanceRequest request{"review-one",{id.source_uid,id.epoch},1,100,lc::HistoryAvailability::expired,{}, {payload}};
  lc::MaintenancePlan plan;require(fixture.service->review_maintenance(request,"review",plan));
  CHECK(plan.expected_bytes>0&&plan.new_floor==2);
  lc::Pin pin{"reader","reader-owner",lc::PinKind::reader,1,101,{payload}};
  require(fixture.service->put_pin(pin,0,100,"pin-after-review"));
  CHECK(fixture.service->activate_maintenance(request.id,102,"race-activation").code==sdk::Code::conflict);
  request.id="reader-still-open";request.now_unix_s=102;
  CHECK(fixture.service->review_maintenance(request,"review-live-reader",plan).code==sdk::Code::busy);
  require(fixture.service->release_pin("reader",1,"release-reader"));
  request.id="review-two";require(fixture.service->review_maintenance(request,"review-two",plan));
  ev::set_native_store_fault(*fixture.store,ev::FaultPoint::after_root);
  CHECK(fixture.service->activate_maintenance(request.id,102,"activate-two").code==sdk::Code::committed_degraded);
  fixture.reopen();require(fixture.service->activate_maintenance(request.id,103,"activate-two"));
  CHECK(fixture.service->replay_status(request.source,0).code==sdk::Code::history_expired);
  require(fixture.service->replay_status(request.source,1));
  sdk::Bytes bytes;CHECK(fixture.store->get_blob(payload,bytes).code==sdk::Code::history_expired);
  ev::BlobInfo before;require(fixture.store->blob_info(payload,before));CHECK(before.present);
  pin.generation=2;pin.expires_unix_s=200;
  CHECK(fixture.service->put_pin(pin,1,102,"pin-expired-object").code==sdk::Code::history_expired);
  const auto reclaim_generation=fixture.store->generation();
  ev::set_native_store_fault(*fixture.store,ev::FaultPoint::after_transaction);
  CHECK(!fixture.service->execute_maintenance(request.id,"prune-two").committed());fixture.reopen();
  CHECK(fixture.store->generation()==reclaim_generation&&!fs::exists(fixture.path/"blobs"/payload));
  require(fixture.service->execute_maintenance(request.id,"prune-two"));
  CHECK(fixture.store->generation()==reclaim_generation+2); // Reclaimed-byte accounting plus completed plan.
  ev::BlobInfo after;require(fixture.store->blob_info(payload,after));CHECK(!after.present);
  CHECK(!fs::exists(fixture.path/"blobs"/payload));
  ev::ObservationClaim claim;require(fixture.store->lookup_observation(id.source_uid,id.epoch,1,claim));CHECK(claim.payload_digest==payload);
  lc::Peer peer{"peer",sdk::sha256("certificate"),1,1000,lc::PeerPhase::active};require(fixture.service->upsert_peer(peer,0,102,"peer"));
  lc::Subscription sub{"bootstrap-sub","peer",request.source,1,900,lc::SubscriptionPhase::active,{},{},false};
  CHECK(fixture.service->open_subscription(sub,102,"needs-bootstrap").code==sdk::Code::history_expired);
  const auto final=fixture.source(id);const auto snapshot=sdk::encode_state(state);
  lc::Bootstrap bootstrap{"bootstrap-sub","peer",1,900,request.source,{final.last_sequence,final.last_payload_sha256},sdk::sha256(snapshot),snapshot};
  auto wrong=bootstrap;wrong.tip.payload_sha256=sdk::sha256("other-tip");
  CHECK(fixture.service->accept_bootstrap(wrong,102,"wrong-bootstrap").code==sdk::Code::conflict);
  require(fixture.service->accept_bootstrap(bootstrap,102,"accept-bootstrap"));
  fixture.reopen();require(fixture.service->subscription("bootstrap-sub",sub));
  CHECK(sub.acknowledged==bootstrap.tip&&sub.generation==1);
  lc::Bootstrap installed_bootstrap;require(fixture.service->installed_bootstrap("bootstrap-sub",installed_bootstrap));
  CHECK(installed_bootstrap.snapshot==snapshot&&installed_bootstrap.tip==bootstrap.tip);
  ev::StoredRecord installed;require(fixture.store->latest("lifecycle.bootstrap",sdk::sha256("bootstrap-sub"),installed));
  CHECK(installed.record.references.size()==1&&installed.record.references.front()==final.state_object_sha256);
  require(fixture.store->get_blob(final.state_object_sha256,bytes));CHECK(bytes==sdk::encode_checkpoint(id,state));
  request.id="archive-final";request.expire_through=2;request.disposition=lc::HistoryAvailability::archived;
  request.archive_directory=std::string(archive->identifier());request.objects={final.last_payload_sha256};
  require(fixture.service->review_maintenance(request,"archive-review",plan));
  require(fixture.service->activate_maintenance(request.id,102,"archive-activate"));
  require(fixture.service->execute_maintenance(request.id,"archive-prune"));
  CHECK(fixture.service->replay_status(request.source,1).code==sdk::Code::archive_required);
  CHECK(fixture.service->replay_status(request.source,0).code==sdk::Code::history_expired);
  require(archive->get(final.last_payload_sha256,bytes));CHECK(sdk::sha256(bytes)==final.last_payload_sha256);
  lc::Service unavailable(*fixture.store);require(unavailable.open());
  CHECK(unavailable.replay_status(request.source,1).code==sdk::Code::archive_unavailable);
  require(fixture.service->close_subscription("bootstrap-sub",1,"close-bootstrap"));
  require(fixture.store->latest("lifecycle.bootstrap",sdk::sha256("bootstrap-sub"),installed));CHECK(installed.record.tombstone);
}

static void restored_authority(const fs::path& root){
  Fixture fixture(root/"restore");const auto id=identity();auto state=fixture.start(id,"source");state=fixture.step(id,state,"step");
  lc::Peer peer{"peer",sdk::sha256("old-certificate"),1,1000,lc::PeerPhase::active};require(fixture.service->upsert_peer(peer,0,100,"peer"));
  lc::Subscription sub{"sub","peer",{id.source_uid,id.epoch},1,900,lc::SubscriptionPhase::active,{},{},false};
  require(fixture.service->open_subscription(sub,100,"sub"));
  require(fixture.service->quarantine_restore("restore-from-backup"));fixture.reopen();
  lc::Snapshot snapshot;require(fixture.service->snapshot(snapshot));CHECK(snapshot.quarantined&&!snapshot.incarnation.empty());
  auto transition=fixture.prepare(id,state,"forbidden-step");ev::CommitBatch batch;batch.operation_id=transition.operation_id;batch.request_digest=transition.request_sha256;
  CHECK(fixture.service->stage_commit(batch,transition).code==sdk::Code::unauthorized);
  CHECK(fixture.service->offer("sub",1,{},100,"quarantined-offer").code==sdk::Code::unauthorized);
  require(fixture.service->reauthorize_restore(sdk::sha256("operator-current-policy"),"operator-reauthorize"));
  fixture.reopen();require(fixture.service->snapshot(snapshot));
  CHECK(!snapshot.quarantined&&snapshot.sources.empty()&&snapshot.metrics.empty()&&snapshot.lifetime_observations==1);
  CHECK(snapshot.active_peers==0&&snapshot.active_subscriptions==0);
  std::vector<sdk::EngineIdentity> cleanup;
  require(fixture.service->reauthorization_sources("operator-reauthorize",cleanup));CHECK(cleanup.size()==1&&cleanup.front()==id);
  lc::Peer restored;require(fixture.service->peer("peer",restored));CHECK(restored.phase==lc::PeerPhase::revoked);
  require(fixture.service->subscription("sub",sub));CHECK(sub.phase==lc::SubscriptionPhase::expired);
  peer.generation=2;CHECK(fixture.service->upsert_peer(peer,1,101,"reuse-old-certificate").code==sdk::Code::unauthorized);
  peer.certificate_sha256=sdk::sha256("new-certificate");require(fixture.service->upsert_peer(peer,1,101,"fresh-peer"));
  CHECK(fixture.service->register_engine(identity("source",2),"owner","reuse-unknown-epoch").code==sdk::Code::unauthorized);
  const auto fresh=identity(snapshot.incarnation+".source");(void)fixture.start(fresh,"fresh-incarnation");
  require(fixture.service->reauthorize_restore(sdk::sha256("operator-current-policy"),"operator-reauthorize"));
  require(fixture.service->reauthorization_sources("operator-reauthorize",cleanup));CHECK(cleanup.size()==1&&cleanup.front()==id);
  sub.generation=2;sub.phase=lc::SubscriptionPhase::active;
  CHECK(fixture.service->open_subscription(sub,101,"reuse-restored-sub").code==sdk::Code::rebootstrap_required);
}

static void archive_quota(const fs::path& root){
  const auto directory=root/fs::path(u8"small-archive-\u00e9-\u7bb1");
  std::unique_ptr<lc::IArchiveStore> archive;
  require(lc::open_native_archive(utf8(directory),{8,8},archive));
  CHECK(fs::is_directory(directory));
  const auto identifier=utf8(fs::weakly_canonical(directory));CHECK(archive->identifier()==identifier);
  require(archive->put(sdk::sha256("12345678"),"12345678"));
  require(archive->put(sdk::sha256("12345678"),"12345678"));
  CHECK(archive->put(sdk::sha256("x"),"x").code==sdk::Code::capacity);
  CHECK(archive->put(sdk::sha256("wrong"),"bytes").code==sdk::Code::invalid);
  std::unique_ptr<lc::IArchiveStore> competing;
  CHECK(lc::open_native_archive(utf8(directory),{8,8},competing).code==sdk::Code::busy);
  archive.reset();require(lc::open_native_archive(identifier,{8,8},archive));
  sdk::Bytes bytes;require(archive->get(sdk::sha256("12345678"),bytes));
  CHECK(bytes=="12345678"&&archive->identifier()==identifier);
}
static std::map<std::string,sdk::Digest> file_digests(const fs::path& root){
  std::map<std::string,sdk::Digest> result;
  for(const auto& entry:fs::recursive_directory_iterator(root))if(entry.is_regular_file()){
    std::ifstream stream(entry.path(),std::ios::binary);
    const sdk::Bytes bytes((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
    result[entry.path().lexically_relative(root).generic_string()]=sdk::sha256(bytes);
  }
  return result;
}
static void native_restore(const fs::path& root){
  Fixture source(root/"backup-source");const auto id=identity();auto state=source.start(id,"source");state=source.step(id,state,"step");
  lc::Peer peer{"peer",sdk::sha256("certificate"),1,1000,lc::PeerPhase::active};require(source.service->upsert_peer(peer,0,100,"peer"));
  const auto source_generation=source.store->generation();const auto source_root=source.store->info().root_digest;
  const auto destination=root/"restored-copy";lc::RestoreReport report;lc::RestoreOptions options;options.store.admission_clock=&source.clock;
  CHECK(lc::restore_quarantined_store(source.path.string(),destination.string(),"clone",report,options).code==sdk::Code::busy);
  CHECK(!fs::exists(destination));
  source.service.reset();source.store.reset();const auto before=file_digests(source.path);
  const auto nested_parent=source.path/"new-parent"/"deeper";
  CHECK(lc::restore_quarantined_store(utf8(source.path),utf8(nested_parent/"clone"),"nested",report,options).code==sdk::Code::invalid);
  CHECK(!fs::exists(source.path/"new-parent"));
  CHECK(lc::restore_quarantined_store(utf8(source.path),utf8(source.path),"same-source",report,options).code==sdk::Code::invalid);
  CHECK(lc::restore_quarantined_store(utf8(source.path),utf8(source.path.parent_path()),"ancestor",report,options).code==sdk::Code::invalid);
#ifdef _WIN32
  const auto case_source=source.path.parent_path()/L"BACKUP-SOURCE";
  if(fs::exists(case_source)&&fs::equivalent(case_source,source.path)){
    CHECK(lc::restore_quarantined_store(utf8(source.path),utf8(case_source/"new-case-parent"/"clone"),"case-nested",report,options).code==sdk::Code::invalid);
    CHECK(!fs::exists(source.path/"new-case-parent"));
    CHECK(lc::restore_quarantined_store(utf8(source.path),utf8(case_source),"case-source",report,options).code==sdk::Code::invalid);
  }
#endif
  CHECK(file_digests(source.path)==before);
  require(lc::restore_quarantined_store(source.path.string(),destination.string(),"clone",report,options));
  CHECK(report.published&&report.copied_files>0&&report.copied_bytes>0&&report.source_generation==source_generation);
  CHECK(report.source_root==source_root&&!report.incarnation.empty()&&fs::exists(destination));
  CHECK(file_digests(source.path)==before);
  lc::RestoreReport duplicate;require(lc::restore_quarantined_store(source.path.string(),destination.string(),"clone",duplicate,options));
  CHECK(duplicate.published&&duplicate.incarnation==report.incarnation);
  lc::RestoreReport independent;
  const auto independent_destination=root/"new-destination-parent"/"independent-copy";
  require(lc::restore_quarantined_store(utf8(source.path),utf8(independent_destination),"clone",independent,options));
  CHECK(independent.published&&independent.incarnation!=report.incarnation);
  CHECK(fs::is_directory(independent_destination));
  {
    Fixture restored(destination);lc::Snapshot snapshot;require(restored.service->snapshot(snapshot));
    CHECK(snapshot.quarantined&&snapshot.incarnation==report.incarnation);
    CHECK(restored.store->generation()==source_generation+1);
    CHECK(restored.service->upsert_peer({"another",sdk::sha256("fresh"),1,1000,lc::PeerPhase::active},0,100,"forbidden").code==sdk::Code::unauthorized);
    require(restored.service->reauthorize_restore(sdk::sha256("current-operator-policy"),"authorize-copy"));
    restored.reopen();require(restored.service->snapshot(snapshot));
    CHECK(!snapshot.quarantined&&snapshot.sources.empty()&&snapshot.active_peers==0&&snapshot.lifetime_observations==1);
  }
  fs::create_directory(root/"occupied");
  CHECK(lc::restore_quarantined_store(source.path.string(),(root/"occupied").string(),"no-overwrite",report,options).code==sdk::Code::conflict);
  auto small=options;small.store.maximum_bytes=65536;
  CHECK(lc::restore_quarantined_store(source.path.string(),(root/"too-small").string(),"quota",report,small).code==sdk::Code::capacity);
  CHECK(!fs::exists(root/"too-small"));
#ifndef _WIN32
  fs::create_symlink(root/"occupied",source.path/"outside");
  CHECK(lc::restore_quarantined_store(source.path.string(),(root/"symlink-copy").string(),"symlink",report,options).code==sdk::Code::unauthorized);
  CHECK(!fs::exists(root/"symlink-copy"));fs::remove(source.path/"outside");
#endif
  CHECK(file_digests(source.path)==before);
  // A corrupted input must never produce a visible destination or success.
  {std::ofstream corrupt(source.path/"blobs"/sdk::checkpoint_digest(id,state),std::ios::binary|std::ios::trunc);corrupt<<"damaged";}
  CHECK(lc::restore_quarantined_store(source.path.string(),(root/"corrupt-copy").string(),"corruption",report,options).code==sdk::Code::corrupt);
  CHECK(!fs::exists(root/"corrupt-copy"));
  {std::ofstream repaired(source.path/"blobs"/sdk::checkpoint_digest(id,state),std::ios::binary|std::ios::trunc);
    repaired<<sdk::encode_checkpoint(id,state);}
  require(lc::restore_quarantined_store(source.path.string(),(root/"corrupt-copy").string(),"corruption",report,options));
  CHECK(report.published&&file_digests(source.path)==before);
}
int main(int argc,char** argv){
  CHECK(argc==2||argc==3);const fs::path root(argv[1]);const std::string scenario=argc==3?argv[2]:"all";
  CHECK(scenario=="all"||scenario=="retries"||scenario=="restore");fs::remove_all(root);fs::create_directories(root);
  if(scenario=="all"){unified_source_authority(root);source_churn(root);}
  if(scenario=="all"||scenario=="retries"){subscription_generations(root);retention_and_bootstrap(root);}
  if(scenario=="all"){restored_authority(root);archive_quota(root);}
  if(scenario=="all"||scenario=="restore")native_restore(root);
  std::cout<<"Lifecycle source churn, retirement barriers, stale ACKs, retention races, bootstrap, quarantine, native restore and archive bounds passed\n";
}
