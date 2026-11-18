#pragma once
// Composition and single-owner gateway. Remote messages cannot reach the
// native session command dispatcher or any solver/experiment launch API.
#include "anaiero/contracts/mesh_host.hpp"
#include "anaiero/services/mesh_authorization_service.hpp"
#include "anaiero/services/mesh_ingress_service.hpp"
#include "anaiero/services/mesh_recovery_service.hpp"
#include "anaiero/services/mesh_replay_service.hpp"
#include "anaiero/services/mesh_artifact_service.hpp"
#include "anaiero/services/runtime_service.hpp"
#include "anaiero/services/epoch_observation_delivery_service.hpp"
#include "anaiero/services/observation_checkpoint_service.hpp"
#include "anaiero/infrastructure/mesh_configuration.hpp"
#include "anaiero/infrastructure/mesh_enrollment_store.hpp"
#include "anaiero/infrastructure/mesh_ingress_store.hpp"
#include "anaiero/infrastructure/mesh_wire.hpp"
#include "anaiero/infrastructure/windstore_mesh_artifact.hpp"
#include "anaiero/infrastructure/windstore_epoch_observation_journal.hpp"
#include "anaiero/infrastructure/epoch_runtime_projection.hpp"
#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/infrastructure/service_trace.hpp"
#include <array>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
namespace anaiero::application {
class NativeMeshApplication final:public mesh::IMeshGateway {
  using Registry=christeye::living::durable::Registry;
  class Projection final:public runtime::IObservationEpochProjection {
    runtime::EpochRuntimeProjectionPort delegate_;
    std::array<runtime::SourcePermission,runtime::kMaxSources> sources_{};
    std::size_t count_=0;
    bool ensure(std::uint32_t source,std::uint32_t clock)noexcept {
      for(std::size_t i=0;i<count_;++i)if(sources_[i].source==source)return sources_[i].clock_domain==clock;
      if(count_==sources_.size())return false;
      const runtime::SourcePermission permission{source,clock,runtime::Origin::simulated,true,true,true,0};
      if(!delegate_.enroll(permission))return false;
      sources_[count_++]=permission;return true;
    }
  public:
    explicit Projection(runtime::IObservationRuntime& runtime)noexcept:delegate_(runtime){}
    bool enroll(runtime::SourcePermission)noexcept override{return false;}
    bool prepare(const runtime::Observation& observation)noexcept {
      return runtime::valid_observation(observation)&&observation.origin==runtime::Origin::simulated&&
        ensure(observation.source,observation.clock_domain)&&delegate_.can_accept(observation);
    }
    bool can_accept(const runtime::Observation& observation)const noexcept override{return delegate_.can_accept(observation);}
    bool apply(const runtime::Observation& observation)noexcept override {
      return observation.origin==runtime::Origin::simulated&&ensure(observation.source,observation.clock_domain)&&delegate_.apply(observation);
    }
    runtime::ColonyState latest()const noexcept override{return delegate_.latest();}
    bool restore(const runtime::ColonyState& state)noexcept override {
      if(state.sources>runtime::kMaxSources)return false;
      for(std::size_t i=0;i<state.sources;++i)if(!ensure(state.cursors[i].id,state.cursors[i].clock_domain))return false;
      return delegate_.restore(state);
    }
  };
  class IngressProjection final:public mesh::IMeshIngressProjection {
    Projection& projection_;runtime::ObservationCheckpointService& checkpoint_;
    runtime::WindStoreEpochObservationJournal& journal_;Registry& evidence_;
  public:
    IngressProjection(Projection& projection,runtime::ObservationCheckpointService& checkpoint,
      runtime::WindStoreEpochObservationJournal& journal,Registry& evidence)noexcept:
      projection_(projection),checkpoint_(checkpoint),journal_(journal),evidence_(evidence){}
    bool prepare(const runtime::Observation& observation)noexcept override {
      return checkpoint_.can_accept(observation)&&projection_.prepare(observation);
    }
    bool apply(const runtime::Observation& observation)noexcept override {
      if(!projection_.apply(observation)||!checkpoint_.apply(observation))return false;
      const auto capacity=evidence_.capacity();
      return !capacity.event_limit||capacity.total_events%capacity.event_limit||journal_.checkpoint_now();
    }
  };
  struct PeerSlot {std::uint64_t id=0;mesh::VerifiedMeshSession session{};};
  mesh::files::MeshConfiguration configuration_{};
  std::filesystem::path root_;
  Registry metadata_,evidence_,artifacts_;
  mesh::files::SystemUnixClock clock_;
  infrastructure::ServiceTrace trace_;
  runtime::WeightedQueue<> queue_;
  runtime::RuntimeService runtime_{queue_};
  Projection projection_{runtime_};
  runtime::EpochObservationCodec codec_;
  runtime::ObservationCheckpointService checkpoint_;
  std::unique_ptr<mesh::WindStoreEnrollmentStore> policy_store_;
  std::unique_ptr<mesh::MeshAuthorizationService> authorization_;
  std::unique_ptr<mesh::WindStoreMeshReplayStore> replay_store_;
  std::unique_ptr<mesh::MeshReplayService> replay_;
  std::unique_ptr<runtime::WindStoreEpochObservationJournal> journal_;
  std::unique_ptr<IngressProjection> ingress_projection_;
  std::unique_ptr<mesh::WindStoreMeshIngressStore> ingress_store_;
  std::unique_ptr<mesh::MeshIngressService> ingress_;
  std::unique_ptr<mesh::WindStoreMeshArtifactStore> artifact_store_;
  std::unique_ptr<mesh::MeshArtifactService> artifact_service_;
  std::unique_ptr<mesh::MeshArtifactReceiver> artifact_receiver_;
  std::array<PeerSlot,8> peers_{};
  std::uint64_t next_peer_=1;
  std::string error_;
  PeerSlot* peer(std::uint64_t id)noexcept {
    if(!id)return nullptr;
    for(auto& slot:peers_)if(slot.id==id)return &slot;
    return nullptr;
  }
  bool identity(){
    namespace db=christeye::living::durable;
    db::Event event;bool found=false;
    if(!metadata_.latest_event("CULTURE_CHECKPOINT","mesh-root","mesh-identity",event,found,"anaiero-mesh"))return false;
    const auto bytes="CEVMESHIDENTITY/1\n"+configuration_.name+'\n'+configuration_.principal+'\n';
    if(found){std::string prior;return metadata_.get(event.object_hash,prior)&&prior==bytes;}
    if(evidence_.event_count()||artifacts_.event_count())return false;
    const auto object=metadata_.put(bytes);
    return object.code==db::Code::ok&&metadata_.publish("CULTURE_CHECKPOINT","mesh-root",object.hash,
      "IMPORTED_UNVERIFIED",1,"mesh-identity","anaiero-mesh").code==db::Code::ok;
  }
  static std::string replay_error(mesh::ReplayCode code){return mesh::wire::error("replay_"+std::to_string(static_cast<unsigned>(code)));}
public:
  NativeMeshApplication()=default;
  NativeMeshApplication(const NativeMeshApplication&)=delete;
  NativeMeshApplication& operator=(const NativeMeshApplication&)=delete;
  bool open(const std::filesystem::path& configuration,const std::filesystem::path& root){
    namespace db=christeye::living::durable;
    if(authorization_)return false;
    if(!mesh::files::load_configuration(configuration,configuration_,error_))return false;
    root_=root;
    const auto options=db::StorageOptions::sustained(64);
    if(metadata_.open(root/"metadata",options)!=db::Code::ok||evidence_.open(root/"evidence",options)!=db::Code::ok||
      artifacts_.open(root/"artifacts",options)!=db::Code::ok){error_="mesh state is unavailable, corrupt, or already owned";return false;}
    if(!identity()){error_="mesh state identity mismatch";return false;}
    policy_store_=std::make_unique<mesh::WindStoreEnrollmentStore>(metadata_,configuration_.name);
    authorization_=std::make_unique<mesh::MeshAuthorizationService>(*policy_store_,clock_);
    authorization_->set_extension(&trace_);
    if(!authorization_->start()){error_="mesh enrollment recovery failed";return false;}
    replay_store_=std::make_unique<mesh::WindStoreMeshReplayStore>(evidence_,configuration_.name,root/"replay");
    if(!replay_store_->initialize()){error_="mesh subscriptions are invalid";return false;}
    ingress_store_=std::make_unique<mesh::WindStoreMeshIngressStore>(metadata_,*replay_store_);
    journal_=std::make_unique<runtime::WindStoreEpochObservationJournal>(evidence_,configuration_.name,&checkpoint_);
    runtime_.set_extension(&trace_);
    mesh::MeshRecoveryService recovery(codec_,*ingress_store_);
    if(!recovery.recover(*journal_,projection_,codec_)){error_="mesh evidence/provenance recovery failed";return false;}
    replay_=std::make_unique<mesh::MeshReplayService>(*replay_store_);replay_->set_extension(&trace_);
    ingress_projection_=std::make_unique<IngressProjection>(projection_,checkpoint_,*journal_,evidence_);
    ingress_=std::make_unique<mesh::MeshIngressService>(*authorization_,*ingress_store_,*ingress_projection_,codec_,clock_);
    ingress_->set_extension(&trace_);
    artifact_store_=std::make_unique<mesh::WindStoreMeshArtifactStore>(artifacts_);
    artifact_service_=std::make_unique<mesh::MeshArtifactService>(*artifact_store_);artifact_service_->set_extension(&trace_);
    artifact_receiver_=std::make_unique<mesh::MeshArtifactReceiver>(*artifact_store_);artifact_receiver_->set_extension(&trace_);
    return true;
  }
  const mesh::files::MeshConfiguration& configuration()const noexcept{return configuration_;}
  const std::string& error()const noexcept{return error_;}
  mesh::MeshAuthorizationService& authorization()noexcept{return *authorization_;}
  mesh::WindStoreMeshReplayStore& replay_store()noexcept{return *replay_store_;}
  mesh::MeshReplayService& replay()noexcept{return *replay_;}
  mesh::MeshArtifactReceiver& artifact_receiver()noexcept{return *artifact_receiver_;}
  mesh::WindStoreMeshArtifactStore& artifact_store()noexcept{return *artifact_store_;}
  mesh::IngressResult receive(const mesh::VerifiedMeshSession& session,const runtime::Observation& observation,
    mesh::IngressKind kind=mesh::IngressKind::mutual_tls)noexcept {
    return ingress_->receive(session,observation,kind);
  }
  runtime::ColonyState snapshot()const noexcept{return projection_.latest();}
  bool open_peer(const mesh::IAuthenticatedPeer& proof,std::uint64_t& id)override {
    id=0;mesh::VerifiedMeshSession session;
    if(!authorization_||authorization_->authenticate(proof,session)!=mesh::EnrollmentCode::ok||next_peer_==UINT64_MAX)return false;
    for(auto& existing:peers_)if(existing.id&&!authorization_->active(existing.session))close_peer(existing.id);
    for(const auto& existing:peers_)if(existing.id&&
      existing.session.authorized_peer().principal==session.authorized_peer().principal)return false;
    for(auto& slot:peers_)if(!slot.id){slot={next_peer_++,std::move(session)};id=slot.id;return true;}
    return false;
  }
  void close_peer(std::uint64_t id)override {
    if(auto* slot=peer(id)){replay_->disconnect(slot->session.authorized_peer());
      artifact_receiver_->cancel(slot->session.authorized_peer());*slot={};}
  }
  std::string request(std::uint64_t id,std::string_view bytes)override {
    using namespace mesh;
    auto* slot=peer(id);if(!slot)return wire::error("denied");
    if(!authorization_->active(slot->session)){close_peer(id);return wire::error("denied");}
    wire::Request message;if(!wire::decode(bytes,message))return wire::error("malformed");
    const auto& session=slot->session;const auto& principal=session.authorized_peer();
    switch(message.command){
      case wire::Command::ping:return "CEVMESH/1 PONG\n";
      case wire::Command::publish:{
        const auto result=ingress_->receive(session,message.observation);
        if(result.code!=ReplayCode::ok)return replay_error(result.code);
        return "CEVMESH/1 PERSISTED "+std::to_string(result.committed.sequence)+' '+result.committed.hash+' '+
          result.receipt.receipt_sha256+' '+result.receipt.payload_sha256+' '+(result.duplicate?"1":"0")+'\n';
      }
      case wire::Command::subscribe:{
        if(!authorization_->may_subscribe(session,message.scope))return wire::error("denied");
        // Pin the receiver's durable tip before replaying any earlier lost ACK.
        // A restored/forked server cannot silently skip or replace that history.
        ReplayPage probe;
        if(replay_store_->read_page(message.scope,message.cursor,probe)!=ReplayCode::ok)return wire::error("receive_tip_conflict");
        ReplayCursor cursor;const auto code=replay_->open(principal,message.id,message.scope,cursor);
        return code==ReplayCode::ok?wire::cursor_reply("SUBSCRIBED",cursor):replay_error(code);
      }
      case wire::Command::next:case wire::Command::ack:{
        Scope scope;const auto found=replay_->scope(principal,message.id,scope);
        if(found!=ReplayCode::ok||!authorization_->may_subscribe(session,scope))return wire::error("denied");
        if(message.command==wire::Command::next){ReplayPage page;const auto code=replay_->offer(principal,message.id,page);
          return code==ReplayCode::ok?wire::page(page):replay_error(code);}
        const auto code=replay_->acknowledge(principal,message.id,message.cursor);
        return code==ReplayCode::ok?wire::cursor_reply("ACKNOWLEDGED",message.cursor):replay_error(code);
      }
      case wire::Command::artifact:case wire::Command::chunk:{
        if(!authorization_->may_read_artifact(session,message.digest))return wire::error("denied");
        if(message.command==wire::Command::artifact){ArtifactReceipt receipt;
          const auto code=artifact_service_->describe(principal,message.digest,receipt);
          return code==ArtifactCode::ok?wire::artifact(receipt):wire::error("artifact_unavailable");}
        ArtifactChunk chunk;const auto code=artifact_service_->read(principal,message.digest,message.offset,chunk);
        return code==ArtifactCode::ok?wire::chunk(chunk):wire::error("artifact_unavailable");
      }
    }
    return wire::error("malformed");
  }
  std::string state_json()const {
    const auto state=projection_.latest();const auto capacity=evidence_.capacity();std::size_t connected=0;
    for(const auto& slot:peers_)if(slot.id)++connected;
    std::ostringstream out;out<<"{\"name\":\""<<configuration_.name<<"\",\"principal\":\""<<configuration_.principal
      <<"\",\"policy_revision\":"<<authorization_->snapshot().revision<<",\"enrolled_peers\":"<<authorization_->snapshot().count
      <<",\"connections\":"<<connected<<",\"committed_observations\":"<<evidence_.event_count()
      <<",\"projected_observations\":"<<state.committed_updates<<",\"projection_revision\":"<<state.revision
      <<",\"resident_events\":"<<capacity.resident_events<<",\"archived_segments\":"<<capacity.archived_segments
      <<",\"ingress_failed\":"<<(ingress_->failed()?"true":"false")<<"}\n";return out.str();
  }
  std::string administer(std::string_view request)override {
    using namespace mesh;
    std::istringstream in{std::string(request)};std::string command,value,extra;std::uint64_t expected=0;
    if(!(in>>command))return "ERROR malformed\n";
    if(command=="STATE"&&!(in>>extra))return state_json();
    if(command=="TRACE"&&!(in>>extra))return trace_.trace_json();
    if(command=="STOP"&&!(in>>extra))return "{\"stopping\":true}\n";
    if(command=="RELOAD"&&!(in>>extra))return authorization_->reload()?"OK reloaded\n":"ERROR persistence\n";
    if(command=="REVOKE"&&(in>>value>>expected)&&!(in>>extra)){
      const auto code=authorization_->revoke(value,expected);return code==EnrollmentCode::ok?"OK revoked\n":"ERROR policy_"+std::to_string(static_cast<unsigned>(code))+'\n';
    }
    if(command=="IMPORT"&&(in>>std::quoted(value)>>expected)&&!(in>>extra)){
      EnrollmentPolicy policy;std::string error;
      if(!files::load_policy(value,policy,error))return "ERROR invalid_policy\n";
      const auto code=authorization_->replace(policy,expected);return code==EnrollmentCode::ok?"OK imported\n":"ERROR policy_"+std::to_string(static_cast<unsigned>(code))+'\n';
    }
    return "ERROR malformed\n";
  }
  bool maintain(){
    namespace db=christeye::living::durable;
    if(ingress_->failed()||!authorization_->ready())return false;
    runtime::WindStoreEpochObservationJournal history(evidence_,configuration_.name);
    mesh::MeshRecoveryService recovery(codec_,*ingress_store_);
    if(!recovery.audit(history))return false;
    if(evidence_.event_count()&&!journal_->checkpoint_now())return false;
    const auto control="CEVMESHCONTROL/1\npolicy_revision "+std::to_string(authorization_->snapshot().revision)+'\n';
    if(!metadata_.save_replay_checkpoint(control)||metadata_.archive_ready()!=db::Code::ok)return false;
    db::ArchiveStats stats;
    return evidence_.collect_objects(stats)==db::Code::ok&&metadata_.collect_objects(stats)==db::Code::ok&&
      artifacts_.collect_objects(stats)==db::Code::ok;
  }
};
} // namespace anaiero::application
