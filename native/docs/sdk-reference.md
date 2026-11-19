# Anaiero installed SDK reference

Generated from the public headers under `native/sdk/include`. The header
hashes identify the exact source bytes. This file has no generation timestamp
or dependency on a Git commit that changes when the generated file is committed.

Regenerate with `python3 tools/sdk-reference/generate_reference.py`; use `--check`
to verify the checked-in output. Python and the parser packages are optional
documentation tools and are not native runtime dependencies.

The symbol tables provide navigation. The complete header blocks are the
authoritative declaration text, including defaults, macros, annotations and
inline definitions. A syntax index is not a substitute for compiling against
the installed SDK. Operational rules are in [modular-lifecycle.md](modular-lifecycle.md).

**Public-header manifest SHA-256:** `a557f5793af9aadd1eff89787aa26876455fbe52cf50632897bc9972de718780`

| Header | SHA-256 |
|---|---|
| [catalog.hpp](../sdk/include/anaiero/sdk/catalog.hpp) | `b6ecea87ac83cdc4f29ed3d2030ba703840cacf944eb9bc9221124c5904bde18` |
| [engine.hpp](../sdk/include/anaiero/sdk/engine.hpp) | `6c7f377106bea1d231d04423e9c302b8088808a0e0608f2361a78c16c217cf16` |
| [engine_files.hpp](../sdk/include/anaiero/sdk/engine_files.hpp) | `c1a311a612733ded9d175472111ecdc4ad0e064ec59e91c68fac58dcf8b593cd` |
| [engines.hpp](../sdk/include/anaiero/sdk/engines.hpp) | `2bf154a15f57beedb85a500ce8f7da6a878c9682d31fdaa52e60dbe6477ed0e4` |
| [evidence.hpp](../sdk/include/anaiero/sdk/evidence.hpp) | `49191ad324dafa1f96b97bbc8e0757f01e63effea2ef657e9d6f0d8327d85957` |
| [experiment_history.hpp](../sdk/include/anaiero/sdk/experiment_history.hpp) | `fab291fe81c41b4a1c9a52c621be741eff70b58cbc26cef3a5d28bb8c05917ca` |
| [experiments.hpp](../sdk/include/anaiero/sdk/experiments.hpp) | `de18d005d734e926c1593f3fed870d4bd01ec9901f2acf5ec4047e3754cd0cec` |
| [host.hpp](../sdk/include/anaiero/sdk/host.hpp) | `05bfbf7ccccf0b4d06168860d8a9af08b4240017ae3765e0faac60d909d7aa81` |
| [lifecycle.hpp](../sdk/include/anaiero/sdk/lifecycle.hpp) | `01ce7b57d5616e309d944b246e1fcb32dc0a5ca299d650a886d1804c9c6ab7eb` |
| [mesh.hpp](../sdk/include/anaiero/sdk/mesh.hpp) | `7a999ffbdc51057887033ab6e300d444313e26ec6a196d260611e46897de1fac` |
| [module_abi.h](../sdk/include/anaiero/sdk/module_abi.h) | `6a5c4c08a9da5873875ca3926d5b8dd9bf6d6cca2bc599856fa594bb31c2a868` |
| [module_host.hpp](../sdk/include/anaiero/sdk/module_host.hpp) | `e17c5ead618314291511e00795f36adc0ebe7135e10f44dfe417be94c6b77f9e` |
| [native_mesh.hpp](../sdk/include/anaiero/sdk/native_mesh.hpp) | `767d3e7deaea31a4105955ce3fa401e118b1bf1e9d955a335ba4409876a14441` |
| [native_session.hpp](../sdk/include/anaiero/sdk/native_session.hpp) | `d3c7fdf4b97e2549504e4dcf44a9b87468c2e29e4f596b0a724a4c88a9de0a0b` |
| [observer.hpp](../sdk/include/anaiero/sdk/observer.hpp) | `956071d349b9e4e82599b055f1109c5e4aacf6beba02da1b723ee0cdff8afe3a` |
| [projection.hpp](../sdk/include/anaiero/sdk/projection.hpp) | `d8304748739cef4086277b2c80615d685d87c2ea5536bd1d0f5d2c1933fbe9ca` |
| [runtime.hpp](../sdk/include/anaiero/sdk/runtime.hpp) | `5130027301e56991dbe3540c839b9f1929f462c5787a30ab061825f02a4744bf` |
| [session_runner.hpp](../sdk/include/anaiero/sdk/session_runner.hpp) | `debc0606bc7a45ac80f5778b3281cdb857e5436973940bfd2eb0b634679093d8` |
| [storage_tools.hpp](../sdk/include/anaiero/sdk/storage_tools.hpp) | `1afc85ef2c9533a1211a16df10042a652cf47575dc02241d5adb6ca011659ed8` |
| [types.hpp](../sdk/include/anaiero/sdk/types.hpp) | `2d4d5d222c41e59ac3ff91dfbe7c388d68d2eb3bdacad77bb3eb8a6f71b202bb` |

## catalog.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::host::NativeSession` | class |
| `anaiero::sdk::EngineModuleCatalog` | class |
| `anaiero::sdk::EngineModuleCatalog::EngineModuleCatalog` | constructor |
| `anaiero::sdk::EngineModuleCatalog::~EngineModuleCatalog` | destructor |
| `anaiero::sdk::EngineModuleCatalog::operator=` | method |
| `anaiero::sdk::EngineModuleCatalog::add` | method |
| `anaiero::sdk::EngineModuleCatalog::freeze` | method |
| `anaiero::sdk::EngineModuleCatalog::frozen` | method |
| `anaiero::sdk::EngineModuleCatalog::size` | method |
| `anaiero::sdk::EngineModuleCatalog::find` | method |
| `anaiero::sdk::EngineModuleCatalog::describe` | method |
| `anaiero::sdk::EngineModuleCatalog::create` | method |

```cpp
#pragma once
#include "anaiero/sdk/engine.hpp"
#include <memory>

namespace anaiero::sdk {
namespace host {class NativeSession;}
class EngineModuleCatalog {
  struct Impl;
  std::unique_ptr<Impl> impl_;
  // Only NativeSession can install an alias after validating the canonical
  // exact-source compatibility binding. General SDK consumers cannot bypass
  // the retained implementation digest through catalog registration.
  friend class host::NativeSession;
  Result allow_retained_module(std::string_view retained,std::string_view executing);
public:
  explicit EngineModuleCatalog(std::size_t capacity=32);
  ~EngineModuleCatalog();
  EngineModuleCatalog(const EngineModuleCatalog&)=delete;
  EngineModuleCatalog& operator=(const EngineModuleCatalog&)=delete;
  Result add(EngineDescriptor,std::shared_ptr<IEngineFactory>);
  Result freeze();
  bool frozen() const noexcept;
  std::size_t size() const noexcept;
  const EngineDescriptor* find(std::string_view type_id) const noexcept;
  std::vector<EngineDescriptor> describe() const;
  Result create(const EngineIdentity&,const Envelope&,const EngineState*,std::unique_ptr<IEngine>&);
};
}
```

## engine.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::Phase` | enum |
| `anaiero::sdk::EngineIdentity` | struct |
| `anaiero::sdk::EngineIdentity::operator==` | method |
| `anaiero::sdk::EngineState` | struct |
| `anaiero::sdk::EngineState::operator==` | method |
| `anaiero::sdk::Observation` | struct |
| `anaiero::sdk::Observation::operator==` | method |
| `anaiero::sdk::StepRequest` | struct |
| `anaiero::sdk::PreparedTransition` | struct |
| `anaiero::sdk::IEngine` | class |
| `anaiero::sdk::ITrialCapability` | struct |
| `anaiero::sdk::ITrialCapability::~ITrialCapability` | destructor |
| `anaiero::sdk::ITrialCapability::fork` | method |
| `anaiero::sdk::IChangeCapability` | struct |
| `anaiero::sdk::IChangeCapability::~IChangeCapability` | destructor |
| `anaiero::sdk::IChangeCapability::prepare_change` | method |
| `anaiero::sdk::IEngine::~IEngine` | destructor |
| `anaiero::sdk::IEngine::identity` | method |
| `anaiero::sdk::IEngine::state` | method |
| `anaiero::sdk::IEngine::prepare_step` | method |
| `anaiero::sdk::IEngine::install` | method |
| `anaiero::sdk::IEngine::restore` | method |
| `anaiero::sdk::IEngine::trials` | method |
| `anaiero::sdk::IEngine::changes` | method |
| `anaiero::sdk::IEngine::request_cancel` | method |
| `anaiero::sdk::EngineDescriptor` | struct |
| `anaiero::sdk::IEngineFactory` | struct |
| `anaiero::sdk::IEngineFactory::~IEngineFactory` | destructor |
| `anaiero::sdk::IEngineFactory::create` | method |
| `anaiero::sdk::valid_identity` | function |
| `anaiero::sdk::valid_state` | function |
| `anaiero::sdk::encode_identity` | function |
| `anaiero::sdk::decode_identity` | function |
| `anaiero::sdk::encode_state` | function |
| `anaiero::sdk::decode_state` | function |
| `anaiero::sdk::encode_observation` | function |
| `anaiero::sdk::decode_observation` | function |
| `anaiero::sdk::encode_transition` | function |
| `anaiero::sdk::decode_transition` | function |
| `anaiero::sdk::state_digest` | function |
| `anaiero::sdk::encode_checkpoint` | function |
| `anaiero::sdk::decode_checkpoint` | function |
| `anaiero::sdk::checkpoint_digest` | function |
| `anaiero::sdk::request_digest` | function |
| `anaiero::sdk::encode_request` | function |
| `anaiero::sdk::decode_request` | function |
| `anaiero::sdk::encode_descriptor` | function |
| `anaiero::sdk::decode_descriptor` | function |
| `anaiero::sdk::phase_name` | function |

```cpp
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
```

## engine_files.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::engines::read_cell_configuration` | function |
| `anaiero::engines::read_colony_configuration` | function |
| `anaiero::engines::read_plan_configuration` | function |
| `anaiero::engines::read_session_definition` | function |
| `anaiero::engines::write_new_artifact` | function |

```cpp
#pragma once
// Optional native file admission adapter. The portable SDK accepts owned bytes.
#include "anaiero/sdk/engines.hpp"
#include "anaiero/sdk/session_runner.hpp"
namespace anaiero::engines {
sdk::Result read_cell_configuration(std::string_view atlas_path,std::string_view world_path,sdk::Envelope&);
sdk::Result read_colony_configuration(std::string_view model_path,sdk::Envelope&);
sdk::Result read_plan_configuration(std::string_view plan_path,std::string_view base_path,
                                    std::string_view asset_root,sdk::Envelope&);
sdk::Result read_session_definition(std::string_view configuration_path,
                                    const sdk::EngineModuleCatalog&,sdk::SessionDefinition&);
// Publish complete bytes without replacing an existing destination. An exact
// existing match is an idempotent retry. A conflicting pending file is kept.
sdk::Result write_new_artifact(std::string_view path,std::string_view bytes);
}
```

## engines.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::engines::RetainedAsset` | struct |
| `anaiero::engines::cell_configuration` | function |
| `anaiero::engines::colony_configuration` | function |
| `anaiero::engines::plan_configuration` | function |
| `anaiero::engines::numerical_configuration` | function |
| `anaiero::engines::register_builtin_engines` | function |
| `anaiero::engines::identify` | function |
| `anaiero::engines::cell_input` | function |
| `anaiero::engines::no_input` | function |
| `anaiero::engines::numerical_input` | function |
| `anaiero::engines::plan_output` | function |

