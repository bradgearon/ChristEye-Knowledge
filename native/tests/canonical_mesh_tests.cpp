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
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " line "<<__LINE__<<'\n';std::abort();}}while(false)
static void require(const sdk::Result& result){if(!result){std::cerr<<sdk::code_name(result.code)<<": "<<result.message<<'\n';std::abort();}}
struct Fixture {
  struct Clock:ev::IAdmissionClock {std::uint64_t now=100;std::uint64_t unix_seconds()const noexcept override{return now;}} clock;
  fs::path path;std::unique_ptr<ev::ICommitStore> store;std::unique_ptr<lc::Service> lifecycle;std::unique_ptr<mesh::Service> service;
  explicit Fixture(fs::path root):path(std::move(root)){reopen();}
  void reopen(){service.reset();lifecycle.reset();store.reset();ev::StoreOptions options;options.admission_clock=&clock;require(ev::open_native_store(path.string(),options,store));if(!store->info().index_ready)require(store->repair_index());lifecycle=std::make_unique<lc::Service>(*store);require(lifecycle->open());mesh::Options bounds;bounds.maximum_page_observations=2;service=std::make_unique<mesh::Service>(*store,bounds);}
  mesh::Session enroll(std::string principal,std::string cert){
    require(lifecycle->upsert_peer({principal,sdk::sha256(cert),1,1000,lc::PeerPhase::active},0,100,"enroll-"+principal));
    mesh::Session session;require(service->authenticate({sdk::sha256(cert),1000,true},principal,100,session));return session;
  }
  void commit(const sdk::PreparedTransition& transition){ev::CommitBatch batch;batch.operation_id=transition.operation_id;batch.request_digest=transition.request_sha256;require(lifecycle->stage_commit(batch,transition));ev::CommitReceipt receipt;require(store->commit(batch,receipt));}
};
static sdk::EngineIdentity identity(){return {"cell","test.engine/1",sdk::sha256("module"),sdk::sha256("model"),"test.behavior/1","canonical-source","logical",1,0};}
static sdk::EngineState seed(Fixture& fixture,std::uint64_t count){
  const auto id=identity();require(fixture.lifecycle->register_engine(id,"owner","register"));require(fixture.lifecycle->activate({id.source_uid,id.epoch},"activate"));
  sdk::EngineState state{0,0,sdk::Phase::running,{"test.state/1","zero"},{"test.config/1","config"}};
  fixture.commit({id,"attach",sdk::sha256("attach"),sdk::state_digest(state),0,state,{},{}});
  for(std::uint64_t i=1;i<=count;++i){const auto prior=state;state.revision=i;state.tick=i;state.checkpoint.payload=std::to_string(i);const auto op="step-"+std::to_string(i);
    fixture.commit({id,op,sdk::sha256(op),sdk::state_digest(prior),prior.revision,state,{{"test.observation/1","metric","count","unit",static_cast<std::int64_t>(i),i}},{}});}
  return state;
}
static void durable_pages(const fs::path& root){
  Fixture sender(root/"sender"),receiver(root/"receiver");(void)seed(sender,5);
  const auto sending=sender.enroll("receiver","receiver-cert"),receiving=receiver.enroll("sender","sender-cert");
  const auto id=identity();const lc::SourceRef source{id.source_uid,id.epoch};
  require(sender.service->configure_grant({"receiver",id,1,true,false,true,{}},0,"outbound-grant"));
  require(receiver.service->configure_grant({"sender",id,1,false,true,true,{}},0,"inbound-grant"));
  lc::Subscription subscription{"subscription","receiver",source,1,900,lc::SubscriptionPhase::active,{},{},false};
  require(sender.service->subscribe(sending,subscription,100));
  mesh::Delivery page;mesh::TransferCost cost;require(sender.service->next(sending,"subscription",1,100,page,cost));CHECK(page.observations.size()==2&&page.to.sequence==2);
  const auto original=mesh::encode_delivery(page);mesh::ReceiveReceipt receipt;require(receiver.service->receive(receiving,page,100,receipt));CHECK(receipt.cursor.sequence==2&&!receipt.duplicate);
  receiver.reopen();require(receiver.service->receive(receiving,page,100,receipt));CHECK(receipt.duplicate);
  sender.reopen();mesh::Delivery retry;require(sender.service->next(sending,"subscription",1,100,retry,cost));CHECK(mesh::encode_delivery(retry)==original);
  auto wrong=page;wrong.to.payload_sha256=sdk::sha256("wrong");CHECK(sender.service->acknowledge(sending,wrong,100).code==sdk::Code::conflict);
  require(sender.service->acknowledge(sending,page,100));require(sender.service->next(sending,"subscription",1,100,page,cost));CHECK(page.from.sequence==2&&page.to.sequence==4);
  require(receiver.service->receive(receiving,page,100,receipt));require(sender.service->acknowledge(sending,page,100));
  require(sender.service->next(sending,"subscription",1,100,page,cost));CHECK(page.to.sequence==5);
  ev::set_native_store_fault(*receiver.store,ev::FaultPoint::after_root);
  CHECK(receiver.service->receive(receiving,page,100,receipt).committed());receiver.reopen();require(receiver.service->receive(receiving,page,100,receipt));CHECK(receipt.duplicate);
  require(sender.service->acknowledge(sending,page,100));CHECK(sender.service->next(sending,"subscription",1,100,page,cost).code==sdk::Code::completed);
  for(std::uint64_t sequence=1;sequence<=5;++sequence){mesh::DeliveryItem item;require(receiver.service->received_observation("sender",source,sequence,item));CHECK(item.claim.sequence==sequence);}
  require(sender.lifecycle->upsert_peer({"receiver",sdk::sha256("receiver-cert"),2,1000,lc::PeerPhase::revoked},1,100,"revoke"));
  CHECK(sender.service->authorize(sending,source,true,100).code==sdk::Code::unauthorized);
  ev::AuditReport audit;require(sender.store->audit(audit));require(receiver.store->audit(audit));
}
static void fairness(){
  mesh::SchedulerOptions options;options.maximum_connections=4;options.byte_quantum=100;options.work_quantum=4;options.principal_byte_budget=200;options.source_byte_budget=200;
  mesh::FairScheduler scheduler(options);require(scheduler.enqueue({1,"same","first"},{150,2}));require(scheduler.enqueue({2,"same","first"},{50,1}));require(scheduler.enqueue({3,"other","second"},{10,1}));
  CHECK(scheduler.enqueue({3,"other","second"},{10,1}).code==sdk::Code::busy);
  std::uint64_t connection=0;CHECK(scheduler.take(connection)&&connection==2);CHECK(scheduler.take(connection)&&connection==3);CHECK(scheduler.take(connection)&&connection==1);CHECK(!scheduler.take(connection));
  mesh::FairScheduler maximum({8,32768,64,16U*1024U*1024U+65536U,16U*1024U*1024U+65536U,8192,8192});
  require(maximum.enqueue({1,"large-peer","large-source"},{16U*1024U*1024U+65536U,16+2U*8U*1024U*1024U/4096}));
  require(maximum.enqueue({2,"small-peer","small-source"},{49152,80}));
  bool large=false,small=false;for(unsigned turns=0;turns<1024&&(!large||!small);++turns){
    if(maximum.take(connection)){if(connection==1){CHECK(small);large=true;}else if(connection==2)small=true;}
  }
  CHECK(large&&small&&maximum.queued()==0);
}
static void authorization_boundaries(const fs::path& root){
  Fixture sender(root/"authority-sender"),receiver(root/"authority-receiver");(void)seed(sender,3);const auto id=identity();const lc::SourceRef source{id.source_uid,id.epoch};
  const auto sending=sender.enroll("receiver","receiver-cert"),receiving=receiver.enroll("sender","sender-cert");
  const auto object=sdk::sha256("artifact");ev::CommitBatch artifact;artifact.operation_id="artifact";artifact.request_digest=object;artifact.blobs.push_back({object,"artifact",{}});artifact.records.push_back({"operator.fixture","artifact",object,{object},false});ev::CommitReceipt committed;require(sender.store->commit(artifact,committed));
  mesh::Grant outgoing{"receiver",id,1,true,false,true,{object}};require(sender.service->configure_grant(outgoing,0,"send-grant"));
  require(receiver.service->configure_grant({"sender",id,1,false,true,true,{object}},0,"receive-grant"));
  auto local=id;local.epoch=2;local.instance_id="local-conflict";
  CHECK(receiver.lifecycle->register_engine(local,"local-owner","reclaim-relay").code==sdk::Code::unauthorized);
  auto remote=id;remote.epoch=2;remote.instance_id="remote-conflict";
  CHECK(sender.service->configure_grant({"receiver",remote,1,false,true,true,{}},0,"claim-local-uid").code==sdk::Code::unauthorized);
  require(sender.service->subscribe(sending,{"queued","receiver",source,1,900,lc::SubscriptionPhase::active,{},{},false},100));
  mesh::Delivery page;mesh::TransferCost cost;require(sender.service->next(sending,"queued",1,100,page,cost));require(sender.service->authorize_delivery(sending,page,100));
  mesh::Session short_certificate;require(sender.service->authenticate({sdk::sha256("receiver-cert"),101,true},"receiver",100,short_certificate));
  sender.clock.now=101; // Expiry occurs after domain admission, at the storage boundary.
  CHECK(sender.service->subscribe(short_certificate,{"late-subscribe","receiver",source,1,900,lc::SubscriptionPhase::active,{},{},false},100).code==sdk::Code::expired);
  CHECK(sender.service->acknowledge(short_certificate,page,100).code==sdk::Code::expired);
  lc::Subscription unacknowledged;require(sender.lifecycle->subscription("queued",unacknowledged));CHECK(unacknowledged.acknowledged.sequence==0);
  sender.clock.now=100;
  require(sender.lifecycle->close_subscription("queued",1,"close-queued"));CHECK(sender.service->authorize_delivery(sending,page,100).code==sdk::Code::unauthorized);
  mesh::ArtifactChunk chunk;require(sender.service->artifact_chunk(sending,source,object,0,1024,100,chunk,cost));require(sender.service->authorize_artifact(sending,source,object,100));
  outgoing.generation=2;outgoing.artifacts.clear();require(sender.service->configure_grant(outgoing,1,"remove-artifact"));CHECK(sender.service->authorize_artifact(sending,source,object,100).code==sdk::Code::unauthorized);
  mesh::BootstrapOffer offer;require(sender.service->offer_bootstrap(sending,"bootstrap",1,source,900,100,offer));require(sender.service->authorize_bootstrap(sending,offer,100));
  mesh::ReceiveReceipt receipt;require(receiver.service->install_bootstrap(receiving,offer,100,receipt));CHECK(receipt.cursor.sequence==3&&!receipt.duplicate);
  auto changed=offer;sdk::EngineIdentity changed_id;sdk::EngineState state;require(sdk::decode_checkpoint(changed.checkpoint,changed_id,state));state.checkpoint.payload="different-but-valid";
  changed.checkpoint=sdk::encode_checkpoint(changed_id,state);changed.checkpoint_sha256=sdk::sha256(changed.checkpoint);
  CHECK(receiver.service->install_bootstrap(receiving,changed,100,receipt).code==sdk::Code::conflict);
  require(receiver.service->install_bootstrap(receiving,offer,100,receipt));CHECK(receipt.duplicate);
  // Bootstrap has no fabricated observation in either local or relay indexes.
  mesh::DeliveryItem absent;CHECK(receiver.service->received_observation("sender",source,1,absent).code==sdk::Code::not_found);
  ev::ObservationClaim local_observation;CHECK(receiver.store->lookup_observation(source.uid,source.epoch,1,local_observation).code==sdk::Code::not_found);
  outgoing.generation=3;outgoing.bootstrap=false;require(sender.service->configure_grant(outgoing,2,"remove-bootstrap"));
  CHECK(sender.service->authorize_bootstrap(sending,offer,100).code==sdk::Code::unauthorized);
  require(sender.service->cancel_bootstrap(offer.id,"cancel-offer"));require(sender.service->cancel_bootstrap(offer.id,"cancel-offer"));
  lc::Pin pin;CHECK(sender.lifecycle->pin(offer.id,pin).code==sdk::Code::not_found);
  sender.clock.now=101;ev::CommitBatch tick;tick.operation_id="authority-clock";tick.request_digest=sdk::sha256("authority-clock");require(sender.store->commit(tick,committed));
  CHECK(sender.service->authorize(sending,source,true,100).code==sdk::Code::expired);
  CHECK(sender.service->authorize(sending,source,true,1000).code==sdk::Code::unauthorized);
}
static void expired_history(const fs::path& root){
  Fixture sender(root/"expired-sender");(void)seed(sender,3);const auto id=identity();const lc::SourceRef source{id.source_uid,id.epoch};
  const auto session=sender.enroll("receiver","cert");require(sender.service->configure_grant({"receiver",id,1,true,false,true,{}},0,"grant"));
  require(sender.lifecycle->begin_drain(source,"drain-before-retention"));lc::Source sealed;require(sender.lifecycle->source(source,sealed));
  require(sender.lifecycle->seal(source,{sealed.last_sequence,sealed.last_payload_sha256},"seal-before-retention"));
  lc::MaintenanceRequest request{"expire-prefix",source,2,100,lc::HistoryAvailability::expired,{},{}};
  for(std::uint64_t sequence=1;sequence<=2;++sequence){ev::ObservationClaim claim;require(sender.store->lookup_observation(source.uid,source.epoch,sequence,claim));request.objects.push_back(claim.payload_digest);request.objects.push_back(claim.receipt_digest);}
  lc::MaintenancePlan plan;require(sender.lifecycle->review_maintenance(request,"review",plan));require(sender.lifecycle->activate_maintenance(request.id,100,"activate-maintenance"));require(sender.lifecycle->execute_maintenance(request.id,"execute-maintenance"));
  CHECK(sender.service->subscribe(session,{"expired","receiver",source,1,900,lc::SubscriptionPhase::active,{},{},false},100).code==sdk::Code::history_expired);
  mesh::BootstrapOffer offer;require(sender.service->offer_bootstrap(session,"expired",1,source,900,100,offer));CHECK(offer.tip.sequence==3);require(sender.service->cancel_bootstrap(offer.id,"cancel-unused"));
}
static void restored_scope(const fs::path& root){
  Fixture receiver(root/"restored-scope");const auto original=receiver.enroll("sender","old-cert");const auto id=identity();const lc::SourceRef source{id.source_uid,id.epoch};
  mesh::Grant grant{"sender",id,1,false,true,true,{}};require(receiver.service->configure_grant(grant,0,"grant-before-restore"));require(receiver.service->authorize(original,source,false,100));
  require(receiver.lifecycle->quarantine_restore("restore-quarantine",sdk::sha256("restore-incarnation")));
  require(receiver.lifecycle->reauthorize_restore(sdk::sha256("current-policy"),"current-policy"));
  require(receiver.lifecycle->upsert_peer({"sender",sdk::sha256("new-cert"),2,1000,lc::PeerPhase::active},1,100,"re-enroll"));
  mesh::Session current;require(receiver.service->authenticate({sdk::sha256("new-cert"),1000,true},"sender",100,current));
  CHECK(receiver.service->authorize(current,source,false,100).code==sdk::Code::unauthorized);
  CHECK(receiver.service->configure_grant(grant,0,"grant-before-restore").code==sdk::Code::expired);
  grant.generation=2;require(receiver.service->configure_grant(grant,1,"explicit-current-grant"));require(receiver.service->authorize(current,source,false,100));
  receiver.reopen();require(receiver.service->authorize(current,source,false,100));
}
int main(int argc,char** argv){CHECK(argc==2);const fs::path root=argv[1];std::error_code error;fs::remove_all(root,error);CHECK(!error);fs::create_directories(root);durable_pages(root);authorization_boundaries(root);expired_history(root);restored_scope(root);fairness();std::cout<<"canonical mesh durable pages, relay evidence, exact ACK, bootstrap identity, ownership, queued revocation, restored scope, replay expiry and fair scheduling passed\n";}
