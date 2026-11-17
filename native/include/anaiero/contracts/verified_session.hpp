#pragma once
// Remote authenticated session contract. Transport must authenticate identity
// before constructing a trusted implementation of IVerifiedSession.
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
} // namespace anaiero::security
