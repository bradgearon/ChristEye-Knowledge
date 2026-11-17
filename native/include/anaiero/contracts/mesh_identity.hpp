#pragma once
// Local operator policy records. Transport identity is not scientific evidence.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace anaiero::mesh {
inline constexpr std::size_t kMaxMeshPeers=32;
inline constexpr std::size_t kMaxMeshScopes=8;
inline constexpr std::size_t kMaxMeshArtifacts=16;
inline constexpr std::size_t kMaxMeshPrincipal=64;
struct Scope {
  std::uint32_t source=0;
  std::uint64_t epoch=0;
  std::uint32_t clock=0,colony=0;
  bool operator==(const Scope&)const=default;
};
struct AuthorizedPeer {
  std::string principal;
  std::uint64_t generation=0;
  bool operator==(const AuthorizedPeer&)const=default;
};
enum class MeshCapability : std::uint8_t {publish=1,subscribe=2,artifact_read=4};
inline constexpr std::uint8_t kMeshCapabilityMask=7;
constexpr std::uint8_t capability_bit(MeshCapability capability)noexcept {
  return static_cast<std::uint8_t>(capability);
}
struct Enrollment {
  std::string principal,certificate_sha256;
  std::uint64_t generation=0,expires_unix_s=0;
  bool revoked=false;
  std::uint8_t capabilities=0;
  std::array<Scope,kMaxMeshScopes> publications{},subscriptions{};
  std::size_t publication_count=0,subscription_count=0;
  std::array<std::string,kMaxMeshArtifacts> artifact_sha256{};
  std::size_t artifact_count=0;
  bool operator==(const Enrollment&)const=default;
};
struct EnrollmentPolicy {
  std::uint64_t revision=0;
  std::array<Enrollment,kMaxMeshPeers> peers{};
  std::size_t count=0;
  bool operator==(const EnrollmentPolicy&)const=default;
};
enum class EnrollmentCode : std::uint8_t {ok,invalid,denied,expired,not_found,conflict,capacity,persistence_failed};
enum class EnrollmentStoreCode : std::uint8_t {ok,conflict,failure};
struct IUnixClock {
  virtual ~IUnixClock()=default;
  virtual std::uint64_t unix_seconds()const noexcept=0;
};
struct IEnrollmentStore {
  virtual ~IEnrollmentStore()=default;
  virtual bool load(EnrollmentPolicy&,bool& found)noexcept=0;
  // Proposed policy has revision == expected_revision + 1. Publication is
  // the commit boundary. Failure may be indeterminate; callers fail closed
  // until a successful reload establishes the durable policy.
  virtual EnrollmentStoreCode compare_exchange(std::uint64_t expected_revision,
    const EnrollmentPolicy& proposed)noexcept=0;
};
inline bool mesh_token(std::string_view value)noexcept {
  if(value.empty()||value.size()>kMaxMeshPrincipal)return false;
  for(const char c:value)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
    (c>='0'&&c<='9')||c=='-'||c=='_'))return false;
  return true;
}
inline bool mesh_digest(std::string_view value)noexcept {
  if(value.size()!=64)return false;
  for(const char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
  return true;
}
inline bool valid_scope(const Scope& value)noexcept {
  return value.source&&value.epoch&&value.clock&&value.colony;
}
inline bool valid_enrollment(const Enrollment& value)noexcept {
  if(!mesh_token(value.principal)||!mesh_digest(value.certificate_sha256)||!value.generation||
     !value.expires_unix_s||!value.capabilities||(value.capabilities&~kMeshCapabilityMask)||
     value.publication_count>kMaxMeshScopes||value.subscription_count>kMaxMeshScopes||
     value.artifact_count>kMaxMeshArtifacts)return false;
  if(((value.capabilities&capability_bit(MeshCapability::publish))!=0)!=(value.publication_count!=0)||
     ((value.capabilities&capability_bit(MeshCapability::subscribe))!=0)!=(value.subscription_count!=0)||
     ((value.capabilities&capability_bit(MeshCapability::artifact_read))!=0)!=(value.artifact_count!=0))return false;
  for(std::size_t i=0;i<value.publications.size();++i){
    if(i>=value.publication_count){if(value.publications[i]!=Scope{})return false;continue;}
    if(!valid_scope(value.publications[i]))return false;
    for(std::size_t j=0;j<i;++j)if(value.publications[i].source==value.publications[j].source&&
      value.publications[i].epoch==value.publications[j].epoch)return false;
  }
  for(std::size_t i=0;i<value.subscriptions.size();++i){
    if(i>=value.subscription_count){if(value.subscriptions[i]!=Scope{})return false;continue;}
    if(!valid_scope(value.subscriptions[i]))return false;
    for(std::size_t j=0;j<i;++j)if(value.subscriptions[i].source==value.subscriptions[j].source&&
      value.subscriptions[i].epoch==value.subscriptions[j].epoch)return false;
  }
  for(std::size_t i=0;i<value.artifact_sha256.size();++i){
    if(i>=value.artifact_count){if(!value.artifact_sha256[i].empty())return false;continue;}
    if(!mesh_digest(value.artifact_sha256[i]))return false;
    for(std::size_t j=0;j<i;++j)if(value.artifact_sha256[i]==value.artifact_sha256[j])return false;
  }
  return true;
}
inline bool valid_enrollment_policy(const EnrollmentPolicy& value)noexcept {
  if(value.count>kMaxMeshPeers||(!value.revision&&value.count))return false;
  for(std::size_t i=0;i<value.peers.size();++i){
    if(i>=value.count){if(value.peers[i]!=Enrollment{})return false;continue;}
    if(!valid_enrollment(value.peers[i]))return false;
    for(std::size_t j=0;j<i;++j)if(value.peers[i].principal==value.peers[j].principal||
      value.peers[i].certificate_sha256==value.peers[j].certificate_sha256)return false;
  }
  return true;
}
} // namespace anaiero::mesh