```cpp
#pragma once
// Built-in module admission and data packaging; no concrete model types escape.
#include "anaiero/sdk/catalog.hpp"
#include <span>
namespace anaiero::engines {
inline constexpr std::string_view cell_behavior="anaiero.cell-behavior/1";
inline constexpr std::string_view multi_colony="anaiero.multi-colony/1";
inline constexpr std::string_view vlm_plan="anaiero.vlm-plan/1";
inline constexpr std::string_view passive_membrane="anaiero.passive-membrane/1";
inline constexpr std::string_view environment="anaiero.environment/1";
struct RetainedAsset { std::string relative_path,bytes; };
// Each constructor verifies immutable model/source/asset bytes before admission.
sdk::Result cell_configuration(std::string_view atlas,std::string_view reference,
                               std::string_view initial_world,sdk::Envelope&);
sdk::Result colony_configuration(std::string_view model,
                                 std::span<const RetainedAsset>,sdk::Envelope&);
sdk::Result plan_configuration(std::string_view plan,std::string_view base,
                               std::span<const RetainedAsset>,sdk::Envelope&);
sdk::Result numerical_configuration(std::string_view engine_type,std::string_view model,
                                    std::uint64_t tick_ns,std::int32_t input,sdk::Envelope&);
sdk::Result register_builtin_engines(sdk::EngineModuleCatalog&);
sdk::Result identify(const sdk::EngineModuleCatalog&,std::string_view type,
                     std::string_view instance,std::string_view source,std::uint64_t epoch,
                     const sdk::Envelope&,sdk::EngineIdentity&);
// Canonical textual command schemas for operators and independent clients.
sdk::Envelope cell_input(std::int32_t support,std::int32_t stress,
                         std::int32_t signal,bool lesson);
sdk::Envelope no_input(std::string_view engine_type);
sdk::Envelope numerical_input(std::int32_t input);
// Export the unchanged CEVLM/2 image and original interpreter receipts.
sdk::Result plan_output(const sdk::EngineState&,std::string& image,
                        std::string& receipts_ndjson);
}
```

## evidence.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::evidence::Record` | struct |
| `anaiero::sdk::evidence::StoredRecord` | struct |
| `anaiero::sdk::evidence::RecordGuard` | struct |
| `anaiero::sdk::evidence::Blob` | struct |
| `anaiero::sdk::evidence::ObservationClaim` | struct |
| `anaiero::sdk::evidence::EventKey` | struct |
| `anaiero::sdk::evidence::RegistryEvent` | struct |
| `anaiero::sdk::evidence::StoredEvent` | struct |
| `anaiero::sdk::evidence::AuthorizationGuard` | struct |
| `anaiero::sdk::evidence::CommitClass` | enum |
| `anaiero::sdk::evidence::CommitBatch` | struct |
| `anaiero::sdk::evidence::CommitReceipt` | struct |
| `anaiero::sdk::evidence::BlobInfo` | struct |
| `anaiero::sdk::evidence::StoreInfo` | struct |
| `anaiero::sdk::evidence::AuditReport` | struct |
| `anaiero::sdk::evidence::IndexStatistics` | struct |
| `anaiero::sdk::evidence::CommitTimings` | struct |
| `anaiero::sdk::evidence::IAdmissionClock` | class |
| `anaiero::sdk::evidence::IAdmissionClock::~IAdmissionClock` | destructor |
| `anaiero::sdk::evidence::IAdmissionClock::unix_seconds` | method |
| `anaiero::sdk::evidence::StoreOptions` | struct |
| `anaiero::sdk::evidence::FaultPoint` | enum |
| `anaiero::sdk::evidence::IEvidenceIndex` | class |
| `anaiero::sdk::evidence::IEvidenceIndex::~IEvidenceIndex` | destructor |
| `anaiero::sdk::evidence::IEvidenceIndex::lookup_operation` | method |
| `anaiero::sdk::evidence::IEvidenceIndex::latest` | method |
| `anaiero::sdk::evidence::IEvidenceIndex::scan_latest` | method |
| `anaiero::sdk::evidence::IEvidenceIndex::blob_info` | method |
| `anaiero::sdk::evidence::IEvidenceIndex::list_blobs` | method |
| `anaiero::sdk::evidence::IEvidenceIndex::lookup_observation` | method |
| `anaiero::sdk::evidence::IEvidenceIndex::find_event` | method |
| `anaiero::sdk::evidence::IEvidenceIndex::generation` | method |
| `anaiero::sdk::evidence::ICommitStore` | class |
| `anaiero::sdk::evidence::ICommitStore::~ICommitStore` | destructor |
| `anaiero::sdk::evidence::ICommitStore::commit` | method |
| `anaiero::sdk::evidence::ICommitStore::get_blob` | method |
| `anaiero::sdk::evidence::ICommitStore::reclaim_expired_blob` | method |
| `anaiero::sdk::evidence::ICommitStore::repair_index` | method |
| `anaiero::sdk::evidence::ICommitStore::audit` | method |
| `anaiero::sdk::evidence::ICommitStore::info` | method |
| `anaiero::sdk::evidence::open_native_store` | function |
| `anaiero::sdk::evidence::set_native_store_fault` | function |
| `anaiero::sdk::evidence::native_index_statistics` | function |
| `anaiero::sdk::evidence::reset_native_index_statistics` | function |
| `anaiero::sdk::evidence::native_commit_timings` | function |
| `anaiero::sdk::evidence::record_digest` | function |
| `anaiero::sdk::evidence::batch_digest` | function |
| `anaiero::sdk::evidence::CommitCompletion` | struct |
| `anaiero::sdk::evidence::ICommitQueue` | class |
| `anaiero::sdk::evidence::ICommitQueue::~ICommitQueue` | destructor |
| `anaiero::sdk::evidence::ICommitQueue::submit` | method |
| `anaiero::sdk::evidence::ICommitQueue::try_complete` | method |
| `anaiero::sdk::evidence::CommitWorker` | class |
| `anaiero::sdk::evidence::CommitWorker::CommitWorker` | constructor |
| `anaiero::sdk::evidence::CommitWorker::~CommitWorker` | destructor |
| `anaiero::sdk::evidence::CommitWorker::operator=` | method |
| `anaiero::sdk::evidence::CommitWorker::submit` | method |
| `anaiero::sdk::evidence::CommitWorker::try_complete` | method |
| `anaiero::sdk::evidence::CommitWorker::wait_complete` | method |
| `anaiero::sdk::evidence::CommitWorker::cached_info` | method |
| `anaiero::sdk::evidence::CommitWorker::set_wakeup` | method |
| `anaiero::sdk::evidence::CommitWorker::stop` | method |

```cpp
#pragma once
#include "anaiero/sdk/types.hpp"
#include <functional>
#include <memory>
#include <limits>

namespace anaiero::sdk::evidence {
inline constexpr std::uint64_t any_generation=std::numeric_limits<std::uint64_t>::max();
struct Record {
  std::string kind,key;
  Bytes payload;
  std::vector<Digest> references;
  bool tombstone=false;
};
struct StoredRecord {
  Record record;
  std::uint64_t revision=0,generation=0;
  Digest digest;
};
struct RecordGuard {
  std::string kind,key;
  // Zero requires a never-created key. Tombstones retain their nonzero revision.
  std::uint64_t expected_revision=0;
  Digest expected_digest; // Empty omits the additional digest comparison.
};
struct Blob {
  Digest digest;
  Bytes bytes;
  std::vector<Digest> references;
};
struct ObservationClaim {
  std::string source_uid;
  std::uint64_t epoch=0,sequence=0;
  Digest payload_digest,receipt_digest;
};
struct EventKey {
  std::string kind,entity,clock,producer;
  std::uint64_t source_tick=0;
};
struct RegistryEvent {
  EventKey key;
  Digest object_digest;
  std::string origin;
};
struct StoredEvent {
  RegistryEvent event;
  std::uint64_t generation=0,position=0;
  Digest digest;
};
struct AuthorizationGuard {
  std::string principal,scope;
  std::uint64_t credential_generation=0,not_after_unix_s=0;
};
enum class CommitClass : std::uint32_t {ordinary=0,maintenance=1};
struct CommitBatch {
  std::string operation_id;
  Digest request_digest;
  std::uint64_t expected_generation=any_generation;
  std::vector<RecordGuard> guards;
  std::vector<Record> records;
  std::vector<Blob> blobs;
  std::vector<ObservationClaim> observations;
  std::vector<RegistryEvent> events;
  // Explicit canonical expiry. Protected latest-record/blob references reject
  // expiry. Physical reclamation is a separate recoverable operation.
  std::vector<Digest> expire_blobs;
  // Revision authority is enforced by RecordGuard. These bounded claims add
  // expiry at the serialized admission boundary; zero deadline is unbounded.
  std::vector<AuthorizationGuard> authorizations;
  CommitClass commit_class=CommitClass::ordinary;
};
struct CommitReceipt {
  std::string operation_id;
  Digest request_digest,batch_digest,root_digest;
  std::uint64_t generation=0;
  bool duplicate=false;
  std::uint64_t admitted_unix_s=0;
  std::vector<AuthorizationGuard> authorizations={};
  CommitClass commit_class=CommitClass::ordinary;
};
struct BlobInfo {
  Digest digest;
  std::uint64_t ordinal=0,bytes=0,created_generation=0,expired_generation=0;
  bool present=false;
  std::vector<Digest> references;
};
struct StoreInfo {
  std::string store_id,index_backend="native-sealed-avl";
  std::uint64_t generation=0,index_generation=0,blob_count=0,record_count=0;
  // Accounted serialized transaction/payload/index bytes plus fixed allowance,
  // not allocated filesystem blocks. An unreadable index saturates this value
  // to maximum_bytes. Filesystem free space is checked separately at admission.
  std::uint64_t physical_bytes=0,maximum_bytes=0;
  std::size_t maximum_batch_bytes=16U*1024U*1024U;
  std::uint64_t maintenance_reserve_bytes=0,ordinary_limit_bytes=0;
  Digest root_digest,index_root;
  bool writable=false,index_ready=false;
  // Canonical bytes before this generation were replaced by a verified base.
  // Exact operation receipts are retained unless an explicit compaction rotates
  // their namespace. Unknown older-epoch operations then fail HISTORY_EXPIRED.
  std::uint64_t canonical_base_generation=0,operation_epoch=0;
  // Last durably admitted native UTC sample; persists through compaction.
  std::uint64_t admitted_unix_s=0;
  Digest canonical_base_digest;
  // Conservative obsolete-file upper bound until durable cleanup completes.
  std::uint64_t pending_reclamation_bytes=0;
};
struct AuditReport {
  std::uint64_t generations=0,records=0,blobs=0,expired_blobs=0;
  std::uint64_t index_nodes_read=0,canonical_records_read=0;
  std::uint64_t summarized_generations=0,summarized_record_versions=0;
  std::uint64_t expired_observations=0,base_pages=0;
  Digest canonical_base_digest;
};
struct IndexStatistics {
  std::uint64_t nodes_read=0,nodes_written=0,point_queries=0,range_queries=0;
  std::uint64_t staged_nodes=0,staged_spills=0,transient_nodes_elided=0;
  std::uint64_t cache_peak_nodes=0,cache_peak_bytes=0;
  std::uint64_t admission_cache_nodes_reused=0;
};
// Optional, bounded measurements of the most recent native commit attempt.
// Stages are disjoint monotonic durations. They describe software publication,
// not a device power-loss guarantee. Failed/retried attempts remain explicit.
struct CommitTimings {
  std::uint64_t lock_wait_ns=0,validation_ns=0,staging_ns=0,blobs_ns=0;
  std::uint64_t transaction_ns=0,root_ns=0,index_ns=0,cleanup_ns=0,total_ns=0;
  std::uint64_t generation=0;
  bool enabled=false,committed=false,duplicate=false;
  Code result=Code::ok;
};
class IAdmissionClock {
public:
  virtual ~IAdmissionClock()=default;
  virtual std::uint64_t unix_seconds() const noexcept=0;
};
struct StoreOptions {
  std::uint64_t maximum_bytes=1024ULL*1024ULL*1024ULL;
  std::uint64_t minimum_free_bytes=1024ULL*1024ULL;
  std::size_t maximum_batch_bytes=16U*1024U*1024U;
  std::size_t maximum_blob_bytes=8U*1024U*1024U;
  std::size_t maximum_records=128,maximum_blobs=64,maximum_references=256;
  std::size_t maximum_index_updates=1024;
  // Trusted host clock, borrowed for the store's lifetime. Null uses native UTC.
  const IAdmissionClock* admission_clock=nullptr;
  // Ordinary admission leaves min(this, maximum_bytes/8) for explicit bounded
  // maintenance. Maintenance itself still obeys the absolute maximum_bytes.
  std::uint64_t maintenance_reserve_bytes=16U*1024U*1024U;
  // No callbacks or per-commit allocation; false avoids reading the clock.
  bool measure_commit_stages=false;
  // POSIX filesystems with a finite reported file-node pool retain this reserve
  // in addition to the operation's bounded temporary-file estimate. Native
  // Windows filesystems do not expose an equivalent pool through this adapter.
  std::uint64_t minimum_free_file_nodes=128;
};
enum class FaultPoint : std::uint32_t {
  none,after_objects,after_transaction,after_root,before_index,after_index,
  compaction_after_base,compaction_after_root,compaction_during_cleanup
};
// Typed indexed reads are independent of the canonical writer and blob I/O.
// A missing/corrupt index cannot be interpreted as evidence of absence.
class IEvidenceIndex {
public:
  virtual ~IEvidenceIndex()=default;
  virtual Result lookup_operation(std::string_view,CommitReceipt&)=0;
  virtual Result latest(std::string_view kind,std::string_view key,StoredRecord&)=0;
  virtual Result scan_latest(std::string_view kind,std::string_view prefix,
      std::string_view after,std::size_t limit,std::vector<StoredRecord>&,bool& more)=0;
  virtual Result blob_info(std::string_view digest,BlobInfo&)=0;
  virtual Result list_blobs(std::uint64_t after,std::size_t limit,
      std::vector<BlobInfo>&,bool& more)=0;
  virtual Result lookup_observation(std::string_view source_uid,std::uint64_t epoch,
      std::uint64_t sequence,ObservationClaim&)=0;
  virtual Result find_event(const EventKey&,StoredEvent&,
      std::string_view exact_object={},std::string_view exact_origin={})=0;
  virtual std::uint64_t generation() const noexcept=0;
};
class ICommitStore:public IEvidenceIndex {
public:
  ~ICommitStore() override=default;
  virtual Result commit(const CommitBatch&,CommitReceipt&)=0;
  virtual Result get_blob(std::string_view digest,Bytes&)=0;
  virtual Result reclaim_expired_blob(std::string_view digest,
      std::uint64_t expected_generation)=0;
  virtual Result repair_index()=0;
  virtual Result audit(AuditReport&)=0;
  virtual StoreInfo info() const=0;
};
// Implemented in the native storage module; no OS/path type enters the SDK.
Result open_native_store(std::string_view directory,const StoreOptions&,
                         std::unique_ptr<ICommitStore>&);
void set_native_store_fault(ICommitStore&,FaultPoint) noexcept;
IndexStatistics native_index_statistics(const ICommitStore&) noexcept;
void reset_native_index_statistics(ICommitStore&) noexcept;
CommitTimings native_commit_timings(const ICommitStore&) noexcept;
Digest record_digest(const Record&);
Digest batch_digest(const CommitBatch&);

struct CommitCompletion {
  std::uint64_t ticket=0;Result result;CommitReceipt receipt;
  // Process-local steady-clock timestamps; never persisted as event time.
  std::uint64_t started_monotonic_ns=0,completed_monotonic_ns=0;
};
class ICommitQueue {
public:
  virtual ~ICommitQueue()=default;
  virtual Result submit(CommitBatch,std::uint64_t& ticket)=0;
  virtual bool try_complete(CommitCompletion&)=0;
};
class CommitWorker final:public ICommitQueue {
public:
  explicit CommitWorker(std::unique_ptr<ICommitStore>,std::size_t capacity=8);
  explicit CommitWorker(ICommitStore&,std::size_t capacity=8);
  ~CommitWorker();
  CommitWorker(const CommitWorker&)=delete;
  CommitWorker& operator=(const CommitWorker&)=delete;
  // Admission reserves completion capacity as well. The wakeup is a signal-only
  // callback on the storage thread; domain completion runs on the consumer.
  Result submit(CommitBatch,std::uint64_t& ticket) override;
  bool try_complete(CommitCompletion&) override;
  bool wait_complete(CommitCompletion&,std::uint32_t timeout_ms);
  StoreInfo cached_info() const;
  void set_wakeup(std::function<void()>);
  void stop();
private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}
```

