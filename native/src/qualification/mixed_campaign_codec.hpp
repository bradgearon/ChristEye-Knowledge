#pragma once
// Exact ANAIERO-MIXED-CAMPAIGN/1 state; no filesystem/host concerns.
#include "anaiero/sdk/evidence.hpp"
namespace anaiero::qualification::campaign {
namespace ev=anaiero::sdk::evidence;
inline constexpr std::uint64_t maximum_attempts=128,maximum_checkpoints=65536;
struct State {
  sdk::Digest profile,session,definition,build,device;
  std::uint64_t attempt=0,serial=0,confirmed_cycles=0,target_cycles=0;
  std::uint64_t active_ns=0,attempt_ns=0,recovered_cycles=0;
  bool active=false,complete=false,pressure=false,control_retry=false,quota=false,restart=false,qualified=false;
  ev::StoredRecord retained;
};
anaiero::sdk::Bytes encode(const State&);
anaiero::sdk::Result decode(const ev::StoredRecord&,State&);
}
