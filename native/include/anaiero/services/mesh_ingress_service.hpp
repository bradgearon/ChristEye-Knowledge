#pragma once
#include "anaiero/contracts/mesh_ingress.hpp"
#include "anaiero/contracts/epoch_observation.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include "anaiero/services/mesh_authorization_service.hpp"
#include "anaiero/services/session_authorization_service.hpp"
#include "christeye/living/plan_hash.hpp"
#include <array>
#include <span>
namespace anaiero::mesh {
class MeshIngressService final {
  MeshAuthorizationService& authorization_;
  IMeshIngressStore& store_;
  IMeshIngressProjection& projection_;
  const runtime::IObservationEpochCodec& codec_;
  const IUnixClock& clock_;
  contracts::IServiceExtension* extension_=nullptr;
  bool failed_=false;
  void emit(const runtime::Observation& observation,const IngressResult& result)noexcept {
    if(extension_)extension_->on_service_event({contracts::ServiceId::mesh_ingress,
      result.code==ReplayCode::ok?contracts::ServicePhase::committed:contracts::ServicePhase::rejected,
      result.committed.sequence,observation.colony,static_cast<std::int32_t>(result.code),
      0,observation.source_epoch,observation.source});
  }
public:
  MeshIngressService(MeshAuthorizationService& authorization,IMeshIngressStore& store,
    IMeshIngressProjection& projection,const runtime::IObservationEpochCodec& codec,const IUnixClock& clock)noexcept:
    authorization_(authorization),store_(store),projection_(projection),codec_(codec),clock_(clock){}
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  bool failed()const noexcept{return failed_;}
  IngressResult receive(const VerifiedMeshSession& session,const runtime::Observation& observation,
    IngressKind kind=IngressKind::mutual_tls)noexcept {
    IngressResult result;
    const Scope scope{observation.source,observation.source_epoch,observation.clock_domain,observation.colony};
    VerifiedMeshSession bound;
    const security::SessionAuthorizationService common_policy;
    if((kind!=IngressKind::mutual_tls&&kind!=IngressKind::local_import)||
       !authorization_.may_publish(session,observation)||!authorization_.bind_publish(session,scope,bound)||
       !common_policy.may_publish(bound,observation)){
      result.code=ReplayCode::denied;emit(observation,result);return result;
    }
    if(failed_){result.code=ReplayCode::persistence_failed;return result;}
    std::array<std::byte,runtime::kEpochObservationFrame> wire{};
    if(!codec_.encode(observation,wire))return result;
    result.code=store_.check(observation,result.duplicate);
    if(result.code!=ReplayCode::ok){emit(observation,result);return result;}
    if(!result.duplicate&&!projection_.prepare(observation)){
      result.code=ReplayCode::capacity;emit(observation,result);return result;
    }
    result.receipt.peer=session.authorized_peer();
    result.receipt.certificate_sha256=session.certificate_sha256();
    result.receipt.policy_revision=authorization_.snapshot().revision;
    result.receipt.received_unix_s=clock_.unix_seconds();
    result.receipt.scope=scope;result.receipt.source_sequence=observation.sequence;
    result.receipt.kind=kind;
    result.receipt.payload_sha256=christeye::living::plans::sha256_hex(
      std::string_view(reinterpret_cast<const char*>(wire.data()),wire.size()));
    result.code=store_.prepare(result.receipt,result.duplicate);
    if(result.code!=ReplayCode::ok){
      failed_=result.code==ReplayCode::persistence_failed||result.code==ReplayCode::corrupt;
      emit(observation,result);return result;
    }
    result.code=store_.commit(observation,result.committed);
    if(result.code!=ReplayCode::ok){
      // A failed filesystem call can have crossed its durable boundary. Only
      // recovery can establish the committed state before accepting more work.
      failed_=true;emit(observation,result);return result;
    }
    if(!result.duplicate&&!projection_.apply(observation)){
      failed_=true;result.code=ReplayCode::persistence_failed;
    }
    emit(observation,result);return result;
  }
};
} // namespace anaiero::mesh
