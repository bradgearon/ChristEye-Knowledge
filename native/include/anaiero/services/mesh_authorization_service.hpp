#pragma once
// Single-owner enrollment and authorization decisions. Keys and TLS stay in
// infrastructure; a valid transport identity never grants authority by itself.
#include "anaiero/contracts/mesh_identity.hpp"
#include "anaiero/contracts/mesh_transport.hpp"
#include "anaiero/contracts/runtime_events.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include "anaiero/contracts/verified_session.hpp"
#include <algorithm>
#include <utility>

namespace anaiero::mesh {
class MeshAuthorizationService;
class VerifiedMeshSession final:public security::IVerifiedSession {
  friend class MeshAuthorizationService;
  const MeshAuthorizationService* owner_=nullptr;
  AuthorizedPeer peer_{};
  std::string certificate_;
  std::uint64_t certificate_expiry_=0,policy_epoch_=0;
  Scope bound_{};
public:
  bool cryptographically_verified()const noexcept override;
  bool expired()const noexcept override;
  std::uint32_t bound_source()const noexcept override{return bound_.source;}
  std::uint64_t bound_epoch()const noexcept override{return bound_.epoch;}
  bool allows(security::Capability)const noexcept override;
  const AuthorizedPeer& authorized_peer()const noexcept{return peer_;}
  std::string_view certificate_sha256()const noexcept{return certificate_;}
};
class MeshAuthorizationService final {
  friend class VerifiedMeshSession;
  IEnrollmentStore& store_;
  const IUnixClock& clock_;
  EnrollmentPolicy policy_{};
  std::uint64_t epoch_=0;
  mutable std::uint64_t latest_time_=0;
  bool ready_=false;
  contracts::IServiceExtension* extension_=nullptr;
  std::uint64_t now()const noexcept {
    latest_time_=std::max(latest_time_,clock_.unix_seconds());return latest_time_;
  }
  void invalidate()noexcept {
    ready_=false;if(epoch_!=UINT64_MAX)++epoch_;
  }
  void notify(contracts::ServicePhase phase,EnrollmentCode code)noexcept {
    if(extension_)extension_->on_service_event({contracts::ServiceId::mesh_authorization,
      phase,policy_.revision,0,static_cast<std::int32_t>(code)});
  }
  const Enrollment* find(std::string_view principal)const noexcept {
    for(std::size_t i=0;i<policy_.count;++i)if(policy_.peers[i].principal==principal)return &policy_.peers[i];
    return nullptr;
  }
  static bool owns_exclusive_sources(const EnrollmentPolicy& candidate)noexcept {
    std::array<std::uint32_t,runtime::kMaxSources> sources{};std::size_t count=0;
    for(std::size_t i=0;i<candidate.count;++i){const auto& a=candidate.peers[i];
      if(!a.revoked)for(std::size_t k=0;k<a.publication_count;++k){
        const auto source=a.publications[k].source;std::size_t found=0;
        for(;found<count;++found)if(sources[found]==source)break;
        if(found==count){if(count==sources.size())return false;sources[count++]=source;}
      }
      for(std::size_t j=0;j<i;++j){const auto& b=candidate.peers[j];
        for(std::size_t k=0;k<a.publication_count;++k)for(std::size_t l=0;l<b.publication_count;++l)
          if(a.publications[k].source==b.publications[l].source&&a.publications[k].epoch==b.publications[l].epoch&&
            ((!a.revoked&&!b.revoked)||a.publications[k]!=b.publications[l]))return false;
      }
    }
    return true;
  }
  const Enrollment* current(const VerifiedMeshSession& session)const noexcept {
    if(!ready_||session.owner_!=this||session.policy_epoch_!=epoch_||!session.peer_.generation)return nullptr;
    const auto* enrolled=find(session.peer_.principal);const auto time=now();
    if(!enrolled||enrolled->revoked||enrolled->generation!=session.peer_.generation||
       enrolled->certificate_sha256!=session.certificate_||!time||time>=enrolled->expires_unix_s||
       time>=session.certificate_expiry_)return nullptr;
    return enrolled;
  }
  EnrollmentCode commit(EnrollmentPolicy candidate,std::uint64_t expected)noexcept {
    if(!ready_)return EnrollmentCode::persistence_failed;
    if(expected!=policy_.revision)return EnrollmentCode::conflict;
    if(expected==UINT64_MAX||epoch_==UINT64_MAX)return EnrollmentCode::capacity;
    candidate.revision=expected+1;
    if(!valid_enrollment_policy(candidate)||!owns_exclusive_sources(candidate))return EnrollmentCode::invalid;
    const auto result=store_.compare_exchange(expected,candidate);
    if(result!=EnrollmentStoreCode::ok){
      invalidate();const auto code=result==EnrollmentStoreCode::conflict?EnrollmentCode::conflict:EnrollmentCode::persistence_failed;
      notify(contracts::ServicePhase::rejected,code);return code;
    }
    policy_=std::move(candidate);++epoch_;notify(contracts::ServicePhase::committed,EnrollmentCode::ok);
    return EnrollmentCode::ok;
  }
public:
  MeshAuthorizationService(IEnrollmentStore& store,const IUnixClock& clock)noexcept:store_(store),clock_(clock){}
  MeshAuthorizationService(const MeshAuthorizationService&)=delete;
  MeshAuthorizationService& operator=(const MeshAuthorizationService&)=delete;
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  bool start()noexcept{return reload();}
  bool reload()noexcept {
    invalidate();if(epoch_==UINT64_MAX)return false;
    EnrollmentPolicy candidate{};bool found=false;
    if(!store_.load(candidate,found)||(!found&&candidate!=EnrollmentPolicy{})||
      (found&&!candidate.revision)||!valid_enrollment_policy(candidate)||!owns_exclusive_sources(candidate)){
      notify(contracts::ServicePhase::rejected,EnrollmentCode::persistence_failed);return false;
    }
    policy_=std::move(candidate);ready_=true;notify(contracts::ServicePhase::reviewed,EnrollmentCode::ok);return true;
  }
  bool ready()const noexcept{return ready_;}
  const EnrollmentPolicy& snapshot()const noexcept{return policy_;}
  // Local administrative operation. Wire protocols must never expose this API.
  EnrollmentCode upsert(const Enrollment& record,std::uint64_t expected_revision)noexcept {
    if(!ready_)return EnrollmentCode::persistence_failed;
    if(expected_revision!=policy_.revision)return EnrollmentCode::conflict;
    if(!valid_enrollment(record))return EnrollmentCode::invalid;
    auto next=policy_;std::size_t index=0;
    for(;index<next.count;++index)if(next.peers[index].principal==record.principal)break;
    if(index<next.count){
      if(next.peers[index]==record)return EnrollmentCode::ok;
      if(next.peers[index].generation==UINT64_MAX||record.generation!=next.peers[index].generation+1)return EnrollmentCode::conflict;
    }else {
      if(record.generation!=1)return EnrollmentCode::conflict;
      if(next.count==kMaxMeshPeers)return EnrollmentCode::capacity;
      ++next.count;
    }
    next.peers[index]=record;return commit(std::move(next),expected_revision);
  }
  EnrollmentCode revoke(std::string_view principal,std::uint64_t expected_revision)noexcept {
    if(!ready_)return EnrollmentCode::persistence_failed;
    if(expected_revision!=policy_.revision)return EnrollmentCode::conflict;
    const auto* prior=find(principal);if(!prior)return EnrollmentCode::not_found;
    if(prior->revoked)return EnrollmentCode::ok;
    if(prior->generation==UINT64_MAX)return EnrollmentCode::capacity;
    auto next=*prior;next.revoked=true;++next.generation;return upsert(next,expected_revision);
  }
  EnrollmentCode rotate(std::string_view principal,std::string_view certificate,
    std::uint64_t expires_unix_s,std::uint64_t expected_revision)noexcept {
    if(!ready_)return EnrollmentCode::persistence_failed;
    if(expected_revision!=policy_.revision)return EnrollmentCode::conflict;
    const auto* prior=find(principal);if(!prior)return EnrollmentCode::not_found;
    if(!mesh_digest(certificate)||certificate==prior->certificate_sha256||expires_unix_s<=now())return EnrollmentCode::invalid;
    if(prior->generation==UINT64_MAX)return EnrollmentCode::capacity;
    auto next=*prior;next.certificate_sha256=certificate;next.expires_unix_s=expires_unix_s;
    next.revoked=false;++next.generation;return upsert(next,expected_revision);
  }
  // Full local policy import is CAS-bound. Existing principals cannot disappear;
  // retain a revoked tombstone and advance changed records by exactly one generation.
  EnrollmentCode replace(const EnrollmentPolicy& proposed,std::uint64_t expected_revision)noexcept {
    if(!ready_)return EnrollmentCode::persistence_failed;
    if(expected_revision!=policy_.revision||expected_revision==UINT64_MAX||proposed.revision!=expected_revision+1)return EnrollmentCode::conflict;
    if(!valid_enrollment_policy(proposed))return EnrollmentCode::invalid;
    for(std::size_t i=0;i<policy_.count;++i){const auto& prior=policy_.peers[i];const Enrollment* replacement=nullptr;
      for(std::size_t j=0;j<proposed.count;++j)if(proposed.peers[j].principal==prior.principal)replacement=&proposed.peers[j];
      if(!replacement)return EnrollmentCode::invalid;
      if(*replacement!=prior&&(prior.generation==UINT64_MAX||replacement->generation!=prior.generation+1))return EnrollmentCode::conflict;
    }
    for(std::size_t i=0;i<proposed.count;++i)if(!find(proposed.peers[i].principal)&&proposed.peers[i].generation!=1)return EnrollmentCode::conflict;
    return commit(proposed,expected_revision);
  }
  EnrollmentCode authenticate(const IAuthenticatedPeer& peer,VerifiedMeshSession& out)const noexcept {
    out={};if(!ready_||!peer.verified()||!mesh_digest(peer.certificate_sha256()))return EnrollmentCode::denied;
    const auto time=now();if(!time||peer.certificate_expiry_unix_seconds()<=time)return EnrollmentCode::expired;
    for(std::size_t i=0;i<policy_.count;++i){const auto& record=policy_.peers[i];
      if(record.certificate_sha256!=peer.certificate_sha256())continue;
      if(record.revoked)return EnrollmentCode::denied;
      if(record.expires_unix_s<=time)return EnrollmentCode::expired;
      out.owner_=this;out.peer_={record.principal,record.generation};out.certificate_=record.certificate_sha256;
      out.certificate_expiry_=peer.certificate_expiry_unix_seconds();out.policy_epoch_=epoch_;return EnrollmentCode::ok;
    }
    return EnrollmentCode::denied;
  }
  bool active(const VerifiedMeshSession& session)const noexcept{return current(session)!=nullptr;}
  bool may_publish(const VerifiedMeshSession& session,const runtime::Observation& observation)const noexcept {
    const auto* record=current(session);
    if(!record||!(record->capabilities&capability_bit(MeshCapability::publish))||
       !runtime::valid_observation(observation)||observation.origin!=runtime::Origin::simulated||!observation.source_epoch)return false;
    const Scope scope{observation.source,observation.source_epoch,observation.clock_domain,observation.colony};
    if(valid_scope(session.bound_)&&session.bound_!=scope)return false;
    for(std::size_t i=0;i<record->publication_count;++i)if(record->publications[i]==scope)return true;
    return false;
  }
  bool bind_publish(const VerifiedMeshSession& session,const Scope& scope,VerifiedMeshSession& out)const noexcept {
    const auto* record=current(session);if(!record||!valid_scope(scope)||
      !(record->capabilities&capability_bit(MeshCapability::publish))){out={};return false;}
    for(std::size_t i=0;i<record->publication_count;++i)if(record->publications[i]==scope){out=session;out.bound_=scope;return true;}
    out={};return false;
  }
  bool may_subscribe(const VerifiedMeshSession& session,const Scope& scope)const noexcept {
    const auto* record=current(session);if(!record||!valid_scope(scope)||
      !(record->capabilities&capability_bit(MeshCapability::subscribe)))return false;
    for(std::size_t i=0;i<record->subscription_count;++i)if(record->subscriptions[i]==scope)return true;
    return false;
  }
  bool may_read_artifact(const VerifiedMeshSession& session,std::string_view digest)const noexcept {
    const auto* record=current(session);if(!record||!mesh_digest(digest)||
      !(record->capabilities&capability_bit(MeshCapability::artifact_read)))return false;
    for(std::size_t i=0;i<record->artifact_count;++i)if(record->artifact_sha256[i]==digest)return true;
    return false;
  }
};
inline bool VerifiedMeshSession::cryptographically_verified()const noexcept{return owner_&&owner_->active(*this);}
inline bool VerifiedMeshSession::expired()const noexcept{return !owner_||!owner_->active(*this);}
inline bool VerifiedMeshSession::allows(security::Capability capability)const noexcept {
  if(!owner_)return false;
  const auto* record=owner_->current(*this);if(!record)return false;
  switch(capability){
    case security::Capability::subscribe:return (record->capabilities&capability_bit(MeshCapability::subscribe))!=0;
    case security::Capability::publish_observation:return valid_scope(bound_)&&
      (record->capabilities&capability_bit(MeshCapability::publish))!=0;
    case security::Capability::propose_digital_change:return false;
  }
  return false;
}
} // namespace anaiero::mesh