## experiment_history.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::ExperimentSpecification` | struct |
| `anaiero::sdk::RecordedExperiment` | struct |
| `anaiero::sdk::ExperimentHistoryService` | class |
| `anaiero::sdk::ExperimentHistoryService::ExperimentHistoryService` | constructor |
| `anaiero::sdk::ExperimentHistoryService::select` | method |
| `anaiero::sdk::ExperimentHistoryService::load` | method |
| `anaiero::sdk::ExperimentHistoryService::apply_selected` | method |
| `anaiero::sdk::ExperimentHistoryService::review` | method |
| `anaiero::sdk::ExperimentHistoryService::release` | method |

```cpp
#pragma once
#include "anaiero/sdk/experiments.hpp"
#include "anaiero/sdk/runtime.hpp"
#include "anaiero/sdk/evidence.hpp"
namespace anaiero::sdk {
struct ExperimentSpecification {
  std::string id;
  std::vector<Envelope> inputs;
  std::vector<TrialCandidate> candidates;
};
struct RecordedExperiment {
  ExperimentSpecification specification;
  EngineIdentity source;
  EngineState source_state;
  ScoreStrategyIdentity strategy;
  bool completed=false;
  ExperimentResult result;
};
// Serial owner service. The source, input program and strategy are committed
// before isolated work starts. An interrupted run resumes from that frozen
// source; pure bounded trials may be recomputed, with one durable final result.
class ExperimentHistoryService {
  EngineModuleCatalog& catalog_;
  evidence::ICommitStore& store_;
public:
  ExperimentHistoryService(EngineModuleCatalog& catalog,evidence::ICommitStore& store):catalog_(catalog),store_(store){}
  Result select(IEngine&,const ExperimentSpecification&,const IScoreStrategy&,RecordedExperiment&);
  Result load(std::string_view id,RecordedExperiment&);
  // Application is explicit and requires the original live source revision.
  Result apply_selected(std::string_view id,std::string operation_id,SessionService&);
  // One frozen measured review per experiment, from canonical engine evidence.
  Result review(std::string_view id,std::string operation_id,const IScoreStrategy&,OutcomeReview&);
  // Drops live object roots. Audit identities remain; experiment IDs are never reused.
  Result release(std::string_view id,std::string operation_id);
};
}
```

## experiments.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::TrialProgram` | struct |
| `anaiero::sdk::TrialResult` | struct |
| `anaiero::sdk::TrialCandidate` | struct |
| `anaiero::sdk::ExperimentRequest` | struct |
| `anaiero::sdk::MetricComparison` | struct |
| `anaiero::sdk::Score` | struct |
| `anaiero::sdk::ScoreStrategyIdentity` | struct |
| `anaiero::sdk::IScoreStrategy` | struct |
| `anaiero::sdk::IScoreStrategy::~IScoreStrategy` | destructor |
| `anaiero::sdk::IScoreStrategy::identity` | method |
| `anaiero::sdk::IScoreStrategy::score` | method |
| `anaiero::sdk::CandidateResult` | struct |
| `anaiero::sdk::ExperimentResult` | struct |
| `anaiero::sdk::OutcomeEvidence` | struct |
| `anaiero::sdk::OutcomeReview` | struct |
| `anaiero::sdk::ExperimentService` | class |
| `anaiero::sdk::ExperimentService::set_observer_sink` | method |
| `anaiero::sdk::ExperimentService::evaluate` | method |
| `anaiero::sdk::ExperimentService::replay_equivalence` | method |
| `anaiero::sdk::ExperimentService::select` | method |
| `anaiero::sdk::ExperimentService::compare_outcome` | method |

```cpp
#pragma once
#include "anaiero/sdk/engine.hpp"
#include "anaiero/sdk/observer.hpp"
namespace anaiero::sdk {
struct TrialProgram { std::string id;std::vector<Envelope> inputs;Envelope change;bool has_change=false; };
struct TrialResult { std::string id;EngineState final_state;std::vector<Observation> observations;Digest receipt_tip; };
struct TrialCandidate {std::string id;Envelope change;};
struct ExperimentRequest {
  std::string id;
  std::uint64_t expected_revision=0;
  Digest expected_state_sha256;
  std::vector<Envelope> inputs;
  std::vector<TrialCandidate> candidates;
};
struct MetricComparison {Observation baseline,candidate;};
struct Score {std::int64_t value=0;bool eligible=true;std::string rationale;};
struct ScoreStrategyIdentity {std::string id;Digest implementation_sha256;Envelope configuration;};
struct IScoreStrategy {
  virtual ~IScoreStrategy()=default;
  virtual ScoreStrategyIdentity identity()const=0;
  // A higher score is preferred. Every pair has the same metric, schema,
  // quantity, unit and logical tick; strategies retain domain-specific meaning.
  virtual Result score(const std::vector<MetricComparison>&,Score&)const=0;
};
struct CandidateResult {std::string id;Result status;TrialResult trial;Score score;};
struct ExperimentResult {
  EngineIdentity source;
  std::uint64_t source_revision=0;
  Digest source_state_sha256,program_sha256;
  ScoreStrategyIdentity strategy;
  TrialResult baseline;
  Score baseline_score;
  std::vector<CandidateResult> candidates;
  std::string chosen_candidate; // Empty means preserve the baseline.
};
struct OutcomeEvidence {
  EngineIdentity identity;
  EngineState state;
  std::vector<Observation> observations;
  Digest receipt_tip;
};
struct OutcomeReview {
  Score measured_score;
  bool meets_selected_score=false,exact_selected_state=false;
  Digest expected_state_sha256,observed_state_sha256,observed_receipt_tip;
};
class ExperimentService {
  IObserverSink* observer_=nullptr;
public:
  void set_observer_sink(IObserverSink* observer)noexcept{observer_=observer;}
  // Same module interpreter, independent engine; never mutates the live source.
  Result evaluate(IEngine& source,const TrialProgram&,TrialResult&) const;
  Result replay_equivalence(IEngine& source,const TrialProgram&,const EngineState& expected,TrialResult&) const;
  Result select(IEngine& source,const ExperimentRequest&,const IScoreStrategy&,ExperimentResult&) const;
  Result compare_outcome(const ExperimentResult&,const OutcomeEvidence&,const IScoreStrategy&,OutcomeReview&) const;
};
}
```

## host.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::host::StoreTask` | alias |
| `anaiero::sdk::host::StoreTaskCompletion` | alias |
| `anaiero::sdk::host::IStoreTaskExecutor` | class |
| `anaiero::sdk::host::IStoreTaskExecutor::~IStoreTaskExecutor` | destructor |
| `anaiero::sdk::host::IStoreTaskExecutor::submit` | method |
| `anaiero::sdk::host::TimingKind` | enum |
| `anaiero::sdk::host::TimingObservation` | struct |
| `anaiero::sdk::host::ITimingObserver` | class |
| `anaiero::sdk::host::ITimingObserver::~ITimingObserver` | destructor |
| `anaiero::sdk::host::ITimingObserver::observe` | method |
| `anaiero::sdk::host::Options` | struct |
| `anaiero::sdk::host::CommandKind` | enum |
| `anaiero::sdk::host::Command` | struct |
| `anaiero::sdk::host::CommandResult` | struct |
| `anaiero::sdk::host::EngineStatus` | struct |
| `anaiero::sdk::host::Status` | struct |
| `anaiero::sdk::host::NativeContinuousHost` | class |
| `anaiero::sdk::host::NativeContinuousHost::NativeContinuousHost` | constructor |
| `anaiero::sdk::host::NativeContinuousHost::~NativeContinuousHost` | destructor |
| `anaiero::sdk::host::NativeContinuousHost::operator=` | method |
| `anaiero::sdk::host::NativeContinuousHost::start` | method |
| `anaiero::sdk::host::NativeContinuousHost::enqueue` | method |
| `anaiero::sdk::host::NativeContinuousHost::submit` | method |
| `anaiero::sdk::host::NativeContinuousHost::snapshot` | method |
| `anaiero::sdk::host::NativeContinuousHost::command_result` | method |
| `anaiero::sdk::host::NativeContinuousHost::set_commit_wakeup` | method |
| `anaiero::sdk::host::NativeContinuousHost::request_stop` | method |
| `anaiero::sdk::host::NativeContinuousHost::shutdown` | method |
| `anaiero::sdk::host::encode_status` | function |

