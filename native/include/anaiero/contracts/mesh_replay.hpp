#pragma once
// Passive replay records. AuthorizedPeer is supplied by the authenticated
// gateway after checking the current enrollment on every operation.
#include "anaiero/contracts/mesh_identity.hpp"
#include "anaiero/contracts/runtime_events.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace anaiero::mesh {
inline constexpr std::size_t kMaxReplaySubscriptions=32;
inline constexpr std::size_t kMaxReplayRecords=16;
inline constexpr std::size_t kMaxReplayExamined=64;
inline constexpr std::string_view kMeshZeroDigest="0000000000000000000000000000000000000000000000000000000000000000";
enum class ReplayCode : std::uint8_t {
  ok,invalid,denied,not_found,conflict,capacity,corrupt,persistence_failed
};
struct ReplayCursor {
  std::uint64_t sequence=0;
  std::string hash=std::string(kMeshZeroDigest);
  bool operator==(const ReplayCursor&)const=default;
};
struct ReplaySubscription {
  std::string principal,id;
  Scope scope{};
  bool operator==(const ReplaySubscription&)const=default;
};
struct ReplayRecord {
  ReplayCursor cursor{};
  runtime::Observation observation{};
};
struct ReplayPage {
  ReplayCursor after{},terminal{};
  std::array<ReplayRecord,kMaxReplayRecords> records{};
  std::size_t count=0,examined=0;
  bool caught_up=false;
};
inline bool valid_replay_cursor(const ReplayCursor& cursor)noexcept {
  return mesh_digest(cursor.hash)&&(cursor.sequence!=0||cursor.hash==kMeshZeroDigest);
}
inline bool valid_subscription(const ReplaySubscription& subscription)noexcept {
  return mesh_token(subscription.principal)&&mesh_token(subscription.id)&&valid_scope(subscription.scope);
}
inline bool scope_matches(const Scope& scope,const runtime::Observation& observation)noexcept {
  return scope.source==observation.source&&scope.epoch==observation.source_epoch&&
    scope.clock==observation.clock_domain&&scope.colony==observation.colony;
}
struct IMeshReplayStore {
  virtual ~IMeshReplayStore()=default;
  // Registration is durable and scope is immutable for principal + id.
  virtual ReplayCode open_subscription(const ReplaySubscription&,ReplayCursor&)noexcept=0;
  virtual ReplayCode read_page(const Scope&,const ReplayCursor&,ReplayPage&)noexcept=0;
  virtual ReplayCode acknowledge(const ReplaySubscription&,const ReplayCursor&)noexcept=0;
};
} // namespace anaiero::mesh
