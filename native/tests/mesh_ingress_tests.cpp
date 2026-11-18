// Real durable ingress boundaries with a test-only verified transport proof.
// TLS identity establishment has a separate native transport integration suite.
#include "anaiero/services/mesh_ingress_service.hpp"
#include "anaiero/services/mesh_recovery_service.hpp"
#include "anaiero/services/observation_checkpoint_service.hpp"
#include "anaiero/infrastructure/mesh_ingress_store.hpp"
#include "anaiero/infrastructure/windstore_epoch_observation_journal.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " at "<<__LINE__<<'\n';std::abort();}}while(false)
namespace mesh=anaiero::mesh;
namespace runtime=anaiero::runtime;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
struct Clock final:mesh::IUnixClock {
  std::uint64_t value=100;
  std::uint64_t unix_seconds()const noexcept override{return value;}
};
struct Peer final:mesh::IAuthenticatedPeer {
  std::string digest=std::string(64,'a');
  bool verified()const noexcept override{return true;}
  std::string_view certificate_sha256()const noexcept override{return digest;}
  std::uint64_t certificate_expiry_unix_seconds()const noexcept override{return 10000;}
};
struct PolicyStore final:mesh::IEnrollmentStore {
  mesh::EnrollmentPolicy value;
  PolicyStore(){value.revision=1;value.count=1;auto& peer=value.peers[0];peer.principal="node-a";
    peer.certificate_sha256=std::string(64,'a');peer.generation=1;peer.expires_unix_s=10000;
    peer.capabilities=1;peer.publication_count=1;peer.publications[0]={101,1,1,101};}
  bool load(mesh::EnrollmentPolicy& output,bool& found)noexcept override {output=value;found=true;return true;}
  mesh::EnrollmentStoreCode compare_exchange(std::uint64_t expected,const mesh::EnrollmentPolicy& next)noexcept override {
    if(value.revision!=expected)return mesh::EnrollmentStoreCode::conflict;
    value=next;return mesh::EnrollmentStoreCode::ok;
  }
};
struct Projection final:mesh::IMeshIngressProjection {
  runtime::ObservationCheckpointService state;
  bool block_prepare=false,block_apply=false;
  std::size_t preparations=0,applications=0;
  bool prepare(const runtime::Observation& o)noexcept override {
    ++preparations;return !block_prepare&&state.can_accept(o);
  }
  bool apply(const runtime::Observation& o)noexcept override {
    ++applications;return !block_apply&&state.apply(o);
  }
  std::uint64_t count()const noexcept{return state.snapshot().state.committed_updates;}
};
struct RecoveryProjection final:runtime::IObservationEpochProjection {
  runtime::ColonyProjector projector;
  bool enroll(runtime::SourcePermission)noexcept override{return false;}
  bool can_accept(const runtime::Observation& observation)const noexcept override{return projector.can_apply(observation)==runtime::Status::ok;}
  bool apply(const runtime::Observation& observation)noexcept override{return projector.apply(observation)==runtime::Status::ok;}
  runtime::ColonyState latest()const noexcept override{return projector.snapshot(UINT64_MAX,UINT64_MAX);}
  bool restore(const runtime::ColonyState& state)noexcept override{return projector.restore(state)==runtime::Status::ok;}
};
struct Scratch {
  fs::path path;
  explicit Scratch(std::string_view name){const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    path=fs::temp_directory_path()/("anaiero-mesh-ingress-"+std::string(name)+'-'+std::to_string(stamp));CHECK(fs::create_directories(path));}
  ~Scratch(){std::error_code ec;fs::remove_all(path,ec);}
};
struct Harness {
  db::Registry metadata,evidence;
  mesh::WindStoreMeshReplayStore replay;
  mesh::WindStoreMeshIngressStore store;
  PolicyStore policies;Clock clock;Peer peer;
  mesh::MeshAuthorizationService authorization;
  mesh::VerifiedMeshSession session;
  runtime::EpochObservationCodec codec;
  Projection projection;
  mesh::MeshIngressService ingress;
  explicit Harness(const fs::path& root,std::uint64_t now=100):
    replay(evidence,"mesh",root/"replay"),store(metadata,replay),authorization(policies,clock),
    ingress(authorization,store,projection,codec,clock){
    clock.value=now;
    CHECK(metadata.open(root/"metadata",db::StorageOptions::sustained(8))==db::Code::ok);
    CHECK(evidence.open(root/"evidence",db::StorageOptions::sustained(8))==db::Code::ok);
    CHECK(replay.initialize());CHECK(authorization.start());
    CHECK(authorization.authenticate(peer,session)==mesh::EnrollmentCode::ok);
  }
  bool recover_projection(){
    return evidence.visit_events(0,[&](const db::Event& event){
      if(event.kind!="OBSERVATION_RECORDED"||event.entity!="mesh")return false;
      std::string bytes;runtime::Observation o;
      if(!evidence.get(event.object_hash,bytes)||!codec.decode(
        {reinterpret_cast<const std::byte*>(bytes.data()),bytes.size()},o))return false;
      return projection.prepare(o)&&projection.apply(o);
    });
  }
};
static runtime::Observation observation(std::uint64_t sequence=1,std::uint64_t epoch=1){
  runtime::Observation o;o.source=101;o.source_epoch=epoch;o.clock_domain=1;o.colony=101;
  o.sequence=sequence;o.acquired_ns=sequence*1000;o.metric=runtime::Metric::activity;o.value=static_cast<std::int32_t>(sequence);return o;
}
static void authorize_before_receipt_and_order(){
  Scratch scratch("authorization");Harness h(scratch.path);mesh::VerifiedMeshSession unknown;
  CHECK(h.ingress.receive(unknown,observation()).code==mesh::ReplayCode::denied);
  auto wrong=observation();++wrong.source;
  CHECK(h.ingress.receive(h.session,wrong).code==mesh::ReplayCode::denied);
  CHECK(h.metadata.event_count()==0&&h.evidence.event_count()==0&&h.projection.preparations==0);
  CHECK(h.ingress.receive(h.session,observation(2)).code==mesh::ReplayCode::conflict);
  CHECK(h.metadata.event_count()==0&&h.evidence.event_count()==0);
  h.projection.block_prepare=true;
  CHECK(h.ingress.receive(h.session,observation()).code==mesh::ReplayCode::capacity);
  CHECK(h.metadata.event_count()==0&&h.evidence.event_count()==0);h.projection.block_prepare=false;
  const auto first=h.ingress.receive(h.session,observation());
  CHECK(first.code==mesh::ReplayCode::ok&&!first.duplicate&&first.committed.sequence==1);
  CHECK(h.metadata.event_count()==1&&h.evidence.event_count()==1&&h.projection.count()==1);
  CHECK(h.metadata.events().front().kind=="MESH_INGRESS_RECEIPT");CHECK(db::valid_kind("MESH_INGRESS_RECEIPT"));
  CHECK(first.receipt.peer.principal=="node-a"&&first.receipt.peer.generation==1);
  CHECK(first.receipt.certificate_sha256==std::string(64,'a')&&first.receipt.policy_revision==1);
  auto altered=observation();++altered.value;
  CHECK(h.ingress.receive(h.session,altered).code==mesh::ReplayCode::conflict);
  CHECK(h.ingress.receive(h.session,observation(3)).code==mesh::ReplayCode::conflict);
  CHECK(h.metadata.event_count()==1&&h.evidence.event_count()==1&&h.projection.count()==1);
  CHECK(h.ingress.receive(h.session,observation(2)).code==mesh::ReplayCode::ok);
  const auto count=h.projection.applications;
  const auto old=h.ingress.receive(h.session,observation());CHECK(old.code==mesh::ReplayCode::ok&&old.duplicate);
  CHECK(old.receipt.receipt_sha256==first.receipt.receipt_sha256&&old.committed==first.committed);
  CHECK(h.projection.applications==count&&h.projection.count()==2&&h.metadata.event_count()==2&&h.evidence.event_count()==2);
}
static void rotation_preserves_first_provenance(){
  Scratch scratch("rotation");Harness h(scratch.path);const auto first=h.ingress.receive(h.session,observation());
  CHECK(first.code==mesh::ReplayCode::ok);h.clock.value=150;
  CHECK(h.authorization.rotate("node-a",std::string(64,'b'),9000,1)==mesh::EnrollmentCode::ok);
  CHECK(h.ingress.receive(h.session,observation(2)).code==mesh::ReplayCode::denied);
  h.peer.digest=std::string(64,'b');CHECK(h.authorization.authenticate(h.peer,h.session)==mesh::EnrollmentCode::ok);
  const auto old=h.ingress.receive(h.session,observation());CHECK(old.code==mesh::ReplayCode::ok&&old.duplicate);
  CHECK(old.receipt.receipt_sha256==first.receipt.receipt_sha256);
  CHECK(old.receipt.peer.generation==1&&old.receipt.certificate_sha256==std::string(64,'a'));
  CHECK(old.receipt.policy_revision==1&&old.receipt.received_unix_s==100&&h.projection.count()==1);
  const auto next=h.ingress.receive(h.session,observation(2));CHECK(next.code==mesh::ReplayCode::ok&&!next.duplicate);
  CHECK(next.receipt.peer.generation==2&&next.receipt.certificate_sha256==std::string(64,'b'));
  CHECK(next.receipt.policy_revision==2&&next.receipt.received_unix_s==150);
}
static void local_import_keeps_its_origin(){
  Scratch scratch("local-import");Harness h(scratch.path);
  const auto first=h.ingress.receive(h.session,observation(),mesh::IngressKind::local_import);
  CHECK(first.code==mesh::ReplayCode::ok&&first.receipt.kind==mesh::IngressKind::local_import);
  const auto duplicate=h.ingress.receive(h.session,observation());
  CHECK(duplicate.code==mesh::ReplayCode::ok&&duplicate.duplicate);
  CHECK(duplicate.receipt.kind==mesh::IngressKind::local_import&&duplicate.receipt.receipt_sha256==first.receipt.receipt_sha256);
  const auto next=h.ingress.receive(h.session,observation(2));
  CHECK(next.code==mesh::ReplayCode::ok&&next.receipt.kind==mesh::IngressKind::mutual_tls);
}
static void principal_cannot_inherit_stream(){
  Scratch scratch("owner");Harness h(scratch.path);CHECK(h.ingress.receive(h.session,observation()).code==mesh::ReplayCode::ok);
  auto newcomer=h.authorization.snapshot().peers[0];newcomer.principal="node-b";newcomer.certificate_sha256=std::string(64,'b');
  CHECK(h.authorization.revoke("node-a",1)==mesh::EnrollmentCode::ok);
  CHECK(h.authorization.upsert(newcomer,2)==mesh::EnrollmentCode::ok);
  h.peer.digest=newcomer.certificate_sha256;CHECK(h.authorization.authenticate(h.peer,h.session)==mesh::EnrollmentCode::ok);
  CHECK(h.ingress.receive(h.session,observation()).code==mesh::ReplayCode::conflict);
  CHECK(h.ingress.receive(h.session,observation(2)).code==mesh::ReplayCode::conflict);
  CHECK(h.metadata.event_count()==1&&h.evidence.event_count()==1&&h.projection.count()==1);
  ++newcomer.generation;newcomer.publications[0].epoch=2;
  CHECK(h.authorization.upsert(newcomer,3)==mesh::EnrollmentCode::ok);
  CHECK(h.authorization.authenticate(h.peer,h.session)==mesh::EnrollmentCode::ok);
  const auto fresh=h.ingress.receive(h.session,observation(1,2));CHECK(fresh.code==mesh::ReplayCode::ok&&!fresh.duplicate);
  CHECK(fresh.receipt.peer.principal=="node-b"&&fresh.receipt.scope.epoch==2&&h.projection.count()==2);
}
static void receipt_commit_before_evidence(){
  Scratch scratch("receipt-crash");std::string original;
  {Harness h(scratch.path);h.metadata.inject_next_publish(db::FaultSite::after_head_commit);
    CHECK(h.ingress.receive(h.session,observation()).code==mesh::ReplayCode::persistence_failed);
    CHECK(h.ingress.failed()&&h.evidence.event_count()==0&&h.projection.count()==0);
    CHECK(h.ingress.receive(h.session,observation()).code==mesh::ReplayCode::persistence_failed);}
  {Harness h(scratch.path,150);CHECK(h.metadata.event_count()==1&&h.evidence.event_count()==0);
    original=h.metadata.events().front().object_hash;
    const auto recovered=h.ingress.receive(h.session,observation());
    CHECK(recovered.code==mesh::ReplayCode::ok&&!recovered.duplicate);
    CHECK(recovered.receipt.receipt_sha256==original&&recovered.receipt.received_unix_s==100);
    CHECK(h.metadata.event_count()==1&&h.evidence.event_count()==1&&h.projection.count()==1);}
}
static void evidence_crash_and_restart(){
  for(const auto cut:{db::FaultSite::after_journal_sync,db::FaultSite::after_head_commit}){
    Scratch scratch("evidence-crash");
    {Harness h(scratch.path);h.evidence.inject_next_publish(cut);
      CHECK(h.ingress.receive(h.session,observation()).code==mesh::ReplayCode::persistence_failed);
      CHECK(h.ingress.failed()&&h.metadata.event_count()==1&&h.projection.count()==0);
      CHECK(h.ingress.receive(h.session,observation(2)).code==mesh::ReplayCode::persistence_failed);}
    {Harness h(scratch.path,150);const bool committed=cut==db::FaultSite::after_head_commit;
      CHECK(h.evidence.event_count()==(committed?1u:0u));CHECK(h.recover_projection());
      CHECK(h.projection.count()==(committed?1u:0u));
      const auto result=h.ingress.receive(h.session,observation());
      CHECK(result.code==mesh::ReplayCode::ok&&result.duplicate==committed&&result.receipt.received_unix_s==100);
      CHECK(h.metadata.event_count()==1&&h.evidence.event_count()==1&&h.projection.count()==1);
      CHECK(h.ingress.receive(h.session,observation(2)).code==mesh::ReplayCode::ok&&h.projection.count()==2);}
  }
}
static void projection_failure_needs_recovery(){
  Scratch scratch("projection-failure");
  {Harness h(scratch.path);h.projection.block_apply=true;
    CHECK(h.ingress.receive(h.session,observation()).code==mesh::ReplayCode::persistence_failed);
    CHECK(h.ingress.failed()&&h.projection.count()==0&&h.metadata.event_count()==1&&h.evidence.event_count()==1);
    h.projection.block_apply=false;
    CHECK(h.ingress.receive(h.session,observation()).code==mesh::ReplayCode::persistence_failed);
    CHECK(h.projection.count()==0);}
  {Harness h(scratch.path);CHECK(h.recover_projection());CHECK(h.projection.count()==1);
    const auto old=h.ingress.receive(h.session,observation());CHECK(old.code==mesh::ReplayCode::ok&&old.duplicate);
    CHECK(h.projection.count()==1);CHECK(h.ingress.receive(h.session,observation(2)).code==mesh::ReplayCode::ok);
    CHECK(h.projection.count()==2);}
}
static void receipt_integrity(){
  {Scratch scratch("missing-record");Harness h(scratch.path);
    CHECK(h.replay.append(observation())==mesh::ReplayCode::ok);CHECK(h.metadata.event_count()==0);
    CHECK(h.ingress.receive(h.session,observation()).code==mesh::ReplayCode::corrupt);
    CHECK(h.ingress.failed()&&h.metadata.event_count()==0&&h.projection.count()==0);}
  for(const bool erase:{false,true}){
    Scratch scratch("receipt-tamper");Harness h(scratch.path);
    const auto first=h.ingress.receive(h.session,observation());CHECK(first.code==mesh::ReplayCode::ok);
    const auto path=scratch.path/"metadata"/"objects"/first.receipt.receipt_sha256;
    if(erase)CHECK(fs::remove(path));else {std::ofstream file(path,std::ios::binary|std::ios::trunc);CHECK(file.good());file<<"tampered\n";}
    CHECK(h.ingress.receive(h.session,observation()).code==mesh::ReplayCode::corrupt);
    CHECK(h.ingress.failed()&&h.projection.count()==1&&h.evidence.event_count()==1);
  }
}
static void verified_recovery_and_audit(){
  Scratch scratch("verified-recovery");Harness h(scratch.path);std::string first_receipt;
  for(std::uint64_t i=1;i<=8;++i){const auto result=h.ingress.receive(h.session,observation(i));
    CHECK(result.code==mesh::ReplayCode::ok);if(i==1)first_receipt=result.receipt.receipt_sha256;}
  CHECK(h.store.verify_committed(observation())==mesh::ReplayCode::ok);
  auto changed=observation();++changed.value;CHECK(h.store.verify_committed(changed)==mesh::ReplayCode::corrupt);
  changed=observation();++changed.clock_domain;CHECK(h.store.verify_committed(changed)==mesh::ReplayCode::corrupt);
  CHECK(h.store.verify_committed(observation(9))==mesh::ReplayCode::corrupt);
  runtime::WindStoreEpochObservationJournal journal(h.evidence,"mesh");mesh::MeshRecoveryService recovery(h.codec,h.store);
  RecoveryProjection whole;CHECK(recovery.recover(journal,whole,h.codec));CHECK(whole.latest().committed_updates==8);
  CHECK(recovery.audit(journal));
  db::Registry stale_metadata;CHECK(stale_metadata.open(scratch.path/"stale-metadata",db::StorageOptions::sustained(8))==db::Code::ok);
  mesh::WindStoreMeshIngressStore missing(stale_metadata,h.replay);mesh::MeshRecoveryService stale(h.codec,missing);
  RecoveryProjection before_checkpoint;CHECK(!stale.recover(journal,before_checkpoint,h.codec));
  CHECK(before_checkpoint.latest().committed_updates==0);
  std::string checkpoint;CHECK(runtime::ObservationCheckpointCodec::encode(h.projection.state.snapshot(),checkpoint,"mesh"));
  CHECK(h.evidence.save_replay_checkpoint(checkpoint));
  RecoveryProjection checkpoint_recovery;CHECK(recovery.recover(journal,checkpoint_recovery,h.codec));
  CHECK(checkpoint_recovery.latest().committed_updates==8);
  RecoveryProjection missing_tip;CHECK(!stale.recover(journal,missing_tip,h.codec));CHECK(missing_tip.latest().committed_updates==0);
  // Bounded restart verifies checkpoint tips; explicit full audit also checks
  // older receipt payloads that are not reread during checkpoint restoration.
  CHECK(fs::remove(scratch.path/"metadata"/"objects"/first_receipt));
  RecoveryProjection bounded;CHECK(recovery.recover(journal,bounded,h.codec));CHECK(bounded.latest().committed_updates==8);
  CHECK(!recovery.audit(journal));
}
int main(){authorize_before_receipt_and_order();rotation_preserves_first_provenance();local_import_keeps_its_origin();principal_cannot_inherit_stream();
  receipt_commit_before_evidence();evidence_crash_and_restart();projection_failure_needs_recovery();receipt_integrity();verified_recovery_and_audit();
  std::cout<<"mesh ingress passed\n";}