```cpp
#pragma once
#include "anaiero/sdk/session_runner.hpp"
#include <functional>
#include <memory>

namespace anaiero::sdk::host {
using StoreTask=std::function<Result(evidence::ICommitStore&)>;
using StoreTaskCompletion=std::function<void(Result)>;
// Trusted adapters submit bounded work, never arbitrary remote code. Accepted
// work owns its captures and completes exactly once. Success here means queued,
// not durable; only the completion carries the canonical operation result.
// Completion signals its consumer on the task worker; it must not wait for
// another host operation, call shutdown, or access the store again.
class IStoreTaskExecutor {
public:
  virtual ~IStoreTaskExecutor()=default;
  virtual Result submit(StoreTask,StoreTaskCompletion)=0;
};
enum class TimingKind : std::uint32_t {step_dispatch,step_queue_wait,step_commit_completion,step_projection};
struct TimingObservation {
  TimingKind kind=TimingKind::step_dispatch;
  std::uint64_t cycle=0,elapsed_ns=0;
  Code result=Code::ok;
};
// Borrowed, bounded observation port invoked only on the host owner. It must
// return immediately, never reenter the host/store or perform blocking I/O.
// Dispatch spans preparation/submission. Queue wait spans submission through
// writer entry. Commit spans actual writer execution, without owner wake delay.
// Projection spans successful completion adoption by SessionService::pump.
// These do not measure synchronous cycle-boundary/control commits or TLS ACKs.
class ITimingObserver {
public:
  virtual ~ITimingObserver()=default;
  virtual void observe(const TimingObservation&) noexcept=0;
};
struct Options {
  // Wall monotonic pacing is independent of each engine's logical clock.
  std::uint32_t period_ms=100;
  std::size_t command_capacity=32,task_capacity=16;
  bool initially_running=true;
  ITimingObserver* timing_observer=nullptr;
};
enum class CommandKind : std::uint32_t {run,pause,through,engine_pause,engine_resume,engine_cancel};
struct Command {
  CommandKind kind=CommandKind::pause;
  std::string instance_id,operation_id;
  std::uint64_t through_cycle=0;
};
struct CommandResult {
  std::uint64_t ticket=0;
  bool complete=false,durable=false;
  Result result;
};
struct EngineStatus {
  EngineIdentity identity;
  Phase phase=Phase::ready;
  std::uint64_t tick=0,revision=0;
  Digest state_sha256;
};
struct Status {
  std::uint64_t revision=0,completed_cycles=0,through_cycle=0;
  std::uint64_t owner_wakeups=0,completed_tasks=0;
  std::size_t queued_commands=0,queued_tasks=0;
  bool running=false,busy=false,cycle_pending=false,stopping=false,stopped=false;
  Result result;
  evidence::StoreInfo store;
  std::vector<EngineStatus> engines;
};
// Native threading adapter around one SessionService/SessionRunner owner.
// Borrowed services must outlive this object and may not be called concurrently.
// Storage tasks are serialized with engine commits, on a separate bounded lane.
// Cached status/control admission remain available while either lane is busy.
class NativeContinuousHost final:public IStoreTaskExecutor {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  NativeContinuousHost(SessionService&,SessionRunner&,evidence::ICommitStore&,Options={});
  ~NativeContinuousHost();
  NativeContinuousHost(const NativeContinuousHost&)=delete;
  NativeContinuousHost& operator=(const NativeContinuousHost&)=delete;
  Result start();
  Result enqueue(Command,std::uint64_t& ticket);
  Result submit(StoreTask,StoreTaskCompletion) override;
  Status snapshot(std::uint64_t after_revision=0,std::uint32_t wait_ms=0) const;
  Result command_result(std::uint64_t ticket,CommandResult&) const;
  // Signal-only callback, never reenter the host or touch the borrowed store.
  void set_commit_wakeup(std::function<void(std::uint64_t)>);
  void request_stop() noexcept;
  void shutdown() noexcept;
};
// Bounded IPC presentation; engine checkpoints and payloads are never copied
// into this cached response. Source/engine details can be paginated by offset.
Bytes encode_status(const Status&,std::size_t engine_offset=0,std::size_t engine_limit=8);
}
```

## lifecycle.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::lifecycle::SourcePhase` | enum |
| `anaiero::sdk::lifecycle::PeerPhase` | enum |
| `anaiero::sdk::lifecycle::SubscriptionPhase` | enum |
| `anaiero::sdk::lifecycle::PinKind` | enum |
| `anaiero::sdk::lifecycle::BootstrapContinuationPhase` | enum |
| `anaiero::sdk::lifecycle::HistoryAvailability` | enum |
| `anaiero::sdk::lifecycle::MaintenancePhase` | enum |
| `anaiero::sdk::lifecycle::SourceAuthorityKind` | enum |
| `anaiero::sdk::lifecycle::SourceAuthority` | struct |
| `anaiero::sdk::lifecycle::Options` | struct |
| `anaiero::sdk::lifecycle::SourceRef` | struct |
| `anaiero::sdk::lifecycle::SourceRef::operator==` | method |
| `anaiero::sdk::lifecycle::Source` | struct |
| `anaiero::sdk::lifecycle::Cursor` | struct |
| `anaiero::sdk::lifecycle::Cursor::operator==` | method |
| `anaiero::sdk::lifecycle::encode_source_observation` | function |
| `anaiero::sdk::lifecycle::decode_source_observation` | function |
| `anaiero::sdk::lifecycle::Metric` | struct |
| `anaiero::sdk::lifecycle::Snapshot` | struct |
| `anaiero::sdk::lifecycle::CapacityStatus` | struct |
| `anaiero::sdk::lifecycle::Peer` | struct |
| `anaiero::sdk::lifecycle::Subscription` | struct |
| `anaiero::sdk::lifecycle::Pin` | struct |
| `anaiero::sdk::lifecycle::MaintenanceRequest` | struct |
| `anaiero::sdk::lifecycle::MaintenancePlan` | struct |
| `anaiero::sdk::lifecycle::Bootstrap` | struct |
| `anaiero::sdk::lifecycle::BootstrapContinuation` | struct |
| `anaiero::sdk::lifecycle::ArchiveOptions` | struct |
| `anaiero::sdk::lifecycle::IArchiveStore` | struct |
| `anaiero::sdk::lifecycle::IArchiveStore::~IArchiveStore` | destructor |
| `anaiero::sdk::lifecycle::IArchiveStore::identifier` | method |
| `anaiero::sdk::lifecycle::IArchiveStore::put` | method |
| `anaiero::sdk::lifecycle::IArchiveStore::get` | method |
| `anaiero::sdk::lifecycle::open_native_archive` | function |
| `anaiero::sdk::lifecycle::RestoreReport` | struct |
| `anaiero::sdk::lifecycle::RestoreOptions` | struct |
| `anaiero::sdk::lifecycle::restore_quarantined_store` | function |
| `anaiero::sdk::lifecycle::Service` | class |
| `anaiero::sdk::lifecycle::Service::Service` | constructor |
| `anaiero::sdk::lifecycle::Service::~Service` | destructor |
| `anaiero::sdk::lifecycle::Service::operator=` | method |
| `anaiero::sdk::lifecycle::Service::open` | method |
| `anaiero::sdk::lifecycle::Service::snapshot` | method |
| `anaiero::sdk::lifecycle::Service::capacity` | method |
| `anaiero::sdk::lifecycle::Service::source` | method |
| `anaiero::sdk::lifecycle::Service::source_authority` | method |
| `anaiero::sdk::lifecycle::Service::stage_source_authority` | method |
| `anaiero::sdk::lifecycle::Service::register_engine` | method |
| `anaiero::sdk::lifecycle::Service::activate` | method |
| `anaiero::sdk::lifecycle::Service::begin_drain` | method |
| `anaiero::sdk::lifecycle::Service::seal` | method |
| `anaiero::sdk::lifecycle::Service::retire` | method |
| `anaiero::sdk::lifecycle::Service::stage_commit` | method |
| `anaiero::sdk::lifecycle::Service::upsert_peer` | method |
| `anaiero::sdk::lifecycle::Service::peer` | method |
| `anaiero::sdk::lifecycle::Service::open_subscription` | method |
| `anaiero::sdk::lifecycle::Service::subscription` | method |
| `anaiero::sdk::lifecycle::Service::offer` | method |
| `anaiero::sdk::lifecycle::Service::acknowledge` | method |
| `anaiero::sdk::lifecycle::Service::close_subscription` | method |
| `anaiero::sdk::lifecycle::Service::expire_subscriptions` | method |
| `anaiero::sdk::lifecycle::Service::expire_peer_leases` | method |
| `anaiero::sdk::lifecycle::Service::put_pin` | method |
| `anaiero::sdk::lifecycle::Service::release_pin` | method |
| `anaiero::sdk::lifecycle::Service::pin` | method |
| `anaiero::sdk::lifecycle::Service::review_maintenance` | method |
| `anaiero::sdk::lifecycle::Service::maintenance` | method |
| `anaiero::sdk::lifecycle::Service::activate_maintenance` | method |
| `anaiero::sdk::lifecycle::Service::execute_maintenance` | method |
| `anaiero::sdk::lifecycle::Service::replay_status` | method |
| `anaiero::sdk::lifecycle::Service::accept_bootstrap` | method |
| `anaiero::sdk::lifecycle::Service::bootstrap_continuation` | method |
| `anaiero::sdk::lifecycle::Service::pending_bootstrap` | method |
| `anaiero::sdk::lifecycle::Service::stage_bootstrap_continuation` | method |
| `anaiero::sdk::lifecycle::Service::stage_accept_bootstrap_continuation` | method |
| `anaiero::sdk::lifecycle::Service::stage_cancel_bootstrap_continuation` | method |
| `anaiero::sdk::lifecycle::Service::stage_revoke_bootstrap_scope` | method |
| `anaiero::sdk::lifecycle::Service::installed_bootstrap` | method |
| `anaiero::sdk::lifecycle::Service::quarantine_restore` | method |
| `anaiero::sdk::lifecycle::Service::reauthorize_restore` | method |
| `anaiero::sdk::lifecycle::Service::reauthorization_sources` | method |
| `anaiero::sdk::lifecycle::phase_name` | function |

