# Native implementation checkpoint

This index records operational integration separately from component existence.
Verify the current engineering branch and CI when continuing work.

The [native readiness and development plan](native_development_plan.md) records
the 2026-11-17 assessment, commit-pinned evidence, priority order and completion
gates. This checkpoint describes implemented behavior; the plan distinguishes
delivered capabilities from remaining qualification.

[CE-SW-38](CE-SW-38_compiled_runtime_and_lifecycle.md) implements M1–M4 of
[CE-SW-37](CE-SW-37_modular_runtime_and_evidence_lifecycle.md) through the new
compiled modular session path. Continuous mesh integration (M5), sustained
release qualification (M6), and independent scientific evidence have separate
gates. [CE-SW-39](CE-SW-39_continuous_native_operation.md) now supplies the
continuous canonical host/mesh path, reviewed compaction and package/profile
tooling. Long-duration release qualification remains open.
The [CE-SW-40 continuation](CE-SW-40_operating_envelope_and_package_lifecycle.md)
adds measured mixed-host operation, explicit receiver retention, conservative
package activation/rollback and reduced derived-index work.

## Standing delivery preference

The user requests roughly **5× the previous incremental scope per changeset**.
Deliver a substantial complete capability with implementation, adapters,
runtime/CLI wiring, meaningful Windows/Linux tests and documentation. Keep
related work in one changeset with reviewable internal commits.

## Current integrated batch — CE-SW-40 operating envelope and lifecycle

`anaiero qualify mixed PROFILE SESSION NEW_OUTPUT` runs all five compiled engine
families through the actual continuous host. It checks durable pause/retry/resume,
bounded storage/control pressure, historical payload/receipt reads, declared
quota rejection, exact orderly restart and full audit. Exact inputs and a report
are retained. The short profile is executable CI evidence; the supplied 24-hour
and seven-day profiles require their full elapsed durations before qualification.

Local `receiver-retention-review`, `receiver-retention-status`,
`receiver-retention-activate` and `receiver-retention-reclaim` commands expire
explicit received history before deleting it. Whole remaining page members,
additional overlapping receipts, artifacts and obsolete bootstrap checkpoints
have bounded selection. Current cursor/projection/pin roots remain protective;
old reads/retries report history expiry. Both receiver and sender maintenance
resume missing-file byte accounting after an interrupted deletion.

`anaiero host package-check`, `package-review`, `package-rollback-review`,
`package-activate`, `package-status` and `check-selected` supply the
[native package lifecycle](../../deployment/native/package-lifecycle.md).
Canonical selection and retained host/manifest witnesses commit together.
Rollback reviews the current store and preserves the accepted suffix. This
matrix requires unchanged compiled engine identity and an explicit stopped
supervisor switch; different runtime/engine implementations need migration.

The derived index avoids filesystem checks for discarded transient nodes and
coalesces source-tip updates within a canonical transaction. Formats, durability
order and qualification budgets remain unchanged. The
[completed Windows 100k failure](../../qualification/reports/2026-11-18-hosted-windows-100k/README.md)
is preserved; new implementation measurements are separate evidence.

The [retained operating-envelope record](../../qualification/reports/2026-11-18-operating-envelope/README.md)
includes original CI archives, reports, actual checkout identities and raw build
manifests with explicit LF identity witnesses where Windows wrote CRLF bytes.
At runtime `aec1489d`, full native suites pass 81/81 and real TLS/IPC mesh suites
pass 9/9 on both platforms. PR-triggered 100k profiles pass on Windows and Linux,
while an independent push Linux run fails its conservative p99 bound (2.147 seconds
against the unchanged 2-second limit). Its audit, recovery, memory and disk gates
pass. The measurement method is explicit in new reports, and the failed result
remains unchanged. This completes the implementation/evidence batch while leaving
hosted tail-latency qualification and prolonged release qualification open.

## Previous integrated batch — CE-SW-39 continuous native operation

`anaiero host` runs the canonical modular session continuously with monotonic
wall pacing, bounded control/storage queues, cached status and native IPC/service
lifecycle. Engine clocks keep their original meaning. Accepted durable work
drains on stop; a killed process recovers the original pending cycle identities.
The host retains its executable separately from the session CLI's engine package.

