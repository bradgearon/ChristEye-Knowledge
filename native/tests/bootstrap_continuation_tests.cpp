#include "anaiero/sdk/mesh.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
namespace sdk=anaiero::sdk;
namespace ev=sdk::evidence;
namespace lc=sdk::lifecycle;
namespace mesh=sdk::mesh;
namespace fs=std::filesystem;
#define CHECK(expr) do{if(!(expr)){std::cerr<<"FAILED " #expr " at "<<__LINE__<<'\n';std::abort();}}while(false)
static void require(const sdk::Result& r){if(!r){std::cerr<<sdk::code_name(r.code)<<": "<<r.message<<'\n';std::abort();}}
using Phase=lc::BootstrapContinuationPhase;
struct Fixture {
  struct Clock:ev::IAdmissionClock{std::uint64_t now=100;std::uint64_t unix_seconds()const noexcept override{return now;}}clock;
  fs::path path;std::unique_ptr<ev::ICommitStore> store;std::unique_ptr<lc::Service> lifecycle;std::unique_ptr<mesh::Service> service;
  sdk::EngineIdentity id{"cell","test.engine/1",sdk::sha256("module"),sdk::sha256("model"),"test.behavior/1","continuous-source","logical",1,0};
  lc::SourceRef source{id.source_uid,id.epoch};sdk::EngineState state{0,0,sdk::Phase::running,{"test.state/1","zero"},{"test.config/1","config"}};
  mesh::Session session;std::string principal;
  Fixture(fs::path p,bool sender=true):path(std::move(p)),principal(sender?"receiver":"sender"){
    reopen();
    if(sender){require(lifecycle->register_engine(id,"owner","register"));require(lifecycle->activate(source,"activate"));
      commit({id,"attach",sdk::sha256("attach"),sdk::state_digest(state),0,state,{},{}});}
    require(lifecycle->upsert_peer({principal,sdk::sha256(principal),1,1000,lc::PeerPhase::active},0,100,"enroll"));
    require(service->authenticate({sdk::sha256(principal),1000,true},principal,100,session));
    require(service->configure_grant({principal,id,1,sender,!sender,true,{}},0,"grant"));
  }
  void reopen(){service.reset();lifecycle.reset();store.reset();ev::StoreOptions o;o.admission_clock=&clock;
    require(ev::open_native_store(path.string(),o,store));if(!store->info().index_ready)require(store->repair_index());
    lifecycle=std::make_unique<lc::Service>(*store);require(lifecycle->open());service=std::make_unique<mesh::Service>(*store);}
  void commit(const sdk::PreparedTransition& t){ev::CommitBatch b;b.operation_id=t.operation_id;b.request_digest=t.request_sha256;
    require(lifecycle->stage_commit(b,t));ev::CommitReceipt c;require(store->commit(b,c));}
  void advance(){const auto previous=state;++state.revision;++state.tick;const auto i=state.revision;state.checkpoint.payload=std::to_string(i);
    const auto op="tick-"+std::to_string(i);commit({id,op,sdk::sha256(op),sdk::state_digest(previous),previous.revision,state,
      {{"test.observation/1","metric","count","unit",static_cast<std::int64_t>(i),i}},{}});}
  void seal(){require(lifecycle->begin_drain(source,"drain"));lc::Source s;require(lifecycle->source(source,s));
    require(lifecycle->seal(source,{s.last_sequence,s.last_payload_sha256},"seal"));require(lifecycle->retire(source,"retire"));}
  mesh::BootstrapOffer offer(std::string name="sub",std::uint64_t lease=900){mesh::BootstrapOffer o;
    require(service->offer_bootstrap(session,std::move(name),1,source,lease,clock.now,o));return o;}
  void verify(const mesh::BootstrapOffer& o,Phase phase){lc::BootstrapContinuation c;require(lifecycle->bootstrap_continuation(o.id,c));
    CHECK(c.phase==phase&&c.tip==o.tip&&c.source==o.source&&c.checkpoint_sha256==o.checkpoint_sha256&&c.subscription_id==o.subscription_id);
    lc::Pin pin;const auto pinned=lifecycle->pin(o.id,pin);lc::BootstrapContinuation current;const auto pending=lifecycle->pending_bootstrap(o.subscription_id,current);
    if(phase==Phase::pending){require(pinned);CHECK(pin.kind==lc::PinKind::bootstrap_continuation&&pin.objects.size()==(o.tip.sequence?2U:1U));require(pending);CHECK(current.id==o.id);}
    else{CHECK(pinned.code==sdk::Code::not_found&&pending.code==sdk::Code::not_found);}
    ev::StoredRecord metadata;require(store->latest("mesh.bootstrap.offer",o.id,metadata));CHECK(metadata.record.references.empty());}
  void check_counts(std::size_t pending,std::size_t pins,std::size_t subscriptions){lc::Snapshot v;require(lifecycle->snapshot(v));
    CHECK(v.pending_bootstraps==pending&&v.pins==pins&&v.active_subscriptions==subscriptions);}
};
static void live_replay(const fs::path& root,bool empty){
  Fixture s(root/"sender"),r(root/"receiver",false);if(!empty)s.advance();const auto o=s.offer();CHECK(o.tip.sequence==(empty?0U:1U));s.verify(o,Phase::pending);
  s.advance();s.advance();s.reopen();const auto prior=s.store->generation();CHECK(mesh::encode_bootstrap(s.offer())==mesh::encode_bootstrap(o)&&s.store->generation()==prior);
  mesh::BootstrapOffer different;CHECK(s.service->offer_bootstrap(s.session,"sub",1,s.source,901,100,different).code==sdk::Code::busy);
  CHECK(s.lifecycle->release_pin(o.id,1,"untyped-release").code==sdk::Code::invalid);
  CHECK(s.lifecycle->put_pin({o.id,s.principal,lc::PinKind::reader,2,900,{o.checkpoint_sha256}},1,100,"replace-protection").code==sdk::Code::invalid);
  s.seal();ev::ObservationClaim suffix;require(s.store->lookup_observation(s.source.uid,s.source.epoch,o.tip.sequence+1,suffix));
  lc::MaintenanceRequest expiry{"suffix",s.source,suffix.sequence,100,lc::HistoryAvailability::expired,{},{suffix.payload_digest}};
  lc::MaintenancePlan plan;CHECK(s.lifecycle->review_maintenance(expiry,"blocked-pending",plan).code==sdk::Code::busy);
  mesh::ReceiveReceipt installed;require(r.service->install_bootstrap(r.session,o,100,installed));const auto installed_generation=installed.store_generation;
  r.reopen();require(r.service->install_bootstrap(r.session,o,100,installed));CHECK(installed.duplicate&&installed.store_generation==installed_generation);
  auto tampered=o;tampered.checkpoint+="x";CHECK(!s.service->accept_bootstrap(s.session,tampered,100));s.verify(o,Phase::pending);
  ev::set_native_store_fault(*s.store,ev::FaultPoint::after_transaction);CHECK(!s.service->accept_bootstrap(s.session,o,100).committed());
  s.reopen();s.verify(o,Phase::pending);s.check_counts(1,1,0);
  ev::set_native_store_fault(*s.store,ev::FaultPoint::after_root);CHECK(s.service->accept_bootstrap(s.session,o,100).committed());
  s.reopen();s.verify(o,Phase::accepted);s.check_counts(0,0,1);const auto accepted=s.store->generation();
  require(s.service->accept_bootstrap(s.session,o,100));CHECK(s.store->generation()==accepted);
  require(s.service->bootstrap_accepted(s.session,"sub",1,o.checkpoint_sha256,o.tip,100));
  CHECK(s.lifecycle->review_maintenance(expiry,"blocked-subscription",plan).code==sdk::Code::busy);
  mesh::Delivery page;mesh::TransferCost cost;require(s.service->next(s.session,"sub",1,100,page,cost));
  CHECK(page.from==o.tip&&page.to.sequence==o.tip.sequence+2&&page.observations.size()==2);
  require(r.service->receive(r.session,page,100,installed));require(s.service->acknowledge(s.session,page,100));
  const auto ack_generation=s.store->generation();require(s.service->accept_bootstrap(s.session,o,100));CHECK(s.store->generation()==ack_generation);
  lc::Subscription sub;require(s.lifecycle->subscription("sub",sub));CHECK(sub.generation==1&&sub.acknowledged==page.to);
  require(s.lifecycle->review_maintenance(expiry,"allowed-after-ack",plan));require(s.lifecycle->activate_maintenance("suffix",100,"activate-prefix"));
  require(s.lifecycle->execute_maintenance("suffix","reclaim-prefix"));sdk::Bytes bytes;CHECK(s.store->get_blob(suffix.payload_digest,bytes).code==sdk::Code::history_expired);
  mesh::DeliveryItem item;require(r.service->received_observation(r.principal,r.source,suffix.sequence,item));
  if(!empty)CHECK(r.service->received_observation(r.principal,r.source,o.tip.sequence,item).code==sdk::Code::not_found);
  ev::AuditReport audit;require(s.store->audit(audit));require(r.store->audit(audit));
}
static void revoke_cancel_expire(const fs::path& root){
  Fixture f(root);f.advance();const auto cancel=f.offer("cancel");
  ev::set_native_store_fault(*f.store,ev::FaultPoint::after_transaction);CHECK(!f.service->cancel_bootstrap(cancel.id,"cancel").committed());
  f.reopen();f.verify(cancel,Phase::pending);ev::set_native_store_fault(*f.store,ev::FaultPoint::after_root);CHECK(f.service->cancel_bootstrap(cancel.id,"cancel").committed());
  f.reopen();f.verify(cancel,Phase::cancelled);const auto before=f.store->generation();require(f.service->cancel_bootstrap(cancel.id,"cancel"));
  CHECK(f.store->generation()==before&&f.service->accept_bootstrap(f.session,cancel,100).code==sdk::Code::expired);
  const auto expire=f.offer("short",101);f.clock.now=101;CHECK(!f.service->authorize_bootstrap(f.session,expire,101));
  require(f.lifecycle->expire_subscriptions(101,"expire"));f.verify(expire,Phase::expired);f.check_counts(0,0,0);
  const auto withdrawn=f.offer("withdrawn");require(f.service->configure_grant({f.principal,f.id,2,true,false,false,{}},1,"withdraw-scope"));
  f.verify(withdrawn,Phase::expired);require(f.service->configure_grant({f.principal,f.id,3,true,false,true,{}},2,"regrant"));
  CHECK(f.service->accept_bootstrap(f.session,withdrawn,101).code==sdk::Code::expired);
  const auto rotated=f.offer("rotated");require(f.lifecycle->upsert_peer({f.principal,sdk::sha256("replacement"),2,1000,lc::PeerPhase::active},1,101,"rotate"));
  f.verify(rotated,Phase::expired);CHECK(f.service->accept_bootstrap(f.session,rotated,101).code==sdk::Code::unauthorized);
  require(f.service->authenticate({sdk::sha256("replacement"),1000,true},f.principal,101,f.session));
  CHECK(f.service->accept_bootstrap(f.session,rotated,101).code==sdk::Code::unauthorized);
  const auto revoked=f.offer("revoked");require(f.lifecycle->upsert_peer({f.principal,sdk::sha256("replacement"),3,1000,lc::PeerPhase::revoked},2,101,"revoke"));
  f.verify(revoked,Phase::expired);f.reopen();f.check_counts(0,0,0);
}
static void guards_and_capacity(const fs::path& root){
  Fixture f(root);f.advance();lc::Source s;require(f.lifecycle->source(f.source,s));
  lc::BootstrapContinuation c{"race","race-sub",f.principal,f.id,1,900,1,{s.last_sequence,s.last_payload_sha256},s.state_object_sha256,s.state_sha256,Phase::pending};
  ev::CommitBatch b;b.operation_id="stage-capture";b.request_digest=sdk::sha256("stage-capture");require(f.lifecycle->stage_bootstrap_continuation(b,c,100));
  f.advance();ev::CommitReceipt receipt;CHECK(f.store->commit(b,receipt).code==sdk::Code::conflict);CHECK(f.lifecycle->bootstrap_continuation("race",c).code==sdk::Code::not_found);
  mesh::BootstrapOffer interrupted;ev::set_native_store_fault(*f.store,ev::FaultPoint::after_transaction);
  CHECK(!f.service->offer_bootstrap(f.session,"prepared",1,f.source,900,100,interrupted).committed());f.reopen();f.check_counts(0,0,0);
  CHECK(f.lifecycle->bootstrap_continuation(interrupted.id,c).code==sdk::Code::not_found);
  ev::set_native_store_fault(*f.store,ev::FaultPoint::after_root);CHECK(f.service->offer_bootstrap(f.session,"prepared",1,f.source,900,100,interrupted).committed());
  f.reopen();f.verify(interrupted,Phase::pending);f.advance();CHECK(mesh::encode_bootstrap(f.offer("prepared"))==mesh::encode_bootstrap(interrupted));
  require(f.service->subscribe(f.session,{"prepared",f.principal,f.source,1,900,lc::SubscriptionPhase::active,{},{},false},100));
  const auto before=f.store->generation();CHECK(f.service->accept_bootstrap(f.session,interrupted,100).code==sdk::Code::conflict);CHECK(f.store->generation()==before);
  f.verify(interrupted,Phase::pending);require(f.service->cancel_bootstrap(interrupted.id,"cancel-stale"));
  std::vector<mesh::BootstrapOffer> captures;for(std::size_t i=0;i<15;++i)captures.push_back(f.offer("bounded-"+std::to_string(i)));
  CHECK(f.service->offer_bootstrap(f.session,"overflow",1,f.source,900,100,interrupted).code==sdk::Code::capacity);f.check_counts(15,15,1);
  require(f.service->cancel_bootstrap(captures.front().id,"make-room"));(void)f.offer("overflow");f.check_counts(15,15,1);
  require(f.lifecycle->quarantine_restore("quarantine",sdk::sha256("restored")));require(f.lifecycle->reauthorize_restore(sdk::sha256("policy"),"reauthorize"));
  f.reopen();f.check_counts(0,0,0);f.verify(captures.back(),Phase::expired);
}
static void legacy_contract(const fs::path& root){
  Fixture f(root);f.advance();lc::Source s;require(f.lifecycle->source(f.source,s));sdk::Bytes checkpoint;require(f.store->get_blob(s.state_object_sha256,checkpoint));
  auto operation=[](std::string_view action,std::string_view bytes){return "mesh2-"+std::string(action)+"-"+sdk::sha256(bytes);};
  mesh::BootstrapOffer o{operation("bootstrap",sdk::pack({"legacy","1",s.state_object_sha256,"900"})),"legacy",1,900,f.id,
    {s.last_sequence,s.last_payload_sha256},s.state_object_sha256,checkpoint};
  require(f.lifecycle->put_pin({o.id,f.principal,lc::PinKind::reader,1,900,{o.checkpoint_sha256}},0,100,"legacy-pin"));
  ev::CommitBatch b;b.operation_id=operation("bootstrap-offer",mesh::encode_bootstrap(o));b.request_digest=sdk::sha256(mesh::encode_bootstrap(o));
  b.records.push_back({"mesh.bootstrap.offer",o.id,sdk::pack({"mesh-bootstrap-offer/2",o.id,"legacy","1","900",sdk::encode_identity(f.id),
    sdk::pack({std::to_string(o.tip.sequence),o.tip.payload_sha256}),o.checkpoint_sha256}),{o.checkpoint_sha256},false});
  ev::CommitReceipt receipt;require(f.store->commit(b,receipt));f.reopen();require(f.service->authorize_bootstrap(f.session,o,100));f.check_counts(0,1,0);
  f.advance();CHECK(f.service->accept_bootstrap(f.session,o,100).code==sdk::Code::conflict);require(f.service->cancel_bootstrap(o.id,"release-legacy"));f.reopen();f.check_counts(0,0,0);
}
static void full_directory_expiry(const fs::path& root){
  // Each of the 32 active subscriptions has an installed checkpoint. Expiring
  // every peer and all 15 pending continuations must fit ONE default batch.
  Fixture f(root);std::vector<mesh::BootstrapOffer> pending;
  for(unsigned i=0;i<32;++i){auto session=f.session;std::string principal=f.principal;
    if(i){principal="peer-"+std::to_string(i);const auto cert=sdk::sha256(principal);
      require(f.lifecycle->upsert_peer({principal,cert,1,1000,lc::PeerPhase::active},0,100,"enroll-"+principal));
      require(f.service->authenticate({cert,1000,true},principal,100,session));
      require(f.service->configure_grant({principal,f.id,1,true,false,true,{}},0,"grant-"+principal));}
    mesh::BootstrapOffer o;require(f.service->offer_bootstrap(session,"installed-"+principal,1,f.source,900,100,o));
    require(f.service->accept_bootstrap(session,o,100));
  }
  for(unsigned i=0;i<15;++i)pending.push_back(f.offer("pending-"+std::to_string(i)));
  lc::Snapshot snapshot;require(f.lifecycle->snapshot(snapshot));CHECK(snapshot.active_peers==32);f.check_counts(15,15,32);
  f.clock.now=1000;const auto before=f.store->generation();require(f.lifecycle->expire_peer_leases(1000,"expire-full-directory"));
  CHECK(f.store->generation()==before+1);f.reopen();f.check_counts(0,0,0);require(f.lifecycle->snapshot(snapshot));CHECK(!snapshot.active_peers);
  for(const auto& o:pending)f.verify(o,Phase::expired);
}
int main(int argc,char** argv){CHECK(argc==2);const fs::path p(argv[1]);fs::remove_all(p);fs::create_directories(p);
  live_replay(p/"moving",false);live_replay(p/"empty",true);revoke_cancel_expire(p/"release");guards_and_capacity(p/"bounds");legacy_contract(p/"legacy");full_directory_expiry(p/"full-expiry");
  std::cout<<"live bootstrap capture, protected continuation, atomic handoff/release, bounded slots, authority and legacy contracts passed\n";
}