```cpp
#pragma once
#include "anaiero/sdk/engine.hpp"
#include "anaiero/sdk/evidence.hpp"
#include <memory>

namespace anaiero::sdk::lifecycle {
enum class SourcePhase : std::uint32_t { registered=0,active,draining,sealed,retired };
enum class PeerPhase : std::uint32_t { active=0,suspended,revoked };
enum class SubscriptionPhase : std::uint32_t { active=0,suspended,expired,closed };
enum class PinKind : std::uint32_t { recovery=0,program,module,operation,reader,backup,operator_pin,bootstrap_continuation };
enum class BootstrapContinuationPhase : std::uint32_t { pending=0,accepted,cancelled,expired };
enum class HistoryAvailability : std::uint32_t { retained=0,archived,expired };
enum class MaintenancePhase : std::uint32_t { reviewed=0,activated,completed };
enum class SourceAuthorityKind : std::uint32_t { local=0,relay=1 };

struct SourceAuthority {
  std::string uid,owner,clock_domain;
  SourceAuthorityKind kind=SourceAuthorityKind::local;
};

struct Options {
  std::size_t max_active_sources=32,max_metrics=256,max_active_peers=32,
              max_active_subscriptions=32,max_pins=64,max_pin_objects=64;
  std::uint64_t maximum_lease_seconds=30ULL*24ULL*60ULL*60ULL;
  // 15 leaves room for atomic expiry with all 32 peers/subscriptions and
  // installed roots in the native store's default 128-record transaction.
  std::size_t max_bootstrap_continuations=15;
};
struct SourceRef {
  std::string uid;
  std::uint64_t epoch=0;
  bool operator==(const SourceRef&) const=default;
};
struct Source {
  SourceRef id;
  EngineIdentity identity;
  std::string owner;
  SourcePhase phase=SourcePhase::registered;
  std::uint64_t engine_revision=0,last_sequence=0,last_tick=0;
  Digest last_payload_sha256,state_sha256,state_object_sha256;
};
struct Cursor {
  std::uint64_t sequence=0;
  Digest payload_sha256;
  bool operator==(const Cursor&) const=default;
};
// Content identity includes the stream and sequence; equal numerical values in
// different streams do not accidentally share a history-retention identity.
Bytes encode_source_observation(const SourceRef&,std::uint64_t sequence,const Observation&);
Result decode_source_observation(std::string_view,SourceRef&,std::uint64_t& sequence,Observation&);
struct Metric {
  SourceRef source;
  Observation observation;
  std::uint64_t sequence=0;
};
struct Snapshot {
  std::uint32_t schema=2;
  std::uint64_t revision=0,lifetime_observations=0;
  std::vector<Source> sources;
  std::vector<Metric> metrics;
  std::size_t active_peers=0,active_subscriptions=0,pins=0;
  bool quarantined=false;
  std::string incarnation;
  Digest recovery_policy_sha256;
  std::size_t pending_bootstraps=0;
};
// Accounted capacity at one canonical generation. These values describe the
// configured quota, not filesystem allocation/free space or a reservation for a
// future transaction. Protected authority/retry/tombstone identities remain
// retained; source retirement does not promise to reclaim their metadata.
struct CapacityStatus {
  std::uint64_t generation=0,accounted_bytes=0,maximum_bytes=0;
  std::uint64_t ordinary_limit_bytes=0,maintenance_reserve_bytes=0;
  std::uint64_t ordinary_remaining_bytes=0,maintenance_remaining_bytes=0;
  std::uint64_t pending_reclamation_bytes=0;
  bool writable=false,index_ready=false;
};
struct Peer {
  std::string principal;
  Digest certificate_sha256;
  std::uint64_t generation=0,expires_unix_s=0;
  PeerPhase phase=PeerPhase::active;
};
struct Subscription {
  std::string id,principal;
  SourceRef scope;
  std::uint64_t generation=0,lease_until_unix_s=0;
  SubscriptionPhase phase=SubscriptionPhase::active;
  Cursor acknowledged,offered;
  bool has_offer=false;
};
struct Pin {
  std::string id,owner;
  PinKind kind=PinKind::operator_pin;
  std::uint64_t generation=0,expires_unix_s=0;
  std::vector<Digest> objects;
};
struct MaintenanceRequest {
  std::string id;
  SourceRef source;
  std::uint64_t expire_through=0,now_unix_s=0;
  HistoryAvailability disposition=HistoryAvailability::expired;
  std::string archive_directory;
  std::vector<Digest> objects;
};
struct MaintenancePlan {
  MaintenanceRequest request;
  MaintenancePhase phase=MaintenancePhase::reviewed;
  std::uint64_t protected_revision=0,subscription_revision=0,source_sequence=0,
                expected_bytes=0,new_floor=0;
  Digest protected_root_sha256,source_tip_sha256;
};
struct Bootstrap {
  std::string subscription_id,principal;
  std::uint64_t generation=0,lease_until_unix_s=0;
  SourceRef scope;
  Cursor tip;
  Digest snapshot_sha256;
  Bytes snapshot;
};
// An explicit pending checkpoint-to-stream handoff. Its dedicated pin kind
// consumes one ordinary pin slot and one bounded continuation slot. Metadata
// remains as an immutable outcome after those resource protections are released.
struct BootstrapContinuation {
  std::string id,subscription_id,principal;
  EngineIdentity source;
  std::uint64_t generation=0,lease_until_unix_s=0,credential_generation=0;
  Cursor tip;
  Digest checkpoint_sha256,state_sha256;
  BootstrapContinuationPhase phase=BootstrapContinuationPhase::pending;
};
struct ArchiveOptions {
  std::uint64_t maximum_bytes=1024ULL*1024ULL*1024ULL;
  std::size_t maximum_object_bytes=8U*1024U*1024U;
};
struct IArchiveStore {
  virtual ~IArchiveStore()=default;
  virtual std::string_view identifier() const noexcept=0;
  virtual Result put(std::string_view digest,std::string_view bytes)=0;
  virtual Result get(std::string_view digest,Bytes&)=0;
};
Result open_native_archive(std::string_view directory,const ArchiveOptions&,
                           std::unique_ptr<IArchiveStore>&);
struct RestoreReport {
  std::uint64_t copied_files=0,copied_bytes=0,source_generation=0;
  Digest source_root,incarnation;
  std::string staging_directory;
  bool published=false;
};
struct RestoreOptions {
  evidence::StoreOptions store;
  Options lifecycle;
  std::size_t maximum_files=262144,maximum_file_bytes=64U*1024U*1024U;
};
// Quiescent native store clone. Source bytes remain unchanged. The destination
// appears only after the clone is audited and its quarantine barrier commits.
// Existing destinations are never overwritten; a retained matching intent can
// reconcile an uncertain successful publication with the same operation id.
Result restore_quarantined_store(std::string_view source,std::string_view destination,
    std::string operation_id,RestoreReport&,const RestoreOptions& options={});

// Domain service over the canonical commit store. It never owns a separate
// authority log. The host serializes calls; all mutations use store CAS guards.
class Service {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  explicit Service(evidence::ICommitStore&,Options options={},IArchiveStore* archive=nullptr);
  ~Service();
  Service(const Service&)=delete;
  Service& operator=(const Service&)=delete;
  Result open();
  Result snapshot(Snapshot&) const;
  Result capacity(CapacityStatus&) const;
  Result source(const SourceRef&,Source&) const;
  Result source_authority(std::string_view uid,SourceAuthority&) const;
  // One immutable UID owner/clock/role across local engines and authenticated
  // relay streams. Stage in the caller's SAME canonical transaction so racing
  // first registrations cannot establish separate ownership authorities.
  Result stage_source_authority(evidence::CommitBatch&,const SourceAuthority&);
  Result register_engine(const EngineIdentity&,std::string owner,std::string operation_id);
  Result activate(const SourceRef&,std::string operation_id);
  Result begin_drain(const SourceRef&,std::string operation_id);
  Result seal(const SourceRef&,const Cursor& expected_tip,std::string operation_id);
  Result retire(const SourceRef&,std::string operation_id);
  // Enriches the SAME engine transaction with the lifecycle fence, canonical
  // source tip and active checkpoint. No state is advanced before store commit.
  Result stage_commit(evidence::CommitBatch&,const PreparedTransition&);
  Result upsert_peer(const Peer&,std::uint64_t expected_generation,
                     std::uint64_t now_unix_s,std::string operation_id);
  Result peer(std::string_view principal,Peer&) const;
  Result open_subscription(const Subscription&,std::uint64_t now_unix_s,
                           std::string operation_id);
  Result subscription(std::string_view id,Subscription&) const;
  Result offer(std::string_view id,std::uint64_t generation,const Cursor&,
               std::uint64_t now_unix_s,std::string operation_id);
  Result acknowledge(std::string_view id,std::uint64_t generation,const Cursor&,
                     std::uint64_t now_unix_s,std::string operation_id);
  Result close_subscription(std::string_view id,std::uint64_t generation,
                            std::string operation_id);
  Result expire_subscriptions(std::uint64_t now_unix_s,std::string operation_id);
  Result expire_peer_leases(std::uint64_t now_unix_s,std::string operation_id);
  Result put_pin(const Pin&,std::uint64_t expected_generation,
                 std::uint64_t now_unix_s,std::string operation_id);
  Result release_pin(std::string_view id,std::uint64_t expected_generation,
                     std::string operation_id);
  Result pin(std::string_view id,Pin&) const;
  Result review_maintenance(const MaintenanceRequest&,std::string operation_id,
                            MaintenancePlan&);
  Result maintenance(std::string_view id,MaintenancePlan&) const;
  Result activate_maintenance(std::string_view id,std::uint64_t now_unix_s,
                              std::string operation_id);
  Result execute_maintenance(std::string_view id,std::string operation_id);
  Result replay_status(const SourceRef&,std::uint64_t after_sequence) const;
  Result accept_bootstrap(const Bootstrap&,std::uint64_t now_unix_s,
                          std::string operation_id);
  Result bootstrap_continuation(std::string_view id,BootstrapContinuation&) const;
  Result pending_bootstrap(std::string_view subscription_id,BootstrapContinuation&) const;
  // Stage into the caller's SAME canonical transaction as its protocol offer
  // or acceptance/release witness. The caller supplies the operation identity.
  Result stage_bootstrap_continuation(evidence::CommitBatch&,const BootstrapContinuation&,
                                      std::uint64_t now_unix_s);
  Result stage_accept_bootstrap_continuation(evidence::CommitBatch&,std::string_view id,
      const Bootstrap&,std::uint64_t now_unix_s);
  Result stage_cancel_bootstrap_continuation(evidence::CommitBatch&,std::string_view id);
  // Stage scoped grant withdrawal and its active-directory guard in the
  // protocol's same authority-change transaction.
  Result stage_revoke_bootstrap_scope(evidence::CommitBatch&,std::string_view principal,const SourceRef&);
  Result installed_bootstrap(std::string_view subscription_id,Bootstrap&) const;
  // Restored stores remain unable to execute or grant replay until an explicit
  // local operator supplies their current policy witness. The witness is an
  // operator declaration, not a proof that the backup was the newest root.
  Result quarantine_restore(std::string operation_id,std::string_view restore_nonce={});
  Result reauthorize_restore(std::string_view current_policy_sha256,
                             std::string operation_id);
  // Original identities captured in the reauthorization commit, so post-commit
  // engine-root cleanup can resume even after the live view/configuration changed.
  Result reauthorization_sources(std::string_view operation_id,
                                 std::vector<EngineIdentity>&) const;
};
std::string_view phase_name(SourcePhase) noexcept;
std::string_view phase_name(SubscriptionPhase) noexcept;
}
```

## mesh.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::mesh::AuthenticatedIdentity` | struct |
| `anaiero::sdk::mesh::Session` | struct |
| `anaiero::sdk::mesh::Grant` | struct |
| `anaiero::sdk::mesh::Options` | struct |
| `anaiero::sdk::mesh::DeliveryItem` | struct |
| `anaiero::sdk::mesh::Delivery` | struct |
| `anaiero::sdk::mesh::ReceiveReceipt` | struct |
| `anaiero::sdk::mesh::TransferCost` | struct |
| `anaiero::sdk::mesh::ArtifactChunk` | struct |
| `anaiero::sdk::mesh::BootstrapOffer` | struct |
| `anaiero::sdk::mesh::ReceiverRetentionRequest` | struct |
| `anaiero::sdk::mesh::ReceiverRetentionPlan` | struct |
| `anaiero::sdk::mesh::encode_delivery` | function |
| `anaiero::sdk::mesh::decode_delivery` | function |
| `anaiero::sdk::mesh::encode_grant` | function |
| `anaiero::sdk::mesh::decode_grant` | function |
| `anaiero::sdk::mesh::encode_bootstrap` | function |
| `anaiero::sdk::mesh::decode_bootstrap` | function |
| `anaiero::sdk::mesh::Service` | class |
| `anaiero::sdk::mesh::Service::Service` | constructor |
| `anaiero::sdk::mesh::Service::~Service` | destructor |
| `anaiero::sdk::mesh::Service::authenticate` | method |
| `anaiero::sdk::mesh::Service::authorize` | method |
| `anaiero::sdk::mesh::Service::authorize_delivery` | method |
| `anaiero::sdk::mesh::Service::authorize_artifact` | method |
| `anaiero::sdk::mesh::Service::authorize_bootstrap` | method |
| `anaiero::sdk::mesh::Service::bootstrap_accepted` | method |
| `anaiero::sdk::mesh::Service::configure_grant` | method |
| `anaiero::sdk::mesh::Service::grant` | method |
| `anaiero::sdk::mesh::Service::subscribe` | method |
| `anaiero::sdk::mesh::Service::next` | method |
| `anaiero::sdk::mesh::Service::acknowledge` | method |
| `anaiero::sdk::mesh::Service::receive` | method |
| `anaiero::sdk::mesh::Service::received` | method |
| `anaiero::sdk::mesh::Service::received_observation` | method |
| `anaiero::sdk::mesh::Service::artifact_chunk` | method |
| `anaiero::sdk::mesh::Service::receive_artifact` | method |
| `anaiero::sdk::mesh::Service::offer_bootstrap` | method |
| `anaiero::sdk::mesh::Service::install_bootstrap` | method |
| `anaiero::sdk::mesh::Service::accept_bootstrap` | method |
| `anaiero::sdk::mesh::Service::cancel_bootstrap` | method |
| `anaiero::sdk::mesh::Service::review_receiver_retention` | method |
| `anaiero::sdk::mesh::Service::receiver_retention` | method |
| `anaiero::sdk::mesh::Service::activate_receiver_retention` | method |
| `anaiero::sdk::mesh::Service::reclaim_receiver_retention` | method |
| `anaiero::sdk::mesh::ScheduleKey` | struct |
| `anaiero::sdk::mesh::SchedulerOptions` | struct |
| `anaiero::sdk::mesh::FairScheduler` | class |
| `anaiero::sdk::mesh::FairScheduler::FairScheduler` | constructor |
| `anaiero::sdk::mesh::FairScheduler::~FairScheduler` | destructor |
| `anaiero::sdk::mesh::FairScheduler::enqueue` | method |
| `anaiero::sdk::mesh::FairScheduler::take` | method |
| `anaiero::sdk::mesh::FairScheduler::cancel` | method |
| `anaiero::sdk::mesh::FairScheduler::queued` | method |

