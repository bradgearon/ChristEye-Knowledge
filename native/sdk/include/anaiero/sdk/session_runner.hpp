#pragma once
#include "anaiero/sdk/runtime.hpp"
#include "anaiero/sdk/evidence.hpp"
namespace anaiero::sdk {
struct EngineSchedule {
  EngineIdentity identity;
  Envelope configuration,input;
  std::uint32_t cadence=1;
};
struct SessionDefinition {std::string name;std::vector<EngineSchedule> engines;};
struct SessionProgress {std::uint64_t completed_cycles=0;bool cycle_pending=false;};
// Persists a complete cycle intent before advancing any engine. A retry retains
// the exact operation IDs, prior revisions and inputs across process restarts.
class SessionRunner {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  SessionRunner(SessionService&,evidence::ICommitStore&);
  ~SessionRunner();
  Result initialize(const SessionDefinition&);
  Result reconfigure(const SessionDefinition&,std::string operation_id);
  Result open();
  const SessionDefinition& definition()const noexcept;
  SessionProgress progress()const noexcept;
  Result begin_cycle();
  Result next_step(std::string& instance,StepRequest&,bool& has_step);
  Result finish_cycle();
};
Bytes encode_session_definition(const SessionDefinition&);
Result decode_session_definition(std::string_view,SessionDefinition&);
}