The optional `anaiero mesh-live` / CEVMESH/2 path shares that host's serialized
storage lane. Bounded concurrent TLS admission, commit watermarks, one-page
credit and byte/work scheduling support live subscribers without per-peer
unbounded event buffers. Scoped grants, current credential generations, exact
offers/ACKs, relay receipts, verified artifacts and explicit bootstrap are
canonical operations. Local asynchronous mesh administration keeps status and
stop available while a grant or revocation waits for storage.

`session compact-review`, `compact` and `compact-resume` publish a verified
canonical base before removing redundant journal/index history. Repair and
audit use that base plus the retained suffix. Exact retry receipts remain by
default; explicit receipt expiry advances a fenced namespace and is rejected
for existing modular sessions. Latest ownership/source/pin/tombstone identities
remain retained and consume finite quota.

Native CMake package tooling installs and verifies exact file inventories and
retained implementation compatibility. The native qualification executable
runs predeclared evidence/churn profiles and records latency, lookup/reopen
work, memory, CPU and both serialized/disk measurements. The
[profile guide](../../qualification/native/README.md) distinguishes this
synthetic workload from mixed daemon qualification and identifies the remaining
24-hour/seven-day and device-specific gates. Exact CI reports accompany the
reviewed commit; architecture ratings do not replace them.

## Previous integrated batch — CE-SW-38 compiled runtime and lifecycle

`anaiero session` (full build) and `ce_modular_session` (controller profile) run
five registered engine families through a common canonical commit store. The
new `ANAIEROSESSION/1` example runs real SW5 cells, SIM3 colonies, a finite VLM
plan, passive voltage and scalar environment together. The VLM completes at
tick 12 while the other engines continue. Its original image and receipts match
the existing standalone interpreter, including recovery after every plan tick.

The installed SDK separates compiled contracts, catalog, runtime, projection,
experiments, observers and concrete adapters. An independently built additional
engine uses only installed headers/libraries and exercises linked, native C ABI
and supervised process admission. See the [SDK guide](../../native/docs/module-sdk.md)
and [generated public API reference](../../native/docs/sdk-reference.md).

State/configuration, observations, receipts, source tips and active lifecycle
checkpoints commit together. The bounded writer signals completion; model state
and the Projector advance after commitment. Stable operation IDs and persisted
cycle intents finish interrupted work once. Registered changes support actual
SW5 routes and SIM3 link units/future teaching, including explicit cancellation
before teaching executes. Recorded experiments retain their original source,
program, strategy, result and measured outcome independently of the Projector.

The native sealed AVL index provides historical operation, record, observation,
event and blob lookup with verified coverage and explicit repair. Legacy
Registry/segmented evidence migrates through a locked snapshot and staged
verified activation. This is evidence migration; legacy live session state
continues to use its compatible runtime until explicitly migrated.

Sources drain, seal and retire without filling active memory with historical
epochs. Peer/subscription generations, expiry, exact offers/ACKs, protected pins,
archive availability and replay floors use the same canonical store. Native
restore verifies a quiescent copy, publishes quarantine, invalidates restored
grants on explicit reauthorization, and requires a fresh source incarnation.
See [storage](../../native/docs/canonical-evidence.md) and
[lifecycle](../../native/docs/modular-lifecycle.md) for bounded policies.

The store reclaims expired payload files and reserves maintenance headroom.
At CE-SW-38, historical canonical metadata and identity fences remained under
a finite accounted-byte budget without prefix pruning. CE-SW-39 adds the
reviewed canonical-base path above. Protected current identities still consume
finite quota, distinct from allocated filesystem blocks. Separate archives and
backups have their own budgets; no indefinite finite-disk claim follows.

The Windows/Linux `native-modular.yml` gate builds the complete native
distribution, runs existing and new regression suites, installs and exercises
the independent SDK consumer, and builds a controller profile without Python
discovery. Exact commit/run evidence is recorded with the implementation PR.

## Previous integrated batch — CE-SW-36 environment and calibration

