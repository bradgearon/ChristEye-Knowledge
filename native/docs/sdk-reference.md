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

**Public-header manifest SHA-256:** `61c51e3b0ef624866f1dc26c4eda8bd6bbd41b0903a2ac05897f7fdf9371d1d0`

| Header | SHA-256 |
|---|---|
| [catalog.hpp](../sdk/include/anaiero/sdk/catalog.hpp) | `e07ceb3fe2ce59dc95438005fa45769d5b38a4310431ed86b5551d11cebf261a` |
| [engine.hpp](../sdk/include/anaiero/sdk/engine.hpp) | `6c7f377106bea1d231d04423e9c302b8088808a0e0608f2361a78c16c217cf16` |
| [engine_files.hpp](../sdk/include/anaiero/sdk/engine_files.hpp) | `c1a311a612733ded9d175472111ecdc4ad0e064ec59e91c68fac58dcf8b593cd` |
| [engines.hpp](../sdk/include/anaiero/sdk/engines.hpp) | `2bf154a15f57beedb85a500ce8f7da6a878c9682d31fdaa52e60dbe6477ed0e4` |
| [evidence.hpp](../sdk/include/anaiero/sdk/evidence.hpp) | `33e70af2acdc9849efdf6a9d40a40aac29e40c2fe0a839bf4577f24e8b2f0e18` |
| [experiment_history.hpp](../sdk/include/anaiero/sdk/experiment_history.hpp) | `fab291fe81c41b4a1c9a52c621be741eff70b58cbc26cef3a5d28bb8c05917ca` |
| [experiments.hpp](../sdk/include/anaiero/sdk/experiments.hpp) | `de18d005d734e926c1593f3fed870d4bd01ec9901f2acf5ec4047e3754cd0cec` |
| [lifecycle.hpp](../sdk/include/anaiero/sdk/lifecycle.hpp) | `c859fc5f7130885a360fbf6b3c9684394a0f5d82c36e0dd992303f4e74ba487d` |
| [module_abi.h](../sdk/include/anaiero/sdk/module_abi.h) | `6a5c4c08a9da5873875ca3926d5b8dd9bf6d6cca2bc599856fa594bb31c2a868` |
| [module_host.hpp](../sdk/include/anaiero/sdk/module_host.hpp) | `e17c5ead618314291511e00795f36adc0ebe7135e10f44dfe417be94c6b77f9e` |
| [observer.hpp](../sdk/include/anaiero/sdk/observer.hpp) | `956071d349b9e4e82599b055f1109c5e4aacf6beba02da1b723ee0cdff8afe3a` |
| [projection.hpp](../sdk/include/anaiero/sdk/projection.hpp) | `d8304748739cef4086277b2c80615d685d87c2ea5536bd1d0f5d2c1933fbe9ca` |
| [runtime.hpp](../sdk/include/anaiero/sdk/runtime.hpp) | `5130027301e56991dbe3540c839b9f1929f462c5787a30ab061825f02a4744bf` |
| [session_runner.hpp](../sdk/include/anaiero/sdk/session_runner.hpp) | `debc0606bc7a45ac80f5778b3281cdb857e5436973940bfd2eb0b634679093d8` |
| [storage_tools.hpp](../sdk/include/anaiero/sdk/storage_tools.hpp) | `4ec3c8c93411c2692642e66f41e016973f2fa4dc7dd57264225d3e97e908ce38` |
| [types.hpp](../sdk/include/anaiero/sdk/types.hpp) | `2d4d5d222c41e59ac3ff91dfbe7c388d68d2eb3bdacad77bb3eb8a6f71b202bb` |

## catalog.hpp

| Public symbol | Kind |
|---|---|
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
class EngineModuleCatalog {
  struct Impl;
  std::unique_ptr<Impl> impl_;
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
};
struct AuditReport {
  std::uint64_t generations=0,records=0,blobs=0,expired_blobs=0;
  std::uint64_t index_nodes_read=0,canonical_records_read=0;
};
struct IndexStatistics {
  std::uint64_t nodes_read=0,nodes_written=0,point_queries=0,range_queries=0;
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
};
enum class FaultPoint : std::uint32_t {
  none,after_objects,after_transaction,after_root,before_index,after_index
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
Digest record_digest(const Record&);
Digest batch_digest(const CommitBatch&);

struct CommitCompletion {std::uint64_t ticket=0;Result result;CommitReceipt receipt;};
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

## lifecycle.hpp

| Public symbol | Kind |
|---|---|
| `anaiero::sdk::lifecycle::SourcePhase` | enum |
| `anaiero::sdk::lifecycle::PeerPhase` | enum |
| `anaiero::sdk::lifecycle::SubscriptionPhase` | enum |
| `anaiero::sdk::lifecycle::PinKind` | enum |
| `anaiero::sdk::lifecycle::HistoryAvailability` | enum |
| `anaiero::sdk::lifecycle::MaintenancePhase` | enum |
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
| `anaiero::sdk::lifecycle::Peer` | struct |
| `anaiero::sdk::lifecycle::Subscription` | struct |
| `anaiero::sdk::lifecycle::Pin` | struct |
| `anaiero::sdk::lifecycle::MaintenanceRequest` | struct |
| `anaiero::sdk::lifecycle::MaintenancePlan` | struct |
| `anaiero::sdk::lifecycle::Bootstrap` | struct |
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
| `anaiero::sdk::lifecycle::Service::source` | method |
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
enum class PinKind : std::uint32_t { recovery=0,program,module,operation,reader,backup,operator_pin };
enum class HistoryAvailability : std::uint32_t { retained=0,archived,expired };
enum class MaintenancePhase : std::uint32_t { reviewed=0,activated,completed };

struct Options {
  std::size_t max_active_sources=32,max_metrics=256,max_active_peers=32,
              max_active_subscriptions=32,max_pins=64,max_pin_objects=64;
  std::uint64_t maximum_lease_seconds=30ULL*24ULL*60ULL*60ULL;
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
  Result source(const SourceRef&,Source&) const;
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
| `evidence.hpp` | 142 | `type_identifier: (missing syntax node)` |
| `evidence.hpp` | 142 | `type_identifier: (missing syntax node)` |
| `lifecycle.hpp` | 127 | `type_identifier: (missing syntax node)` |
| `lifecycle.hpp` | 135 | `type_identifier: (missing syntax node)` |
| `lifecycle.hpp` | 182 | `type_identifier: (missing syntax node)` |