```cpp
#pragma once
#include "anaiero/sdk/lifecycle.hpp"

namespace anaiero::sdk::mesh {
// Authentication is asserted only by an authenticated transport adapter. Scope
// and current authority are checked separately against the canonical store.
struct AuthenticatedIdentity {
  Digest certificate_sha256;
  std::uint64_t certificate_expiry_unix_s=0;
  bool verified=false;
};
struct Session {
  std::string principal;
  Digest certificate_sha256;
  std::uint64_t credential_generation=0,certificate_expiry_unix_s=0,authority_expiry_unix_s=0;
};
struct Grant {
  std::string principal;
  EngineIdentity source;
  std::uint64_t generation=1;
  bool send=false,receive=false,bootstrap=false;
  std::vector<Digest> artifacts;
};
struct Options {
  std::size_t maximum_page_observations=16,maximum_page_bytes=24576;
  std::size_t maximum_artifact_bytes=8U*1024U*1024U,maximum_artifacts=32;
};
struct DeliveryItem {evidence::ObservationClaim claim;Bytes payload;};
struct Delivery {
  std::string subscription_id;
  std::uint64_t generation=0;
  EngineIdentity source;
  lifecycle::Cursor from,to;
  std::vector<DeliveryItem> observations;
};
struct ReceiveReceipt {
  lifecycle::Cursor cursor;
  std::uint64_t generation=0,store_generation=0;
  Digest receipt_sha256;
  bool duplicate=false;
};
struct TransferCost {std::uint64_t bytes=0,work=0;};
struct ArtifactChunk {
  Digest digest;
  std::uint64_t offset=0,total_bytes=0;
  Bytes bytes;
};
struct BootstrapOffer {
  std::string id,subscription_id;
  std::uint64_t generation=0,lease_until_unix_s=0;
  EngineIdentity source;
  lifecycle::Cursor tip;
  Digest checkpoint_sha256;
  Bytes checkpoint;
};
// Trusted local receiver retention is explicit and bounded. An observation
// interval must contain complete received pages (at most 32 observations).
// Artifact/checkpoint expiry never removes source authority or retry identities.
struct ReceiverRetentionRequest {
  std::string id,principal;
  lifecycle::SourceRef source;
  std::uint64_t first_sequence=0,last_sequence=0;
  std::vector<Digest> artifacts;
  std::vector<std::string> bootstrap_ids;
  // Extra copies received under overlapping subscriptions are selected by
  // their returned receipt digest, without an unbounded historical scan.
  std::vector<Digest> delivery_receipts={};
};
struct ReceiverRetentionPlan {
  ReceiverRetentionRequest request;
  lifecycle::MaintenancePhase phase=lifecycle::MaintenancePhase::reviewed;
  std::string store_id;
  Digest reviewed_root,candidate_sha256;
  std::uint64_t reviewed_generation=0,expected_bytes=0;
  std::vector<Digest> objects;
};
Bytes encode_delivery(const Delivery&);
Result decode_delivery(std::string_view,Delivery&,const Options& options={});
Bytes encode_grant(const Grant&);
Result decode_grant(std::string_view,Grant&);
Bytes encode_bootstrap(const BootstrapOffer&);
Result decode_bootstrap(std::string_view,BootstrapOffer&,std::size_t maximum_bytes=8U*1024U*1024U);

// All calls execute on the same serialized storage lane as engine commits.
// No transport, OS handle or background thread is owned by this domain service.
// Receive receipts identify the authenticated relay; they never assert an
// original-author signature which has not been independently verified.
class Service {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  explicit Service(evidence::ICommitStore&,Options options={});
  ~Service();
  Result authenticate(const AuthenticatedIdentity&,std::string_view principal,
                      std::uint64_t now_unix_s,Session&) const;
  Result authorize(const Session&,const lifecycle::SourceRef&,bool sending,
                   std::uint64_t now_unix_s,Grant* grant=nullptr) const;
  Result authorize_delivery(const Session&,const Delivery&,std::uint64_t now_unix_s) const;
  Result authorize_artifact(const Session&,const lifecycle::SourceRef&,std::string_view digest,
                            std::uint64_t now_unix_s) const;
  Result authorize_bootstrap(const Session&,const BootstrapOffer&,std::uint64_t now_unix_s) const;
  Result bootstrap_accepted(const Session&,std::string_view subscription_id,std::uint64_t generation,
                            std::string_view checkpoint_sha256,const lifecycle::Cursor&,
                            std::uint64_t now_unix_s) const;
  Result configure_grant(const Grant&,std::uint64_t expected_generation,
                         std::string operation_id);
  Result grant(std::string_view principal,const lifecycle::SourceRef&,Grant&) const;
  Result subscribe(const Session&,const lifecycle::Subscription&,
                   std::uint64_t now_unix_s);
  Result next(const Session&,std::string_view subscription_id,std::uint64_t generation,
              std::uint64_t now_unix_s,Delivery&,TransferCost&);
  Result acknowledge(const Session&,const Delivery&,std::uint64_t now_unix_s);
  Result receive(const Session&,const Delivery&,std::uint64_t now_unix_s,ReceiveReceipt&);
  Result received(std::string_view principal,std::string_view subscription_id,
                  const lifecycle::SourceRef&,ReceiveReceipt&) const;
  // Relay evidence has its own indexed canonical namespace. Installing a
  // bootstrap advances its cursor without inventing locally observed events.
  Result received_observation(std::string_view principal,const lifecycle::SourceRef&,
                              std::uint64_t sequence,DeliveryItem&) const;
  Result artifact_chunk(const Session&,const lifecycle::SourceRef&,std::string_view digest,
                        std::uint64_t offset,std::size_t maximum_bytes,
                        std::uint64_t now_unix_s,ArtifactChunk&,TransferCost&);
  Result receive_artifact(const Session&,const lifecycle::SourceRef&,std::string_view digest,
                          Bytes bytes,std::uint64_t now_unix_s,ReceiveReceipt&);
  Result offer_bootstrap(const Session&,std::string subscription_id,std::uint64_t generation,
                        const lifecycle::SourceRef&,std::uint64_t lease_until_unix_s,
                        std::uint64_t now_unix_s,BootstrapOffer&);
  Result install_bootstrap(const Session&,const BootstrapOffer&,std::uint64_t now_unix_s,
                          ReceiveReceipt&);
  Result accept_bootstrap(const Session&,const BootstrapOffer&,std::uint64_t now_unix_s);
  // Trusted local maintenance; releasing a live transfer must follow transport
  // cancellation. Expiry alone does not assert that a reader released its data.
  Result cancel_bootstrap(std::string_view offer_id,std::string operation_id);
  // Review is a canonical immutable plan; activation rejects every intervening
  // store mutation. Current cursors/projections, live pins and all other roots
  // remain protective. Reclamation can resume after interruption; expired
  // reads/retries return HISTORY_EXPIRED instead of recreating accepted data.
  Result review_receiver_retention(const ReceiverRetentionRequest&,
                                  std::string operation_id,ReceiverRetentionPlan&);
  Result receiver_retention(std::string_view plan_id,ReceiverRetentionPlan&) const;
  Result activate_receiver_retention(std::string_view plan_id,std::string operation_id);
  Result reclaim_receiver_retention(std::string_view plan_id,std::string operation_id);
};

// Cost-aware deficit round robin. Each connection has one queued bounded unit;
// repeated connections share principal and source round budgets. The adapter
// also reserves a separate bounded control queue for close and completion.
struct ScheduleKey {std::uint64_t connection=0;std::string principal,source;};
struct SchedulerOptions {
  std::size_t maximum_connections=8;
  std::uint64_t byte_quantum=32768,work_quantum=64;
  std::uint64_t principal_byte_budget=65536,source_byte_budget=65536;
  std::uint64_t principal_work_budget=4096,source_work_budget=4096;
};
class FairScheduler {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  explicit FairScheduler(SchedulerOptions options={});
  ~FairScheduler();
  Result enqueue(ScheduleKey,TransferCost);
  bool take(std::uint64_t& connection);
  void cancel(std::uint64_t connection);
  std::size_t queued() const noexcept;
};
}
```

## module_abi.h

| Public symbol | Kind |
|---|---|
| `AnaieroBytesV1` | alias |
| `AnaieroBufferV1` | alias |
| `AnaieroModuleApiV1` | alias |
| `AnaieroModuleEntryV1` | alias |

```cpp
#ifndef ANAIERO_SDK_MODULE_ABI_H
#define ANAIERO_SDK_MODULE_ABI_H
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32)
#define ANAIERO_MODULE_CALL __cdecl
#if defined(ANAIERO_MODULE_BUILD)
#define ANAIERO_MODULE_EXPORT __declspec(dllexport)
#else
#define ANAIERO_MODULE_EXPORT
#endif
#else
#define ANAIERO_MODULE_CALL
#define ANAIERO_MODULE_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define ANAIERO_MODULE_ABI_VERSION 1u
typedef struct AnaieroBytesV1 {const uint8_t* data;uint64_t size;} AnaieroBytesV1;
typedef struct AnaieroBufferV1 {uint8_t* data;uint64_t capacity;uint64_t written;} AnaieroBufferV1;
/* Canonical ASK1 wire records only; no STL, exceptions, allocation ownership or
 * compiler vtables cross this boundary. Callbacks return SDK Code integers.
 * The host owns output buffers. The module owns handles until destroy().
 * All callbacks execute serially. Native modules are explicitly trusted: this
 * ABI does not sandbox callbacks or promise preemption of arbitrary code. */
typedef struct AnaieroModuleApiV1 {
  uint32_t struct_size,abi_version;
  void* context;
  int32_t (ANAIERO_MODULE_CALL *describe)(void*,AnaieroBufferV1*);
  int32_t (ANAIERO_MODULE_CALL *create)(void*,AnaieroBytesV1,void**);
  void (ANAIERO_MODULE_CALL *destroy)(void*,void*);
  int32_t (ANAIERO_MODULE_CALL *state)(void*,void*,AnaieroBufferV1*);
  int32_t (ANAIERO_MODULE_CALL *prepare)(void*,void*,AnaieroBytesV1,AnaieroBufferV1*);
  int32_t (ANAIERO_MODULE_CALL *install)(void*,void*,AnaieroBytesV1);
  int32_t (ANAIERO_MODULE_CALL *restore)(void*,void*,AnaieroBytesV1);
  int32_t (ANAIERO_MODULE_CALL *fork)(void*,void*,void**);
  int32_t (ANAIERO_MODULE_CALL *change)(void*,void*,AnaieroBytesV1,AnaieroBufferV1*);
  void (ANAIERO_MODULE_CALL *cancel)(void*,void*);
} AnaieroModuleApiV1;
typedef const AnaieroModuleApiV1* (ANAIERO_MODULE_CALL *AnaieroModuleEntryV1)(void);
ANAIERO_MODULE_EXPORT const AnaieroModuleApiV1* ANAIERO_MODULE_CALL anaiero_module_v1(void);
#ifdef __cplusplus
}
#endif
#endif
```

## module_host.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::ModuleAdmission` | struct |
| `anaiero::sdk::load_native_module` | function |
| `anaiero::sdk::load_process_module` | function |
| `anaiero::sdk::ModuleApiAdapter` | class |
| `anaiero::sdk::ModuleApiAdapter::ModuleApiAdapter` | constructor |
| `anaiero::sdk::ModuleApiAdapter::~ModuleApiAdapter` | destructor |
| `anaiero::sdk::ModuleApiAdapter::api` | method |
| `anaiero::sdk::run_module_stdio` | function |

```cpp
#pragma once
#include "anaiero/sdk/catalog.hpp"
#include "anaiero/sdk/module_abi.h"

namespace anaiero::sdk {
struct ModuleAdmission {
  std::string artifact_path;
  Digest artifact_sha256;
  // Required only for an in-process image. Hash agreement is not a trust grant.
  bool trusted_native=false;
  std::vector<std::string> arguments;
  std::uint32_t timeout_ms=2000;
  std::size_t maximum_wire_bytes=16U*1024U*1024U;
};
// Native images and owned processes retain their implementation resources until
// every engine/fork is destroyed. Both check executable bytes before admission.
Result load_native_module(const ModuleAdmission&,EngineDescriptor&,std::shared_ptr<IEngineFactory>&);
Result load_process_module(const ModuleAdmission&,EngineDescriptor&,std::shared_ptr<IEngineFactory>&);
// Adapter for module authors. It translates compiled C++ capabilities into the
// C table locally; the table/context must outlive all created module engines.
class ModuleApiAdapter {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  ModuleApiAdapter(EngineDescriptor,std::shared_ptr<IEngineFactory>);
  ~ModuleApiAdapter();
  const AnaieroModuleApiV1* api()const noexcept;
};
// Process module entry point. Uses bounded chunked stdin/stdout framing and
// exactly one engine per process. Diagnostics belong on stderr.
int run_module_stdio(EngineDescriptor,std::shared_ptr<IEngineFactory>);
}
```