The native controller now runs a second scientific quantity: a fixed-volume,
well-mixed scalar concentration model with an explicit simulated nanosecond
clock and micromolar output. Shared observable contracts preserve voltage
semantics while adding concentration-specific schemas, metrics and views.
Both scientific providers run beside SW5/SIM3 through the existing supervised
process, checkpoint and durable publication paths. JSON views include each
engine's model, behavior, source, epoch, clock and quantity unit.

`ce_scientific_config environment` prepares a pinned native provider and session.
`ce_scientific_calibrate` (or `anaiero calibrate` in the full build) loads sealed
reference artifacts, chooses from bounded model candidates using training data
only, freezes the selected model, and evaluates a separate declared held-out run.
Reports retain declared measurement/reference provenance, raw errors and
uncertainty separately, and require both training and holdout criteria to pass.
They are published as complete new files without replacing existing reports.

The included reference traces are synthetic numerical fixtures. External
read-only data remain explicitly unverified even when their error thresholds
pass. Scientific live optimization and application remain disabled. This is a
generic scalar environment, not a calibrated biological or spatial model.

See [CE-SW-36](CE-SW-36_environment_and_calibration.md) for equations, native
Bash/PowerShell commands, bounds, evidence classifications and acceptance tests.
The new native suite includes independent numerical comparison, a million-step
equilibrium regression, exact process checkpoint continuation, four-engine
mixed-session restart, interrupted evidence recovery, mesh codec preservation,
frozen held-out selection and report/artifact tampering checks.

## Previous integrated batch — CE-SW-35 authenticated evidence mesh

The optional native mesh now provides OpenSSL 3 TLS 1.3 mutual certificate
authentication, explicit certificate-pinned operator enrollment, source scopes,
expiry/revocation/rotation, durable subscriptions, reconnect reconciliation,
and bounded immutable artifact transfer on Windows and Linux. Its dedicated
owner serializes storage, projection, replay and local administration; up to
eight transport workers feed a bounded sixteen-request queue.

Ingress commits an immutable provenance receipt before observation publication
and projects only committed evidence. Startup verifies checkpoint tips and tail
observations against their receipts. Local native-session imports are explicitly
labeled separately from TLS delivery. The offline bridge locks the source
session stores; the mesh never reads an active session worker's Registry.

`ce_mesh serve` exposes evidence operations remotely and a same-user local
administrative endpoint. `serve-stdio` provides a caller-owned local control
pipe. `sync` and `push` reconcile durable subscriptions on explicit connection;
artifact transfers verify the full digest before publication. Native session
commands, experiments and solver launches remain on their existing local path.

Use `-DANAIERO_ENABLE_MESH=ON` for the optional dependency. See
[CE-SW-35](CE-SW-35_authenticated_native_mesh.md) and the
[two-node guide](../../examples/mesh/README.md). The default controller profile
continues to build without OpenSSL or Python discovery.

## Integrated session foundation — CE-SW-32, CE-SW-33 and CE-SW-34

| Capability | Component | Integrated controller |
|---|---|---|
| SW5 + SIM3 + scientific lifecycle | Compiled factory bindings | Configured mixed-cadence session with persisted engine contracts |
| Partial-cycle retries | Corrected and regression-tested | Persistent intent + per-step continuation |
| Async observation evidence | Bounded worker + ordered service | Separate journal owner, completion wakeups, commit-before-projection |
| Checkpoints and restart | Model-pinned CAS catalog | Session manifest, durable source identities, replay and validated continuation |
| Reusable experiments | Immutable sealed plans, real SW5/SIM3 trials, one baseline and source | Resumable receipts, named/hash lookup, staged SW5 apply and measured outcome decisions |
| Durable operations | Persisted intent and 64 retained results | Stable IDs, duplicate/conflict/expiry handling, explicit RESULT and cancellation recovery |
| Local operator control | Same-user IPC | Cached typed views, bounded commands, revision WATCH, EXPERIMENT_STATE and correlated TRACE |
| Process/service lifecycle | Native Windows/Linux adapters | Console signals, Windows SCM entry, systemd notify and manual deployment examples |
| Sustained storage | Segmented journals, fixed bucket indexes, reference roots, archives | Snapshot + tail recovery, cached admission headroom, verified retention and offline maintenance |
| Scientific process | Native Windows/Linux owned child and bounded exchange | Pinned identity, physical-time voltage, full checkpoint restore and native analytic provider |
| Actual external model | Optional pinned NEURON 9.0.1 | Passive compartment qualification and exact fresh-process checkpoint continuation on Linux |

