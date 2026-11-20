#pragma once
#include "anaiero/sdk/types.hpp"
#include <memory>

namespace anaiero::sdk {
enum class Phase : std::uint32_t { ready=0,running,paused,completed,cancelled,faulted };
struct EngineIdentity {
  std::string instance_id,type_id,module_sha256,model_sha256,behavior_id,source_uid;
  std::string clock_domain="logical";
  std::uint64_t epoch=1,tick_ns=0;
  bool operator==(const EngineIdentity&) const = default;
};
struct EngineState {
  std::uint64_t revision=0,tick=0;
  Phase phase=Phase::ready;
  Envelope checkpoint,configuration;
  bool operator==(const EngineState&) const = default;
};
struct Observation {
  std::string schema,metric,quantity,unit;
  std::int64_t value=0;
  std::uint64_t tick=0;
  bool operator==(const Observation&) const = default;
};
struct StepRequest {
  std::string operation_id;
  std::uint64_t expected_revision=0;
  Envelope input;
};
// A migration changes a checkpoint schema under one pinned engine identity.
// It does not authorize replacing the native module or reinterpret old metrics.
struct MigrationRequest {
  std::string operation_id;
  std::uint64_t expected_revision=0;
  std::string source_schema,target_schema;
  Digest implementation_sha256;
  Envelope parameters{"anaiero.migration-parameters/1",{}};
};
struct MigrationPlan {
  EngineIdentity identity;
  MigrationRequest request;
  Digest source_checkpoint_sha256,target_checkpoint_sha256;
};
Bytes encode_migration_request(const MigrationRequest&);
Result decode_migration_request(std::string_view,MigrationRequest&);
Bytes encode_migration_plan(const MigrationPlan&);
Result decode_migration_plan(std::string_view,MigrationPlan&);
Digest migration_request_digest(const EngineIdentity&,const MigrationRequest&);
struct PreparedTransition {
  EngineIdentity identity;
  std::string operation_id,request_sha256,expected_state_sha256;
  std::uint64_t expected_revision=0;
  EngineState target;
  std::vector<Observation> observations;
  std::vector<Envelope> receipts;
};
class IEngine;
struct ITrialCapability {
  virtual ~ITrialCapability()=default;
  virtual Result fork(std::unique_ptr<IEngine>& isolated) const=0;
};
struct IMigrationCapability {
  virtual ~IMigrationCapability()=default;
  // Digest of the actual migration implementation, not a caller assertion.
  virtual std::string_view implementation_sha256() const noexcept=0;
  // Pure preparation; no authoritative changes or durable side effects.
  virtual Result prepare_migration(const MigrationRequest&,PreparedTransition&)=0;
};
struct IChangeCapability {
  virtual ~IChangeCapability()=default;
  virtual Result prepare_change(const StepRequest&,PreparedTransition&)=0;
};
class IEngine {
public:
  virtual ~IEngine()=default;
  virtual const EngineIdentity& identity() const noexcept=0;
  virtual const EngineState& state() const noexcept=0;
  // Prepare must leave visible state unchanged and perform no authoritative I/O.
  virtual Result prepare_step(const StepRequest&,PreparedTransition&)=0;
  virtual Result install(const EngineState& committed)=0;
  virtual Result restore(const EngineState& committed)=0;
  virtual ITrialCapability* trials() noexcept { return nullptr; }
  virtual IChangeCapability* changes() noexcept { return nullptr; }
  virtual IMigrationCapability* migrations() noexcept { return nullptr; }
  virtual void request_cancel() noexcept {}
};
struct EngineDescriptor {
  std::string type_id,display_name,module_sha256,behavior_id;
  std::vector<std::string> configuration_schemas,input_schemas,checkpoint_schemas,
      observation_schemas,change_schemas;
  bool finite=false,isolated_trials=false,change_preparation=false;
  Limits limits;
};
struct IEngineFactory {
  virtual ~IEngineFactory()=default;
  virtual Result create(const EngineIdentity&,const Envelope& configuration,
                        const EngineState* restored,std::unique_ptr<IEngine>& output)=0;
};
bool valid_identity(const EngineIdentity&) noexcept;
bool valid_state(const EngineState&,const Limits& limits={}) noexcept;
Bytes encode_identity(const EngineIdentity&);
Result decode_identity(std::string_view,EngineIdentity&);
Bytes encode_state(const EngineState&);
Result decode_state(std::string_view,EngineState&);
Bytes encode_observation(const Observation&);
Result decode_observation(std::string_view,Observation&);
Bytes encode_transition(const PreparedTransition&);
Result decode_transition(std::string_view,PreparedTransition&);
Digest state_digest(const EngineState&);
// Durable object identity includes source/epoch/model/module provenance. The
// logical state_digest remains independent for transition/replay equivalence.
Bytes encode_checkpoint(const EngineIdentity&,const EngineState&);
Result decode_checkpoint(std::string_view,EngineIdentity&,EngineState&);
Digest checkpoint_digest(const EngineIdentity&,const EngineState&);
Digest request_digest(const EngineIdentity&,const StepRequest&);
Bytes encode_request(const StepRequest&);
Result decode_request(std::string_view,StepRequest&);
Bytes encode_descriptor(const EngineDescriptor&);
Result decode_descriptor(std::string_view,EngineDescriptor&);
std::string_view phase_name(Phase) noexcept;
}