## native_mesh.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::mesh::NativeCredentials` | struct |
| `anaiero::sdk::mesh::AcknowledgementTiming` | struct |
| `anaiero::sdk::mesh::IAcknowledgementObserver` | class |
| `anaiero::sdk::mesh::IAcknowledgementObserver::~IAcknowledgementObserver` | destructor |
| `anaiero::sdk::mesh::IAcknowledgementObserver::observe` | method |
| `anaiero::sdk::mesh::NativeServerOptions` | struct |
| `anaiero::sdk::mesh::NativeServerStatus` | struct |
| `anaiero::sdk::mesh::NativeServer` | class |
| `anaiero::sdk::mesh::NativeServer::NativeServer` | constructor |
| `anaiero::sdk::mesh::NativeServer::~NativeServer` | destructor |
| `anaiero::sdk::mesh::NativeServer::start` | method |
| `anaiero::sdk::mesh::NativeServer::notify_commit` | method |
| `anaiero::sdk::mesh::NativeServer::status` | method |
| `anaiero::sdk::mesh::NativeServer::request_stop` | method |
| `anaiero::sdk::mesh::NativeServer::stop` | method |
| `anaiero::sdk::mesh::read_native_server_options` | function |
| `anaiero::sdk::mesh::administer_native` | function |
| `anaiero::sdk::mesh::submit_native_administration` | function |
| `anaiero::sdk::mesh::NativeSubscriberOptions` | struct |
| `anaiero::sdk::mesh::NativeSubscriberStatus` | struct |
| `anaiero::sdk::mesh::NativeSubscriber` | class |
| `anaiero::sdk::mesh::NativeSubscriber::NativeSubscriber` | constructor |
| `anaiero::sdk::mesh::NativeSubscriber::~NativeSubscriber` | destructor |
| `anaiero::sdk::mesh::NativeSubscriber::start` | method |
| `anaiero::sdk::mesh::NativeSubscriber::status` | method |
| `anaiero::sdk::mesh::NativeSubscriber::wait` | method |
| `anaiero::sdk::mesh::NativeSubscriber::request_stop` | method |
| `anaiero::sdk::mesh::NativeSubscriber::stop` | method |
| `anaiero::sdk::mesh::fetch_native_artifact` | function |
| `anaiero::sdk::mesh::install_native_bootstrap` | function |

```cpp
#pragma once
#include "anaiero/sdk/host.hpp"
#include "anaiero/sdk/mesh.hpp"

namespace anaiero::sdk::mesh {
struct NativeCredentials {std::string certificate_file,private_key_file,authority_file;};
struct AcknowledgementTiming {
  std::uint64_t elapsed_ns=0,subscription_generation=0,to_sequence=0;
};
// Borrowed bounded observation port, called on authenticated client workers.
// Calls can overlap; implementations must be thread safe, nonblocking and must
// not reenter transport or canonical storage. Elapsed time spans offered-page
// write through durable acceptance of its exact ACK, including receiver work.
class IAcknowledgementObserver {
public:
  virtual ~IAcknowledgementObserver()=default;
  virtual void observe(const AcknowledgementTiming&) noexcept=0;
};
struct NativeServerOptions {
  NativeCredentials credentials;
  std::string address="127.0.0.1";
  std::uint16_t port=0;
  std::size_t maximum_clients=8,maximum_handshakes=4;
  std::uint32_t handshake_timeout_ms=5000,io_timeout_ms=5000;
  Options service;
  IAcknowledgementObserver* acknowledgement_observer=nullptr;
};
struct NativeServerStatus {
  std::uint64_t accepted_connections=0,rejected_connections=0,delivered_pages=0,
                acknowledged_pages=0,delivered_bytes=0,charged_read_bytes=0,
                owner_wakeups=0,availability_generation=0;
  std::size_t handshakes=0,clients=0,queued_work=0;
  bool running=false;
  std::string address;
  std::uint16_t port=0;
};
// Storage tasks execute through the host's bounded lane, after in-flight engine
// commits. Transport workers never touch canonical state directly. notify_commit
// is a coalescible signal, safe on the host's signal-only completion callback.
class NativeServer {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  NativeServer(host::IStoreTaskExecutor&,NativeServerOptions);
  ~NativeServer();
  Result start();
  void notify_commit(std::uint64_t generation) noexcept;
  NativeServerStatus status() const;
  void request_stop() noexcept;
  void stop() noexcept;
};
Result read_native_server_options(std::string_view path,NativeServerOptions&);
// Local IPC/operator adapter only. The remote TLS protocol has no admin verb.
Result administer_native(host::IStoreTaskExecutor&,std::string_view command,Bytes& response);
Result submit_native_administration(host::IStoreTaskExecutor&,std::string command,
                                    std::function<void(Result,Bytes)> completion);

struct NativeSubscriberOptions {
  NativeCredentials credentials;
  std::string address="127.0.0.1",local_principal,remote_principal;
  std::uint16_t port=0;
  Digest server_certificate_sha256;
  lifecycle::Subscription subscription;
  std::uint32_t connect_timeout_ms=5000,io_timeout_ms=5000;
  std::uint32_t minimum_retry_ms=100,maximum_retry_ms=5000;
  // Zero runs until stopped. A finite target supports reproducible qualification.
  std::uint64_t through_sequence=0;
  Options service;
};
struct NativeSubscriberStatus {
  std::uint64_t connections=0,reconnects=0,committed_pages=0,committed_bytes=0;
  lifecycle::Cursor cursor;
  bool running=false,blocked=false,completed=false;
  Result result;
};
class NativeSubscriber {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  NativeSubscriber(host::IStoreTaskExecutor&,NativeSubscriberOptions);
  ~NativeSubscriber();
  Result start();
  NativeSubscriberStatus status() const;
  Result wait(std::uint32_t timeout_ms) const;
  void request_stop() noexcept;
  void stop() noexcept;
};
// Finite artifact/bootstrap transfers use independent authenticated connections
// and the same bounded scheduler. Every chunk is reauthorized before handoff.
Result fetch_native_artifact(host::IStoreTaskExecutor&,const NativeSubscriberOptions&,
                            std::string_view digest,ReceiveReceipt&);
Result install_native_bootstrap(host::IStoreTaskExecutor&,const NativeSubscriberOptions&,
                               std::uint64_t next_generation,ReceiveReceipt&);
}
```

## native_session.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::host::NativeSession` | class |
| `anaiero::sdk::host::NativeSession::NativeSession` | constructor |
| `anaiero::sdk::host::NativeSession::~NativeSession` | destructor |
| `anaiero::sdk::host::NativeSession::bind_catalog` | method |
| `anaiero::sdk::host::NativeSession::open` | method |
| `anaiero::sdk::host::NativeSession::ensure_engines` | method |
| `anaiero::sdk::host::NativeSession::retain_host_artifact` | method |
| `anaiero::sdk::host::NativeSession::runtime` | method |
| `anaiero::sdk::host::NativeSession::runner` | method |
| `anaiero::sdk::host::NativeSession::store` | method |
| `anaiero::sdk::host::NativeSession::lifecycle` | method |

```cpp
#pragma once
#include "anaiero/sdk/host.hpp"
#include "anaiero/sdk/lifecycle.hpp"
namespace anaiero::sdk::host {
// Native composition for an already initialized canonical modular session.
// Opening checks retained module/model identities and the persisted capacity.
// No legacy/live-session format is silently migrated by this adapter.
class NativeSession {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  NativeSession();
  ~NativeSession();
  // Validates the canonical selected build and its exact retained source
  // witnesses before enabling any old logical engine identity. No raw alias
  // can be supplied by the caller. Call before freezing the native catalog.
  static Result bind_catalog(evidence::ICommitStore&,EngineModuleCatalog&);
  Result open(std::string_view store_directory);
  Result ensure_engines();
  Result retain_host_artifact(std::string_view executable_path);
  SessionService& runtime();
  SessionRunner& runner();
  evidence::ICommitStore& store();
  lifecycle::Service& lifecycle();
};
}
```

## observer.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::ObserverEventKind` | enum |
| `anaiero::sdk::ObserverEvent` | struct |
| `anaiero::sdk::IObserverSink` | struct |
| `anaiero::sdk::IObserverSink::~IObserverSink` | destructor |
| `anaiero::sdk::IObserverSink::enqueue` | method |
| `anaiero::sdk::IObserver` | struct |
| `anaiero::sdk::IObserver::~IObserver` | destructor |
| `anaiero::sdk::IObserver::observe` | method |
| `anaiero::sdk::IObserverClock` | struct |
| `anaiero::sdk::IObserverClock::~IObserverClock` | destructor |
| `anaiero::sdk::IObserverClock::now_ns` | method |
| `anaiero::sdk::SteadyObserverClock` | class |
| `anaiero::sdk::SteadyObserverClock::now_ns` | method |
| `anaiero::sdk::ObserverStatistics` | struct |
| `anaiero::sdk::ObserverService` | class |
| `anaiero::sdk::ObserverService::ObserverService` | constructor |
| `anaiero::sdk::ObserverService::~ObserverService` | destructor |
| `anaiero::sdk::ObserverService::add` | method |
| `anaiero::sdk::ObserverService::enqueue` | method |
| `anaiero::sdk::ObserverService::dispatch` | method |
| `anaiero::sdk::ObserverService::statistics` | method |

```cpp
#pragma once
#include "anaiero/sdk/engine.hpp"
#include <memory>

namespace anaiero::sdk {
enum class ObserverEventKind : std::uint32_t {committed=0,failed,recovered,retired,trial_completed};
struct ObserverEvent {
  ObserverEventKind kind=ObserverEventKind::committed;
  EngineIdentity identity;
  std::string operation_id;
  std::uint64_t revision=0,tick=0,generation=0;
  Phase phase=Phase::ready;
  Code result=Code::ok;
};
struct IObserverSink {
  virtual ~IObserverSink()=default;
  virtual Result enqueue(const ObserverEvent&)=0;
};
struct IObserver {
  virtual ~IObserver()=default;
  virtual Result observe(const ObserverEvent&)=0;
};
struct IObserverClock {
  virtual ~IObserverClock()=default;
  virtual std::uint64_t now_ns()const noexcept=0;
};
// Native adapter implemented in anaiero_module_host, not in the portable core.
class SteadyObserverClock final:public IObserverClock {
public:
  std::uint64_t now_ns()const noexcept override;
};
struct ObserverStatistics {
  std::uint64_t accepted=0,dropped=0,delivered=0,callback_faults=0,budget_overruns=0;
  std::size_t queued=0,enabled_observers=0;
};
// The owner only enqueues passive values. A host dispatches callbacks later,
// outside SessionService. Faults and elapsed-budget overruns are independent
// diagnostics and disable the offending observer. Inline code is not preempted.
class ObserverService final:public IObserverSink {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  explicit ObserverService(std::size_t event_capacity=64,std::size_t observer_capacity=8);
  ~ObserverService();
  Result add(std::string id,std::shared_ptr<IObserver>);
  Result enqueue(const ObserverEvent&)override;
  Result dispatch(std::size_t maximum_callbacks,std::uint64_t callback_budget_ns,const IObserverClock&);
  ObserverStatistics statistics()const noexcept;
};
}
```

## projection.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::MetricView` | struct |
| `anaiero::sdk::ProjectionService` | class |
| `anaiero::sdk::ProjectionService::ProjectionService` | constructor |
| `anaiero::sdk::ProjectionService::~ProjectionService` | destructor |
| `anaiero::sdk::ProjectionService::can_apply` | method |
| `anaiero::sdk::ProjectionService::apply` | method |
| `anaiero::sdk::ProjectionService::replace` | method |
| `anaiero::sdk::ProjectionService::retire` | method |
| `anaiero::sdk::ProjectionService::clear` | method |
| `anaiero::sdk::ProjectionService::snapshot` | method |
| `anaiero::sdk::ProjectionService::size` | method |

```cpp
#pragma once
#include "anaiero/sdk/engine.hpp"
#include <memory>
namespace anaiero::sdk {
struct MetricView { EngineIdentity identity;Observation observation;std::uint64_t revision=0,generation=0; };
class ProjectionService {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  explicit ProjectionService(std::size_t capacity=512);
  ~ProjectionService();
  Result can_apply(const PreparedTransition&) const;
  Result apply(const PreparedTransition&,std::uint64_t committed_generation);
  Result replace(const std::vector<MetricView>& committed);
  void retire(std::string_view source_uid,std::uint64_t epoch);
  void clear();
  std::vector<MetricView> snapshot() const;
  std::size_t size() const noexcept;
};
}
```