See [CE-SW-32](CE-SW-32_reusable_native_experiments.md),
[CE-SW-33](CE-SW-33_sustained_storage.md), and
[CE-SW-34](CE-SW-34_supervised_scientific_sessions.md) for protocols, commands,
acceptance tests and exact limits. They extend the integrated session in
[CE-SW-31](CE-SW-31_integrated_native_sessions.md), built on PRs #37–41. Review
corrections on the PR #42 foundation fixed SIM3 live-cell counts, optimizer
source/baseline identity and conservation quantity context.

`CEVSESSION/1` remains supported. The supplied
`examples/runtime/mixed-sustained.cevsession` selects segmented storage and
declared logical/physical time through version 2. `ANAIERO_CONTROLLER_ONLY`
builds the session controller, native scientific provider and setup utility
without the legacy/demo CLI collection or Python discovery.

The latest tests cover more than 4,096 events, bounded resident tails,
rotation/archive crash cuts, projection snapshot recovery, operation restart
windows, trial cancellation, interrupted automatic apply, exact staged live
outcomes and scientific session restart. The optional NEURON report checks
200 samples, a 7.60636701136 µV maximum analytic difference within a 41 µV
numerical envelope, and 160 identical restored continuation samples.

## Deliberate remaining limits

- The new canonical/lifecycle services are integrated into the modular session.
  The existing CE-SW-35 authenticated mesh remains on its compatibility path.
  M5 must connect continuous transport, replay, byte/work fairness and artifact
  scheduling to these services; connection-triggered sync is still the current
  mesh behavior.
- The new native index replaces the proposed required SQLite adapter. Historical
  transaction metadata is retained; payload reclamation and finite admission
  quotas are implemented, but canonical journal-prefix pruning is not.
- Legacy evidence migration preserves opaque provenance and original sealed
  event bytes. It does not automatically upgrade live CEVSESSION/2 engine state.
- The modular operator uses CLI commands. Existing IPC/SCM/systemd control
  remains provided by the established runtime; a long-running modular mesh
  owner and deployment installation are separate integration work.
- Runtime commitment is per engine, with recoverable cycle intent, not an
  all-engine atomic snapshot. VLM plans are bounded finite executions; arbitrary
  program variants are not optimization candidates.
- SIM3 supports the named digital changes and coherent recovery. Its model
  ceiling remains 10,000 ticks. Executed teaching cannot be undone by cancelling
  its old schedule.
- Scientific quantities remain modeled passive voltage and scalar concentration.
  Independent biological calibration, scientific trial/application capability,
  spatial environment dynamics and physical acquisition remain separate.
- API/package compatibility and the native failure suites are useful engineering
  evidence. Deployment hardware, filesystem power-loss behavior, prolonged
  workloads and representative operational resource bounds still need M6.

## Next substantial capabilities

1. **M5 — Continuous evidence mesh:** bind the new canonical/index/lifecycle
   services into a long-running native owner; commit-triggered delivery,
   bounded handshake/artifact work, byte/work fairness, live revocation,
   explicit expired/archive/rebootstrap results and multi-peer recovery.
2. **M6 — Qualified native release:** declare supported workload profiles,
   measure RSS/disk/lookup/startup/lag and shutdown behavior, validate packages
   and retained dependencies, run both-platform fault/restore and sustained
   tests, and publish exact evidence. Add canonical prefix-pruning only with
   verified authority summaries and compatibility/recovery proof; finite quota
   stop remains the supported policy until then.

The detailed dependency and acceptance gates remain in the
[development plan](native_development_plan.md#revised-delivery-order).
Scientific model/reference qualification remains an independent evidence
workstream. The Projector stays observational, optimization stays independent,
and Windows and Linux remain first-class native hosts.
