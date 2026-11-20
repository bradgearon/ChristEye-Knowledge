# Anaiero compiled SDK

The SDK is a native C++20 package for Windows and Linux. Its runtime, catalog,
projection, experiment and observer services compile without concrete simulator,
filesystem, process, registry or operating-system headers. Legacy commands use
the explicitly named `anaiero_legacy_compatibility` include graph; that graph is
not exported by the SDK package.

## Build and consume

Build and install the native project with CMake. A separate project needs only
the installation prefix:

```cmake
find_package(Anaiero 1 REQUIRED CONFIG)
target_link_libraries(my_host PRIVATE
  anaiero::anaiero_runtime
  anaiero::anaiero_experiments
  anaiero::anaiero_storage_native)
```

`anaiero_contracts`, `anaiero_catalog`, `anaiero_projection`, `anaiero_runtime`,
`anaiero_experiments` and `anaiero_observers` are compiled libraries. The storage,
native module and built-in engine providers are separate targets. Public SDK
headers live under `anaiero/sdk/`; the old `anaiero/` and `christeye/` implementation
trees are not installed as SDK headers.

`anaiero_mesh_service` includes typed `ReceiverRetentionRequest` and
`ReceiverRetentionPlan` records plus review/status/activate/reclaim operations.
They use the same serialized store lane as delivery, preserve current roots and
immutable ownership/retry identities, and expose explicit expired-history results.
See the [receiver retention guide](continuous-mesh.md#review-and-reclaim-receiver-history)
for bounded pages, overlapping receipts and interruption recovery. Native package
selection is an operator service behind the host commands; it adds no operating
system or package-parser dependency to these domain SDK contracts.

The installed `share/anaiero/examples/independent-sdk` project builds a new engine
without modifying or including Anaiero core sources. It exercises linked, native
module and supervised process delivery. Its counter and deliberately failing
callbacks are conformance fixtures, not scientific models. `anaiero_sdk_conformance`
runs the same fixture in the repository build; `anaiero_sdk_installed_consumer`
installs the complete native build, copies that project to a separate directory,
then independently configures, builds and executes it. The latter also compiles
the C ABI header as C.

## Engine-owned semantics

An `IEngineFactory` constructs an `IEngine` from an explicit `EngineIdentity`, a
schema-bound configuration and an optional retained `EngineState`. A catalog
entry owns its factory and immutable descriptor. Catalog registration rejects
duplicate types, unsupported limits and ambiguous step/change schema names. The
catalog is frozen before activation. It checks actual trial/change capability
pointers and the returned engine's identity and restored state at construction.

`prepare_step` and `prepare_change` produce owned `PreparedTransition` values.
They do not change the visible engine or write a second authoritative state log.
The target revision advances by one; a model change or phase control can retain
the logical tick. Input, checkpoint, configuration, observation and receipt
schemas remain explicit. Observations retain their metric, quantity, unit and
logical tick. A millivolt observation is never reinterpreted as a generic score.
Ordinary steps preserve configuration. A registered change can update its
configuration and checkpoint together, while the engine validates the original
model/source pins and its supported change semantics.

`SessionService` validates the previous-state witness, operation identity,
module/model/source identity, schemas and declared bounds before submitting one
canonical batch. It persists the state, prepared transition, latest engine and
metric records together with optional lifecycle-policy records. Only a committed
completion installs the new engine state and updates the Projector. A failed
post-commit install triggers reconstruction from retained state or reports
`committed_degraded` and stops new work until recovery.

Logical state digests are separate from durable object addresses. A stored
checkpoint envelopes the complete engine identity, including source and epoch,
around the state. Retiring and expiring one source's initial checkpoint therefore
cannot prevent a new source/epoch from using identical logical initial bytes.

An operation retry checks the durable operation receipt before current revision
or finite-completion admission. Repeating a previously committed request does
not run another step. Reusing the operation ID for different bytes conflicts.
Phase controls have a stable request identity independent of the current
revision, so pause/resume/cancel retries remain idempotent after restart.

The owner is serialized and rejects callback reentry. `view()` returns cached
committed state. A pointer returned by `find()` is borrowed, can be invalidated by
recovery/reconstruction, and is intended for isolated capabilities or inspection.
Mutation outside the owner is detected and repaired or stops admission before
another canonical transition. Hosts must serialize the owner, lifecycle service
and direct access to their borrowed store.

## Asynchronous commit and recovery

`step()` is an explicit synchronous convenience for command-line or already
isolated worker use. `begin_step()` submits to an injected `ICommitQueue` and
returns after bounded queue admission; the visible state remains unchanged.
`CommitWorker` reserves completion capacity and signals its wakeup callback.
The host wakes its owner and calls `pump()`, which consumes at most one completion.
There is no hidden polling or sleep loop in the domain service. A host must not
destroy a borrowed store or commit queue while its operation is pending.

`ICommitPolicy::recovery_sources` supplies a bounded active-source catalog.
With the lifecycle policy, recovery point-reads only those engine records and
their active metric prefixes. A host that does not supply this catalog uses a
bounded-memory historical metadata scan; that fallback is not the sustained
lifecycle profile. Retired sources are skipped before their checkpoints are
materialized. `remove_retired()` can release engine/metric roots even after a
restart between source retirement and engine detachment.
Scheduling admission also consults the lifecycle policy: draining/sealed engines
can remain materialized for inspection without blocking active engines' cycles.

## Experiments and measured outcomes

`ExperimentService::select` accepts at most eight candidate changes and a common
horizon of at most 128 inputs. It checks the exact source revision and checkpoint
digest, evaluates one baseline, then independently forks each candidate from the
same source. It retains candidate failures individually and leaves the live
source unchanged. The canonical program digest includes input/change envelopes,
source identity and the score strategy's implementation/configuration pin.

Before calling `IScoreStrategy`, the service aligns final observations by metric
and requires matching schema, quantity, unit and tick. The strategy owns the
domain meaning of the score and eligibility; higher eligible scores are preferred.
The baseline remains selected unless an eligible candidate improves it or the
baseline is ineligible. Candidate ties are ordered by stable candidate ID. A
selection result does not apply a change.

`compare_outcome` compares retained selection evidence with an `OutcomeEvidence`
provided by the host from committed state and observations. It preserves the
same identity, horizon and semantic checks, and separately reports whether the
measured score meets the selected score and whether the complete state is exactly
equal. It does not attest arbitrary caller-supplied data as canonical and does
not automatically reverse or apply a change. Operator services own those actions
and their durable receipts.

## Deferred observers

Runtime and experiment services can enqueue bounded passive events into an
`ObserverService`. The host dispatches them outside the commit owner. A full
observer queue increments its dropped-event counter and cannot roll back a
committed model transition. Observer callback failure and elapsed budget overrun
are different counters; either disables the offending callback. The injected
clock makes timing tests deterministic. `SteadyObserverClock` is a separate
native adapter.

A trusted native callback cannot be forcibly preempted by checking a clock.
Budget overruns are detected when it returns. Put callbacks that require hard
termination boundaries in an owned process and bound their protocol exchanges.

## Native module ABI and process modules

`module_abi.h` defines ABI version 1 using fixed-width integer lengths, a
size/versioned function table, opaque engine handles and caller-owned byte
buffers. Payloads use the bounded canonical ASK1 codecs. No STL type, C++ vtable,
exception or allocation ownership crosses that C boundary. `ModuleApiAdapter`
helps a module author expose the C table while keeping C++ implementation details
inside their own binary. This is an ABI for a compatible operating system,
architecture and calling convention; it is not a universal binary format.

`load_native_module` requires explicit `trusted_native=true`, an absolute artifact
path and its SHA-256 digest. It validates the ABI and mandatory callbacks, rejects
advertised optional capabilities without matching functions and verifies the
descriptor before catalog registration. A file digest is a byte pin, not a trust
grant. Native images can execute arbitrary trusted code. Installations must use
operator-controlled immutable files; the loader does not protect against a
hostile writer modifying the installation while native code is loading/running.

The factory, engine and every isolated fork retain shared ownership of the native
image. The image unloads only after all handles have been destroyed with their
module's own destructor. Providers needed for future checkpoint recovery must
remain available through the operator's installation retention policy. Recovery
rejects changed module/build pins instead of silently substituting code.

`load_process_module` launches the exact pinned executable directly with an
argument vector, never a shell. Each engine owns its process. A cooperating
provider runs `run_module_stdio`; it is not an adapter for arbitrary stdout-based
solver programs. The protocol transports chunks below 16 KiB, verifies complete
payload digests and limits each assembled message to at most 16 MiB. One absolute
deadline covers all exchanges in a command. Expiry/cancellation terminates the
owned child through native Windows/Linux process supervision, and a fresh process
can restore the last committed checkpoint. The supervision boundary does not
claim an OS security sandbox or an enforced child-memory/CPU quota.

## Continuous host and authenticated mesh

`host.hpp` exposes `NativeContinuousHost`, bounded control/status records and
`IStoreTaskExecutor`. `native_session.hpp` provides native composition for an
already initialized canonical modular session. Borrowed runtime/runner/store
objects outlive the host and are never called concurrently outside its ownership
boundary. Accepted storage jobs own their captures, reserve completion capacity
and complete exactly once. Stop closes admission and drains accepted jobs.

`mesh.hpp` is the portable canonical mesh service and cost-aware scheduler.
`native_mesh.hpp` is the optional native authenticated adapter. It submits
storage tasks to the same host lane used by engines, and consumes coalesced
commit availability signals. Native transport and platform headers remain
outside the installed SDK. Link `anaiero::anaiero_mesh_native` only in the
optional mesh profile; `anaiero::anaiero_mesh_service` remains transport-free.

Signal-only wake callbacks must not reenter the host or inspect the borrowed
store. Local administration has a nonblocking submission API with owned result
bytes; its synchronous convenience adapter is for a separate caller, not the
serialized IPC/status handler. Peer/session/grant records do not authorize
remote arbitrary code execution. See [continuous mesh](continuous-mesh.md) and
[CE-SW-39](../../docs/software/CE-SW-39_continuous_native_operation.md) for concrete
host composition, lifecycle, retries and profile limits.

`storage_tools.hpp` also exposes reviewed canonical compaction. Existing
operation identities remain valid when receipts are retained. Explicit receipt
expiry requires an application-owned new intent/operation namespace; fetching
the current epoch and automatically stamping an uncertain retry is incorrect.

## Verification limits

The conformance tests cover canonical codecs, an engine absent from the core,
actual trial/change implementations, finite completion, commit cuts, exact
idempotence, typed outcome mismatches, observer faults/budgets, module lifetime,
ABI rejection, process deadline termination and checkpoint reconstruction.
They are deterministic engineering checks. Sustained resource qualification,
continuous authenticated mesh scheduling and empirical scientific validation
have their own gates and are not implied by passing this SDK suite.

## Versioned checkpoint migrations (CE-SW-43)

An engine may optionally implement `IMigrationCapability` and return it through `IEngine::migrations()`.
This is for **state-bearing checkpoint schema changes within an unchanged engine/module identity**.
It is intentionally separate from ordinary `IChangeCapability` and from package selection.

1. Declare both source and target checkpoint schemas in the registered `EngineDescriptor`.
2. Implement `implementation_sha256()` to return a stable, versioned algorithm fingerprint declared by the module, and
   `prepare_migration()` to calculate a deterministic staged `PreparedTransition`.
3. Pause the engine under the session owner. Build a `MigrationRequest` with the exact expected revision,
   source/target schemas, implementation digest, typed parameters and unique operation ID.
4. Call `SessionService::review_migration(instance,request,plan)`, then
   `SessionService::apply_migration(instance,plan)`. The latter recomputes the candidate and checks
   both checkpoint digests before one canonical guarded commit. Previous and new checkpoint blobs
   remain referenced by the indexed `sdk.engine-migration` receipt.
5. Resume normal engine work after a successful migration, or recover the canonical store before retrying
   the **same** operation ID if the commit result is uncertain.

Migrations must preserve the paused phase, configuration, source identity, and logical tick. The
engine's normal `restore()` and `install()` must accept both checkpoint representations during
the migration transition. Past metrics are not rewritten and the Projector is not an authority.

A source-to-target schema conversion is **not** an engine binary hot swap, publisher signature,
semantic validation of learned ability, or a general downgrade mechanism. For implementation
and an executable v1→v2 example see
[CE-SW-43](../../docs/software/CE-SW-43_versioned_engine_checkpoint_evolution.md).

### Codec and persistence boundaries

The runtime service does not decode canonical engine, metric or migration-receipt
framing. `session_record_codec.hpp` encodes/decodes versioned
`session-engine/1` and `session-metric/1` records, while
`migration_receipt_codec.hpp` handles the binary `engine-migration-receipt/1`
payload. The typed `migration_receipts.hpp` evidence adapter owns
`sdk.engine-migration` record keys, reference validation and generation
lookup. `SessionService` owns migration eligibility, deterministic review,
authorization and commit ordering; it never interprets the stored wire fields.
The operator CLI prints typed migration status as JSON and does not parse
migration receipt fields itself.

No XML is present in this protocol: `pack`/`unpack` are Anaiero's existing
length-prefixed binary framing. A future XML import/export adapter could use
pugixml in infrastructure without changing the canonical store format.

### Canonical integrity and cryptographic ownership

`SessionService` coordinates the active engines, revision checks, commit
sequencing, and the Projector. It no longer computes SHA-256, hashes encoded
checkpoints, assembles canonical records or interprets migration receipts.

`SessionEvidenceService` (`session_evidence_service.hpp`) owns checkpoint
digest verification, canonical session transition records and immutable object
references, deterministic engine registration and phase/detach operation
identifiers, and the source-epoch metric namespace hash.

`MigrationEvidenceService` (`migration_evidence_service.hpp`) owns
canonical migration-request witnesses, source/target checkpoint digest checks,
migration-plan equivalence, transition receipt construction, staged migration
evidence and exact accepted retry verification. The underlying binary codecs
and `migration_receipts` adapter remain responsible for versioned encoding and
retained-record referential integrity.

Both are compiled, native, typed evidence services; neither owns a thread,
changes the independent Lotus authority boundary, or introduces a new source
of truth. Both use Anaiero's existing SHA-256 implementation and sealed
canonical store, preserving the exact established wire formats and operation
identity digests. They do not create digital signatures or prove publisher
authenticity.
