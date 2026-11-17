#pragma once
// Security policy only. Valid packets, hashes and local source claims are
// NOT authentication. Real mesh transport must inject a verified session.
#include "anaiero/contracts/runtime_events.hpp"
#include <cstdint>
namespace anaiero::security {
enum class Capability : std::uint8_t {
  subscribe=1,publish_observation=2,propose_digital_change=3
};
struct IVerifiedSession {
  virtual ~IVerifiedSession()=default;
  virtual bool cryptographically_verified()const noexcept=0;
  virtual bool expired()const noexcept=0;
  virtual std::uint32_t bound_source()const noexcept=0;
  virtual std::uint64_t bound_epoch()const noexcept=0;
  virtual bool allows(Capability capability)const noexcept=0;
};
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