## runtime.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::evidence::CommitBatch` | struct |
| `anaiero::sdk::evidence::CommitReceipt` | struct |
| `anaiero::sdk::evidence::ICommitStore` | class |
| `anaiero::sdk::evidence::ICommitQueue` | class |
| `anaiero::sdk::ICommitPolicy` | struct |
| `anaiero::sdk::ICommitPolicy::~ICommitPolicy` | destructor |
| `anaiero::sdk::ICommitPolicy::enrich` | method |
| `anaiero::sdk::ICommitPolicy::restore_admission` | method |
| `anaiero::sdk::ICommitPolicy::scheduling_admission` | method |
| `anaiero::sdk::ICommitPolicy::recovery_sources` | method |
| `anaiero::sdk::EngineView` | struct |
| `anaiero::sdk::SessionView` | struct |
| `anaiero::sdk::SessionService` | class |
| `anaiero::sdk::SessionService::SessionService` | constructor |
| `anaiero::sdk::SessionService::~SessionService` | destructor |
| `anaiero::sdk::SessionService::set_commit_queue` | method |
| `anaiero::sdk::SessionService::set_observer_sink` | method |
| `anaiero::sdk::SessionService::add_engine` | method |
| `anaiero::sdk::SessionService::recover` | method |
| `anaiero::sdk::SessionService::begin_step` | method |
| `anaiero::sdk::SessionService::pump` | method |
| `anaiero::sdk::SessionService::step` | method |
| `anaiero::sdk::SessionService::control` | method |
| `anaiero::sdk::SessionService::remove_retired` | method |
| `anaiero::sdk::SessionService::scheduling_admission` | method |
| `anaiero::sdk::SessionService::busy` | method |
| `anaiero::sdk::SessionService::view` | method |
| `anaiero::sdk::SessionService::find` | method |

```cpp
#pragma once
#include "anaiero/sdk/catalog.hpp"
#include "anaiero/sdk/projection.hpp"
#include "anaiero/sdk/observer.hpp"
#include <memory>
namespace anaiero::sdk::evidence {struct CommitBatch;struct CommitReceipt;class ICommitStore;class ICommitQueue;}
namespace anaiero::sdk {
struct ICommitPolicy {
  virtual ~ICommitPolicy()=default;
  virtual Result enrich(evidence::CommitBatch&,const PreparedTransition&)=0;
  // Expired means explicitly retired: recovery skips active materialization.
  virtual Result restore_admission(const EngineIdentity&) const { return {}; }
  virtual Result scheduling_admission(const EngineIdentity&,bool& eligible) const {eligible=true;return {};}
  // A lifecycle provider returns only active/draining/sealed materializations.
  // This bounds recovery by active sources instead of lifetime engine count.
  virtual Result recovery_sources(std::vector<EngineIdentity>&) const {
    return {Code::unsupported,"no active source catalog"};
  }
};
struct EngineView { EngineIdentity identity;EngineState state;std::uint64_t committed_generation=0; };
struct SessionView { bool busy=false,degraded=false;std::string error;std::vector<EngineView> engines; };
class SessionService {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  SessionService(EngineModuleCatalog&,evidence::ICommitStore&,ProjectionService&,
                 ICommitPolicy* policy=nullptr,std::size_t capacity=32);
  ~SessionService();
  void set_commit_queue(evidence::ICommitQueue*) noexcept;
  void set_observer_sink(IObserverSink*) noexcept;
  Result add_engine(const EngineIdentity&,const Envelope& configuration,
                    const EngineState* initial=nullptr);
  Result recover();
  Result begin_step(std::string_view instance_id,const StepRequest&,bool change=false);
  // Complete at most one pending transition; no polling loop is hidden here.
  Result pump();
  Result step(std::string_view instance_id,const StepRequest&,bool change=false);
  Result control(std::string_view instance_id,std::string operation_id,Phase phase);
  Result remove_retired(std::string_view instance_id);
  Result scheduling_admission(std::string_view instance_id,bool& eligible) const;
  bool busy() const noexcept;
  SessionView view() const;
  IEngine* find(std::string_view instance_id) noexcept;
};
}
```

## session_runner.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::EngineSchedule` | struct |
| `anaiero::sdk::SessionDefinition` | struct |
| `anaiero::sdk::SessionProgress` | struct |
| `anaiero::sdk::SessionRunner` | class |
| `anaiero::sdk::SessionRunner::SessionRunner` | constructor |
| `anaiero::sdk::SessionRunner::~SessionRunner` | destructor |
| `anaiero::sdk::SessionRunner::initialize` | method |
| `anaiero::sdk::SessionRunner::reconfigure` | method |
| `anaiero::sdk::SessionRunner::open` | method |
| `anaiero::sdk::SessionRunner::definition` | method |
| `anaiero::sdk::SessionRunner::progress` | method |
| `anaiero::sdk::SessionRunner::begin_cycle` | method |
| `anaiero::sdk::SessionRunner::next_step` | method |
| `anaiero::sdk::SessionRunner::finish_cycle` | method |
| `anaiero::sdk::encode_session_definition` | function |
| `anaiero::sdk::decode_session_definition` | function |

```cpp
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
```

## storage_tools.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::evidence::MigrationReport` | struct |
| `anaiero::sdk::evidence::migrate_legacy_evidence` | function |
| `anaiero::sdk::evidence::CompactionOptions` | struct |
| `anaiero::sdk::evidence::CompactionPlan` | struct |
| `anaiero::sdk::evidence::CompactionReport` | struct |
| `anaiero::sdk::evidence::preview_native_compaction` | function |
| `anaiero::sdk::evidence::execute_native_compaction` | function |
| `anaiero::sdk::evidence::resume_native_compaction` | function |
| `anaiero::sdk::evidence::qualify_operation_id` | function |
| `anaiero::sdk::evidence::inspect_native_operation_epoch` | function |

```cpp
#pragma once
#include "anaiero/sdk/evidence.hpp"

namespace anaiero::sdk::evidence {
struct MigrationReport {
  Digest source_fingerprint,destination_root;
  std::string backup_directory;
  std::uint64_t source_files=0,source_bytes=0,objects=0,events=0;
  std::uint64_t target_bytes=0,peak_required_bytes=0,retained_backup_bytes=0;
  bool resumed=false,already_complete=false,source_unchanged=false,activated=false;
};
// Quiescent evidence-only migration. Preserves all verified legacy CAS objects,
// exact committed event wire bytes and provenance; does not upgrade live engine
// state. A separate immutable source snapshot remains beside the destination and
// is explicitly reported outside the canonical store's quota.
Result migrate_legacy_evidence(std::string_view source,std::string_view destination,
    const StoreOptions&,MigrationReport&);

struct CompactionOptions {
  std::string operation_id;
  std::uint64_t expected_generation=any_generation;
  Digest expected_root;
  // Additional temporary bytes, separately checked against store and filesystem
  // headroom. Pages and index work remain bounded while the history is scanned.
  std::uint64_t working_budget_bytes=256ULL*1024ULL*1024ULL;
  bool expire_operation_receipts=false;
};
struct CompactionPlan {
  CompactionOptions options;
  Digest plan_digest,source_root,source_base;
  std::uint64_t generation=0,next_operation_epoch=0;
  std::uint64_t accounted_bytes_before=0,required_working_bytes=0;
  std::uint64_t retained_records=0,retained_blobs=0,retained_operations=0;
  std::uint64_t summarized_record_versions=0,expired_observations=0;
  std::uint64_t canonical_transactions_removed=0;
};
struct CompactionReport {
  Digest plan_digest,canonical_base_digest;
  std::uint64_t generation=0,operation_epoch=0;
  std::uint64_t accounted_bytes_before=0,accounted_bytes_after=0;
  std::uint64_t removed_transaction_bytes=0,removed_index_bytes=0;
  std::uint64_t summarized_record_versions=0,expired_observations=0;
  bool activated=false,cleanup_complete=false,already_complete=false;
};
// A maintenance scan creates a deterministic plan. Execution revalidates the
// exact root/index/base and recomputes the plan before publication. All latest
// records (including tombstones/authority), blob identity fences and source tips
// remain protected; arbitrary identity cardinality still consumes finite quota.
Result preview_native_compaction(ICommitStore&,const CompactionOptions&,CompactionPlan&);
Result execute_native_compaction(ICommitStore&,const CompactionPlan&,CompactionReport&);
Result resume_native_compaction(ICommitStore&,CompactionReport&);
// Explicitly create a NEW operation identity after an operator rotates receipt
// history. Never use this to relabel an unknown retry from an earlier epoch.
std::string qualify_operation_id(std::uint64_t epoch,std::string_view logical_id);
// Read-only format/epoch inspection for a caller holding a quiescent store lock.
// Does not open a writer, repair a cache, or initialize missing storage.
Result inspect_native_operation_epoch(std::string_view directory,std::uint64_t& epoch);
}
```

## types.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::Bytes` | alias |
| `anaiero::sdk::Digest` | alias |
| `anaiero::sdk::Code` | enum |
| `anaiero::sdk::Result` | struct |
| `anaiero::sdk::Result::ok` | method |
| `anaiero::sdk::Result::committed` | method |
| `anaiero::sdk::Envelope` | struct |
| `anaiero::sdk::Envelope::operator==` | method |
| `anaiero::sdk::Limits` | struct |
| `anaiero::sdk::sha256` | function |
| `anaiero::sdk::valid_digest` | function |
| `anaiero::sdk::valid_token` | function |
| `anaiero::sdk::code_name` | function |
| `anaiero::sdk::pack` | function |
| `anaiero::sdk::unpack` | function |
| `anaiero::sdk::encode_envelope` | function |
| `anaiero::sdk::decode_envelope` | function |

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace anaiero::sdk {
using Bytes = std::string; // Owned binary-safe bytes; no borrowed asynchronous payloads.
using Digest = std::string;
enum class Code : std::uint32_t {
  ok=0, invalid, unsupported, conflict, capacity, not_found, corrupt, io_error,
  committed_degraded, cancelled, expired, unauthorized, history_expired, busy, completed,
  archive_required,archive_unavailable,rebootstrap_required,deadline
};
struct Result {
  Code code=Code::ok;
  std::string message;
  bool ok() const noexcept { return code==Code::ok; }
  bool committed() const noexcept { return code==Code::ok||code==Code::committed_degraded; }
  explicit operator bool() const noexcept { return ok(); }
};
struct Envelope {
  std::string schema;
  Bytes payload;
  bool operator==(const Envelope&) const = default;
};
struct Limits {
  std::size_t max_payload_bytes=4U*1024U*1024U;
  std::size_t max_checkpoint_bytes=4U*1024U*1024U;
  std::size_t max_observations=128;
  std::size_t max_receipts=128;
};
Digest sha256(std::string_view bytes);
bool valid_digest(std::string_view value) noexcept;
bool valid_token(std::string_view value, std::size_t maximum=128) noexcept;
std::string_view code_name(Code code) noexcept;
// Canonical binary length-prefixed record codec; preserves embedded NUL bytes.
Bytes pack(std::span<const std::string> fields);
inline Bytes pack(std::initializer_list<std::string> fields) { return pack(std::span<const std::string>(fields.begin(),fields.size())); }
Result unpack(std::string_view bytes, std::vector<std::string>& fields,
              std::size_t max_fields=256, std::size_t max_bytes=16U*1024U*1024U);
Bytes encode_envelope(const Envelope& value);
Result decode_envelope(std::string_view bytes, Envelope& value);
}
```

## Syntax-index audit

Exact header text is retained above even when the optional navigation parser
cannot interpret a declaration. Known C calling-convention qualifiers are
masked with equal-length whitespace for parsing only; displayed source is unchanged.

| Header | Line | Parser note |
|---|---|---|
| `engine.hpp` | 73 | `type_identifier: (missing syntax node)` |
| `evidence.hpp` | 174 | `type_identifier: (missing syntax node)` |
| `evidence.hpp` | 174 | `type_identifier: (missing syntax node)` |
| `host.hpp` | 77 | `type_identifier: (missing syntax node)` |
| `lifecycle.hpp` | 160 | `type_identifier: (missing syntax node)` |
| `lifecycle.hpp` | 168 | `type_identifier: (missing syntax node)` |
| `lifecycle.hpp` | 233 | `type_identifier: (missing syntax node)` |
| `mesh.hpp` | 78 | `type_identifier: (missing syntax node)` |
| `mesh.hpp` | 91 | `type_identifier: (missing syntax node)` |
| `mesh.hpp` | 157 | `type_identifier: (missing syntax node)` |
