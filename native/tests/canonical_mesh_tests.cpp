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
static void receiver_retention(const fs::path& root){
  Fixture sender(root/"retention-sender"),receiver(root/"retention-receiver");auto state=seed(sender,6);
  const auto sending=sender.enroll("receiver","receiver-cert"),receiving=receiver.enroll("sender","sender-cert");
  const auto id=identity();const lc::SourceRef source{id.source_uid,id.epoch};const std::string artifact_bytes(262144,'a');const auto artifact=sdk::sha256(artifact_bytes);
  require(sender.service->configure_grant({"receiver",id,1,true,false,true,{}},0,"retention-outbound"));
  require(receiver.service->configure_grant({"sender",id,1,false,true,true,{artifact}},0,"retention-inbound"));
  require(sender.service->subscribe(sending,{"sub","receiver",source,1,900,lc::SubscriptionPhase::active,{},{},false},100));
  mesh::ReceiveReceipt receipt;mesh::Delivery first,last;mesh::TransferCost cost;
  for(unsigned i=0;i<3;++i){mesh::Delivery page;require(sender.service->next(sending,"sub",1,100,page,cost));
    require(receiver.service->receive(receiving,page,100,receipt));require(sender.service->acknowledge(sending,page,100));if(i==0)first=page;last=page;}
  require(sender.service->subscribe(sending,{"overlap","receiver",source,1,900,lc::SubscriptionPhase::active,{},{},false},100));
  mesh::Delivery overlapping;require(sender.service->next(sending,"overlap",1,100,overlapping,cost));
  require(receiver.service->receive(receiving,overlapping,100,receipt));const auto overlapping_receipt=receipt.receipt_sha256;
  require(sender.service->acknowledge(sending,overlapping,100));
  mesh::ReceiverRetentionPlan plan;mesh::ReceiverRetentionRequest request{"prefix","sender",source,1,2,{},{}};
  auto lagging=request;lagging.id="lagging-cursor";require(receiver.service->review_receiver_retention(lagging,"review-lagging",plan));
  CHECK(receiver.service->activate_receiver_retention(lagging.id,"activate-lagging").code==sdk::Code::conflict);
  for(unsigned i=0;i<2;++i){mesh::Delivery page;require(sender.service->next(sending,"overlap",1,100,page,cost));
    require(receiver.service->receive(receiving,page,100,receipt));require(sender.service->acknowledge(sending,page,100));}
  auto partial=request;partial.last_sequence=1;CHECK(receiver.service->review_receiver_retention(partial,"partial-page",plan).code==sdk::Code::invalid);
  auto current=request;current.first_sequence=5;current.last_sequence=6;CHECK(receiver.service->review_receiver_retention(current,"latest-page",plan).code==sdk::Code::busy);
  auto too_big=request;too_big.last_sequence=33;CHECK(receiver.service->review_receiver_retention(too_big,"large-page",plan).code==sdk::Code::invalid);
  auto wrong=request;wrong.principal="different";CHECK(receiver.service->review_receiver_retention(wrong,"wrong-principal",plan).code==sdk::Code::unauthorized);
  require(receiver.service->review_receiver_retention(request,"review-prefix",plan));CHECK(plan.objects.size()==3&&plan.expected_bytes>0);
  const auto reviewed=plan.reviewed_root;require(receiver.service->review_receiver_retention(request,"review-prefix",plan));CHECK(plan.reviewed_root==reviewed);
  CHECK(receiver.service->reclaim_receiver_retention(request.id,"early-reclaim").code==sdk::Code::conflict);
  lc::Pin pin{"reader","operator",lc::PinKind::reader,1,0,{first.observations.front().claim.payload_digest}};
  require(receiver.lifecycle->put_pin(pin,0,100,"protect-prefix"));
  CHECK(receiver.service->activate_receiver_retention(request.id,"stale-activation").code==sdk::Code::conflict);
  request.id="pinned-prefix";require(receiver.service->review_receiver_retention(request,"review-pinned",plan));
  CHECK(receiver.service->activate_receiver_retention(request.id,"pinned-activation").code==sdk::Code::conflict);
  mesh::DeliveryItem observed;require(receiver.service->received_observation("sender",source,1,observed));
  require(receiver.lifecycle->release_pin(pin.id,1,"release-reader"));
  CHECK(receiver.service->activate_receiver_retention(request.id,"pinned-activation").code==sdk::Code::conflict);
  request.id="expire-prefix";require(receiver.service->review_receiver_retention(request,"review-expiry",plan));
  const auto expired_objects=plan.objects;ev::set_native_store_fault(*receiver.store,ev::FaultPoint::after_root);
  CHECK(receiver.service->activate_receiver_retention(request.id,"activate-expiry").committed());receiver.reopen();
  require(receiver.service->activate_receiver_retention(request.id,"activate-expiry"));
  CHECK(receiver.service->received_observation("sender",source,1,observed).code==sdk::Code::history_expired);
  CHECK(receiver.service->receive(receiving,first,100,receipt).code==sdk::Code::history_expired);
  require(receiver.service->receive(receiving,last,100,receipt));CHECK(receipt.duplicate&&receipt.cursor.sequence==6);
  ev::set_native_store_fault(*receiver.store,ev::FaultPoint::after_root);
  CHECK(receiver.service->reclaim_receiver_retention(request.id,"reclaim-expiry").committed());receiver.reopen();
  require(receiver.service->reclaim_receiver_retention(request.id,"reclaim-expiry"));require(receiver.service->reclaim_receiver_retention(request.id,"reclaim-expiry"));
  for(const auto& object:expired_objects){ev::BlobInfo info;require(receiver.store->blob_info(object,info));CHECK(info.expired_generation&&!info.present);}
  require(receiver.service->receiver_retention(request.id,plan));CHECK(plan.phase==lc::MaintenancePhase::completed);
  CHECK(receiver.service->receive(receiving,overlapping,100,receipt).code==sdk::Code::history_expired);
  // A second subscription's independently retained copy is selected explicitly
  // by its returned receipt digest; no unbounded scan discovers historical peers.
  request={"overlap-receipt","sender",source,0,0,{},{},{overlapping_receipt}};
  require(receiver.service->review_receiver_retention(request,"review-overlap",plan));CHECK(plan.objects.size()==1);
  require(receiver.service->activate_receiver_retention(request.id,"activate-overlap"));
  require(receiver.service->reclaim_receiver_retention(request.id,"reclaim-overlap"));
  ev::BlobInfo overlapping_info;require(receiver.store->blob_info(overlapping_receipt,overlapping_info));CHECK(overlapping_info.expired_generation&&!overlapping_info.present);
  // Artifact expiry keeps the accepted identity and rejects exact retransmission.
  require(receiver.service->receive_artifact(receiving,source,artifact,artifact_bytes,100,receipt));
  request={"expire-artifact","sender",source,0,0,{artifact},{}};
  require(receiver.service->review_receiver_retention(request,"review-artifact",plan));require(receiver.service->activate_receiver_retention(request.id,"activate-artifact"));
  const auto before_reclaim=receiver.store->info();ev::set_native_store_fault(*receiver.store,ev::FaultPoint::after_objects);
  CHECK(!receiver.service->reclaim_receiver_retention(request.id,"reclaim-artifact").committed());receiver.reopen();
  CHECK(receiver.store->generation()==before_reclaim.generation);
  ev::BlobInfo interrupted;require(receiver.store->blob_info(artifact,interrupted));CHECK(!interrupted.present&&interrupted.expired_generation);
  require(receiver.service->reclaim_receiver_retention(request.id,"reclaim-artifact"));
  CHECK(receiver.store->info().physical_bytes+200000<before_reclaim.physical_bytes);
  CHECK(receiver.service->receive_artifact(receiving,source,artifact,artifact_bytes,100,receipt).code==sdk::Code::history_expired);
  // A checkpoint remains protected until its subscription receives a later page.
  mesh::BootstrapOffer offer;require(sender.service->offer_bootstrap(sending,"boot-sub",1,source,900,100,offer));
  require(receiver.service->install_bootstrap(receiving,offer,100,receipt));require(sender.service->accept_bootstrap(sending,offer,100));
  request={"expire-checkpoint","sender",source,0,0,{}, {offer.id}};
  CHECK(receiver.service->review_receiver_retention(request,"review-current-checkpoint",plan).code==sdk::Code::busy);
  for(std::uint64_t i=7;i<=8;++i){auto prior=state;state.revision=i;state.tick=i;state.checkpoint.payload=std::to_string(i);const auto operation="step-"+std::to_string(i);
    sender.commit({id,operation,sdk::sha256(operation),sdk::state_digest(prior),prior.revision,state,{{"test.observation/1","metric","count","unit",static_cast<std::int64_t>(i),i}},{}});}
  mesh::Delivery suffix;require(sender.service->next(sending,"boot-sub",1,100,suffix,cost));require(receiver.service->receive(receiving,suffix,100,receipt));
  require(receiver.service->review_receiver_retention(request,"review-old-checkpoint",plan));require(receiver.service->activate_receiver_retention(request.id,"activate-checkpoint"));
  require(receiver.service->reclaim_receiver_retention(request.id,"reclaim-checkpoint"));
  CHECK(receiver.service->install_bootstrap(receiving,offer,100,receipt).code==sdk::Code::history_expired);
  require(receiver.service->received("sender","boot-sub",source,receipt));CHECK(receipt.cursor.sequence==8);
  ev::AuditReport audit;require(receiver.store->audit(audit));require(receiver.store->repair_index());receiver.reopen();
  require(receiver.store->audit(audit));CHECK(receiver.service->received_observation("sender",source,2,observed).code==sdk::Code::history_expired);
  lc::SourceAuthority authority;require(receiver.lifecycle->source_authority(source.uid,authority));CHECK(authority.owner=="sender"&&authority.kind==lc::SourceAuthorityKind::relay);
}
static void overlapping_retention_pages(const fs::path& root){
  Fixture sender(root/"retention-overlap-sender"),receiver(root/"retention-overlap-receiver");(void)seed(sender,4);
  const auto session=receiver.enroll("sender","sender-cert");const auto id=identity();const lc::SourceRef source{id.source_uid,id.epoch};
  mesh::Options options;options.maximum_page_observations=3;receiver.service=std::make_unique<mesh::Service>(*receiver.store,options);
  require(receiver.service->configure_grant({"sender",id,1,false,true,true,{}},0,"grant"));
  const auto deliver=[&](std::string subscription,std::uint64_t after,std::uint64_t through){
    mesh::Delivery page;page.subscription_id=std::move(subscription);page.generation=1;page.source=id;
    if(after){ev::ObservationClaim claim;require(sender.store->lookup_observation(source.uid,source.epoch,after,claim));page.from={after,claim.payload_digest};}
    for(auto sequence=after+1;sequence<=through;++sequence){mesh::DeliveryItem item;require(sender.store->lookup_observation(source.uid,source.epoch,sequence,item.claim));require(sender.store->get_blob(item.claim.payload_digest,item.payload));page.observations.push_back(std::move(item));}
    page.to={through,page.observations.back().claim.payload_digest};mesh::ReceiveReceipt receipt;require(receiver.service->receive(session,page,100,receipt));
  };
  deliver("two",0,2);deliver("three",0,3);deliver("two",2,4);deliver("three",3,4);
  mesh::ReceiverRetentionPlan plan;mesh::ReceiverRetentionRequest request{"first","sender",source,1,2,{},{}};
  require(receiver.service->review_receiver_retention(request,"review-first",plan));require(receiver.service->activate_receiver_retention(request.id,"activate-first"));require(receiver.service->reclaim_receiver_retention(request.id,"reclaim-first"));
  request.id="remaining";request.first_sequence=3;request.last_sequence=3;
  require(receiver.service->review_receiver_retention(request,"review-remaining",plan));CHECK(plan.objects.size()==2);
  require(receiver.service->activate_receiver_retention(request.id,"activate-remaining"));require(receiver.service->reclaim_receiver_retention(request.id,"reclaim-remaining"));
  for(std::uint64_t sequence=1;sequence<=3;++sequence){mesh::DeliveryItem item;CHECK(receiver.service->received_observation("sender",source,sequence,item).code==sdk::Code::history_expired);}
  ev::AuditReport audit;require(receiver.store->audit(audit));
}
int main(int argc,char** argv){CHECK(argc==2);const fs::path root=argv[1];std::error_code error;fs::remove_all(root,error);CHECK(!error);fs::create_directories(root);durable_pages(root);authorization_boundaries(root);expired_history(root);restored_scope(root);receiver_retention(root);overlapping_retention_pages(root);fairness();std::cout<<"canonical mesh durable pages, relay evidence, exact ACK, bootstrap identity, ownership, queued revocation, restored scope, receiver retention/recovery, replay expiry and fair scheduling passed\n";}
