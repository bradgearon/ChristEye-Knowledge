#pragma once
// Security policy only. Valid packets, hashes and local source claims are
// NOT authentication. Real mesh transport must inject a verified session.
#include "anaiero/contracts/runtime_events.hpp"
#include <cstdint>
#include "anaiero/contracts/verified_session.hpp"
namespace anaiero::security {
class SessionAuthorizationService final {
public:
  bool may_publish(const IVerifiedSession& session,
                   const runtime::Observation& observation)const noexcept {
    return session.cryptographically_verified()&&!session.expired()&&
      session.allows(Capability::publish_observation)&&
      session.bound_source()==observation.source&&
      session.bound_epoch()==observation.source_epoch&&
      observation.source_epoch!=0&&runtime::valid_observation(observation)&&
      observation.origin==runtime::Origin::simulated;
  }
  bool may_subscribe(const IVerifiedSession& session)const noexcept {
    return session.cryptographically_verified()&&!session.expired()&&
      session.allows(Capability::subscribe);
  }
  bool may_change_digital_policy(const IVerifiedSession& session)const noexcept {
    return session.cryptographically_verified()&&!session.expired()&&
      session.allows(Capability::propose_digital_change);
  }
};
} // namespace anaiero::security
