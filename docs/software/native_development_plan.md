# Native readiness and development plan

**Current implementation update:** 2026-11-18 (America/Chicago).

**Original baseline review:** 2026-11-17 (America/Chicago).

**Historical assessment baseline:** [`c5a4e8a`](https://github.com/bradgearon/ChristEye-Knowledge/commit/c5a4e8a04616c76decab52d237de53a3f3b09f23), through CE-SW-36.

**Purpose:** assess implemented native software, identify the next usable
capabilities, and make their completion criteria explicit.

**Architecture amendment — 2026-11-18:** [CE-SW-37](CE-SW-37_modular_runtime_and_evidence_lifecycle.md)
now defines the complete module, capability, commit/index, lifecycle and
continuous-mesh architecture. Its M1–M6 sequence below supersedes the earlier
five-item delivery order. [CE-SW-38](CE-SW-38_compiled_runtime_and_lifecycle.md)
now implements M1–M4 in the compiled modular session. The older ratings and
CI table below remain a historical CE-SW-36 comparison, not current release proof.

**Operational continuation — 2026-11-18:**
[CE-SW-39](CE-SW-39_continuous_native_operation.md) delivers continuous canonical
mesh, reviewed canonical compaction, a continuous native host/package path and
measured operating profiles. These are the next four concrete deliveries under
M5/M6; the long-duration release gates remain separate.

**Operating-envelope continuation — 2026-11-18:**
[CE-SW-40](CE-SW-40_operating_envelope_and_package_lifecycle.md) reduces avoidable
index work, adds a real five-engine continuous-host qualification path, supplies
reviewed receiver payload/receipt retention and implements canonical package
selection with suffix-preserving rollback. These close specific operational
paths within M4/M5/M6; elapsed release evidence and semantic engine migration
remain explicit work.

Use the [implementation checkpoint](native_implementation_status.md) for what
currently runs. This document tracks CE-SW-37's forward order and retains the
readiness evidence and detailed capability subplans. Historical
CE-SW specifications retain the scope and limitations of their original
increments; their old TODOs are not the current backlog. The
[project phase roadmap](../roadmap.md) separately records scientific and
physical qualification.

**Runtime lifecycle continuation — 2026-11-18:**
[CE-SW-41](CE-SW-41_runtime_lifecycle_and_qualification.md) implements the next four operational batches.
Use its [qualification record](../../qualification/reports/2026-11-18-runtime-lifecycle/README.md)
to distinguish actual measurements from available profiles. The next acceptance
work is the million-row device matrix, elapsed campaigns, and per-OS
full-volume and supervisor evidence. No rating increase is implied by adding
the runners or by passing a short integration test.

### Next storage decision after the frozen 10k run

The [frozen run](../../qualification/reports/2026-11-18-runtime-lifecycle/synthetic-10k-frozen/report.json)
completed its audit and all 10,000 observations, but 4 of 159 commits exceeded
two seconds when the predeclared gate allowed only one. Of 101.04 seconds
inside commits, the derived index used 61.37 and blob publication used 33.14.
Aggregate stage times cannot identify which stage caused an individual slow
commit. The two million-row admissions also exposed a layout constraint that
free bytes alone cannot solve.

1. Replay the **same executable and profile** on a declared idle device and
   under one prescribed background I/O load. Keep a bounded sample pairing
   each slow batch with its stage times. Record free bytes/nodes, filesystem,
   flush policy and scheduler/device observations. Preserve every result.
2. Prototype packed pages for the **derived index only**. Keep canonical
   state, receipts and existing ROOT semantics. Build the alternative from
   the same retained inputs in a new directory, verify complete sorted
   coverage and checksums, then switch only after the new index is durable.
3. Compare the two layouts on the same volume in a predeclared alternating
   order, then repeat on the second target platform. Measure exact exceedance
   counts, lookup/reopen costs, resident memory, regular/directory allocation
   and **peak live file nodes while both indexes coexist**. Never change a
   threshold after observing the result.
4. Require an interrupted build/switch/cleanup, damaged-page recovery, old-store
   open, full audit and exact retry with pins/expiry before considering
   promotion. Run 100k and 1m at their original budgets on volumes that meet
   the relevant headroom checks. A capacity refusal is evidence for sizing,
   never a successful scale run.

## Current assessment after the continuous-operation changes

**Approximately 8.5/10 as a native engineering research platform**, as an
engineering review judgment rather than a measured completion percentage.
Extension/reuse and integrated plan execution move from material capability
gaps to implemented paths with installed-consumer, equivalence and recovery
fixtures. Continuous operation now has a canonical host/mesh path and explicit
history compaction. Sustained deployment evidence remains the limiting category;
the overall rating stays approximately 8.5 until representative longer runs and
independent application adoption support a higher judgment.

| Area | Current judgment | Implemented improvement | Remaining evidence |
|---|---:|---|---|
| Extension and library reuse | 8.5/10 | Compiled SDK, typed catalog, native/process admission, independent installed consumer | Independent application adoption and supported ABI/package upgrade matrix |
| Integrated plans and digital experiments | 8.5/10 | Real retained VLM execution; coherent SW5/SIM3 changes; frozen selection and canonical outcomes | Broader declared workloads and variant/scientific capability qualification |
| Evidence and recovery | 8.5/10 | Single canonical commit; verified historical index; reviewed base/suffix compaction; explicit retry-history floors; migration and process/fault recovery | Filesystem/power-loss qualification and the dedicated million-observation profile |
| Source/retention operation | 8/10 | Reusable active slots; indexed history; leases/archive/expiry; canonical prefix reclamation; continuous host/mesh binding | Mixed 24-hour/seven-day deployment evidence and policy for arbitrary retained-identity growth |
| Documentation and SDK entry points | 8.5/10 | Current host/mesh/compaction guides, installed SDK example, package verifier and generated public reference | Supported package upgrade matrix and evidence for each shipped deployment profile |

CE-SW-40 adds bounded receiver observation/receipt/artifact/bootstrap reclamation
and a tested package-selection matrix for unchanged compiled engine identity.
The mixed qualification executable now measures the actual five-engine host;
its short profile does not replace the elapsed 24-hour/seven-day gates. The
assessment remains approximately **8.5/10** until the declared sustained
envelopes and independent adoption are demonstrated.

The numerical scope retains its prior evidence. CE-SW-39 binds the new
authenticated mesh to the modular session rather than inferring integration
from component existence. Biological validity remains unestablished. These
ratings do not assert M6 release qualification or replace exact Windows/Linux
CI reports, measured operating profiles or hardware-specific evidence.

The first completed [shared-sandbox 100,000-observation preflight](../../qualification/reports/2026-11-18-shared-sandbox-100k/README.md)
passed its recovery, audit, memory, disk and indexed-read gates but failed the
unchanged batch-commit p99 upper-bound budget (2.147 seconds against 2 seconds).
That negative result remains recorded separately from hosted CI qualification;
it reinforces the need to declare and measure each deployment's operating limits.
The [hosted Windows run at `c196e56`](../../qualification/reports/2026-11-18-hosted-windows-100k/README.md)
also completed and failed only that same unchanged commit-p99 upper-bound gate.
Its report is retained independently of subsequent optimization runs. All its
correctness, audit, memory, disk and lookup gates passed; none of those outcomes
makes the failed latency gate a pass.

The [CE-SW-40 measurement record](../../qualification/reports/2026-11-18-operating-envelope/README.md)
now retains the completed hosted runs, including every observed failure, with
original archives and exact source/profile witnesses. Runtime `aec1489d` passed
the full distribution suites (81/81 on each platform), real TLS/IPC mesh suites
(9/9 on each), and its PR-triggered 100k profile on both Windows and Linux.
An independent push-triggered Linux run of the same implementation failed the
unchanged commit-p99 upper-bound gate: 2.147483648 seconds against 2 seconds,
with an exact maximum of 3.543940010 seconds. Correctness, audit, recovery,
memory and disk gates passed. The logarithmic bound does not reveal the exact
p99 within that bucket, and the cause of the elapsed-time variation is not yet
established. Hosted latency qualification therefore remains **unresolved**.
The versioned observed-maximum cap tightens a conservative estimate without
reclassifying prior reports; it is not itself a storage speed improvement.

## Historical CE-SW-36 assessment

**Overall: approximately 8/10 as a native engineering research platform.**
The service boundaries, durable evidence, mixed-engine sessions and failure
handling form a credible integrated foundation. Executable plan coverage,
source and archive lifecycle, deployment measurements and independent
scientific evidence remain the main constraints.

These are review judgments, not measured coverage, confidence probabilities,
or a percentage of the eventual Anaiero vision completed. A score of 5–6 means
a useful implementation with material integration or lifecycle gaps; 7–8 means
an integrated capability with meaningful failure tests and explicit bounds;
9 requires representative operational evidence and use by independent
consumers. The scores are not averaged into a biological-validity claim.

| Area | Rating | Implemented strength | Evidence needed to advance |
|---|---:|---|---|
| Service architecture and ownership | 8.5/10 | Passive state, typed ports, separate domain and infrastructure responsibilities, observational Projector | Keep new plan semantics behind services; reduce concrete engine assumptions where a second implementation needs the same contract |
| Native Windows/Linux integration | 8.5/10 | Native IPC, service/process adapters, mixed sessions, controller-only build and hosted tests on both platforms | Measured installation, shutdown, recovery and resource behavior on declared deployment hardware |
| Durable evidence and recovery | 8.5/10 | Recorded cycle intent, checkpoint continuation, commit-before-projection, durable operations and mesh receipt verification | Broader maintenance crash cuts, source retirement and documented filesystem/power-loss qualification |
| Extension and library reuse | 7/10 | Common engine lifecycle, explicit capabilities, evaluator/strategy ports and bounded observer hooks | Independently consumable compiled modules and a second consumer; current factories remain a compiled registry |
| Integrated digital plans and experiments | 6.5/10 | Actual isolated SW5/SIM3 trials, immutable source context, shared baselines, resumable receipts and SW5 outcomes | Execute pinned plans through shared sessions and isolated replay qualification; add variant evaluation and SIM3 live changes only through explicit recoverable contracts |
| Numerical implementation in its supported scope | 8/10 | Passive voltage and scalar environment models, stable stepping, full checkpoints, numerical comparisons and frozen calibration selection | Typed scientific input schedules, isolated provider trials, richer quantity meaning and independently reviewed reference extraction |
| Authenticated evidence mesh | 7/10 | Explicit enrollment, mutual TLS, scoped streams, durable replay, receipts and verified artifacts | Indexed historical receipts, lifecycle/lag qualification, sustained fan-out and deployment key management |
| Sustained operational scale | 5/10 | Bounded active memory, segmented journals, admission checks, snapshot/tail recovery and maintenance commands | Bounded active-source lifecycle, archive policy, efficient historical lookup and reproducible long-workload measurements |
| Documentation and developer entry points at the baseline | 6.5/10 | Detailed increment specifications and runnable native examples | One current capability map, accurate full/lean command profiles, browsable API reference and acceptance-driven roadmap |

**Empirical biological validity: not established.** The current numerical
fixtures and synthetic learning/resource proxies do not demonstrate biological
learning, retention, transfer or model validity. Raw artifact hashes and a
passing holdout threshold establish software evidence properties within the
declared protocol. They do not authenticate a measurement or establish
experimental independence. See [CE-SW-36](CE-SW-36_environment_and_calibration.md)
and the [reference producer declaration](../../examples/scientific/calibration/analytic-reference-method.txt).

The documentation score describes the reviewed implementation baseline.
This plan and the current-status corrections address navigation/status drift;
they do not increase runtime or scientific capability.

## Verified baseline and limits of the evidence

The baseline's nineteen pull-request GitHub Actions workflows completed
successfully. Relevant results include:

| Evidence | Observed result |
|---|---|
| [Full native suite](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44242654531) | 61/61 tests on Linux and 61/61 on Windows |
| [Environment and calibration](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44242654566) | 4/4 targeted tests on each native platform, including controller-only builds without Python discovery |
| [Integrated sessions](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44242654693) | 12/12 on each native platform; separate optional NEURON qualification on Linux |
| [Authenticated mesh](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44242654475) | 6/6 on each native platform with real TLS required |
| [Architecture guard](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44242654437) | 10/10 on each native platform |

These suites overlap; their counts must not be summed as independent coverage.
They are evidence for the pinned implementation, not for a future merge or
deployment. The separate Cloudflare Workers check was failing at this
baseline; native Actions success does not establish website deployment health.
Check the actual target commit and hosting status before a release.

The assessment inspected the implementation and failure tests. It did not
perform a new long-duration load run, independent security audit or physical
experiment. No new correctness defect requiring an immediate hotfix was
identified in the sampled paths; the priorities below address demonstrated
capability and scale gaps.

## Original findings and their disposition

The findings below describe the reviewed CE-SW-36 paths. CE-SW-38 closes the
compiled-module, shared-session VLM, supported SIM3 live-change and modular
source-lifecycle gaps through a new versioned path. CE-SW-39 supplies the M5
canonical mesh integration; legacy mesh callers retain their original versioned
path until explicitly adopted. The details remain useful compatibility context.

### Existing plans execute, but not through the integrated experiment path

The legacy [CEVPLAN Session](../../native/include/christeye/living/learning_plan.hpp)
already executes stages against CEVLM/2. In contrast, the
[experiment definition](../../native/include/anaiero/contracts/experiment.hpp)
and [artifact loader](../../native/include/anaiero/infrastructure/experiment_definition_file.hpp)
validate and pin those references as provenance. They do not execute the
referenced VLM program in the common session/trial lifecycle.

The shared [step input record](../../native/include/anaiero/contracts/simulation_engine.hpp)
currently carries SW5 support, stress, signal and lesson fields.
[SIM3](../../native/include/anaiero/application/sim3_simulation_adapter.hpp) and
[scientific engines](../../native/include/anaiero/services/scientific_engine_service.hpp)
reject those fields; scientific stepping uses its configured constant input.
Mixed scheduling and shared lifecycle therefore do not imply general
model-specific input programs.

The next capability must execute a supported pinned program and retain its
progress. Merely adding another provenance field would not close this gap.
Mappings must preserve each executor's semantics; VLM RNA/support proxies
cannot silently become SW5 inputs or physical quantities.

### SIM3 evaluation is ahead of recoverable live mutation

[SIM3 experiment policies](../../native/include/anaiero/application/sim3_experiment_adapter.hpp)
change an existing link's units or add a synthetic teach action inside an
isolated trial. The live adapter does not support arbitrary checkpoint restore
or live apply, and [experiment validation](../../native/include/anaiero/services/experiment_service.hpp)
rejects automatic SIM3 apply/revert.

Configuration changes and state checkpoints must recover as one coherent
identity before those capabilities are enabled. SIM3 retains a 10,000-tick
limit. Plans must preflight their remaining horizon; raising that constant is
not a substitute for state, storage and recovery work.

### Historical lookup cost is a distinct operational gap

[`SegmentedStore::find_event`](../../native/include/christeye/living/segmented_store.hpp)
uses a latest-stream index but falls back to scanning retained events for an
older tick. [Receipt verification](../../native/include/anaiero/infrastructure/mesh_ingress_store.hpp)
uses that lookup. A [full mesh audit](../../native/include/anaiero/services/mesh_recovery_service.hpp)
verifies each observation, so repeated historical receipt searches can produce
quadratic total work as retained history grows.

This is an inference from the current call paths, not a measured latency
result or a demonstrated recovery error. Direct lookup and measurement should
precede a substantial increase in mesh fan-out.

### Active-source bounds need a lifecycle

The [Projector state](../../native/include/anaiero/models/colony_state.hpp)
allows 32 source/epoch cursors and 256 metric slots.
[Old epochs become stale](../../native/include/anaiero/services/colony_projector.hpp)
but remain resident. Repeated rollover therefore consumes finite active
capacity. Retirement must preserve historical evidence while allowing active
materialization slots to be reclaimed.

Segmented storage bounds its resident tail, not retained disk usage. Its
64 index buckets each allow 256 latest-stream entries; a bucket may fill before
the theoretical total. Archives preserve retained evidence, and SIM3 retains
tick snapshots. These need explicit lifecycle and capacity policies.

### Source boundaries are useful; binary module boundaries remain work

The [architecture guard](../../tests/test_native_architecture.py) enumerates
legacy bridges and forbids new reverse-layer dependencies.
The principal [CMake layer targets](../../native/CMakeLists.txt) are still
`INTERFACE` libraries sharing an include root. Engine factories are compiled
bindings, and extension callbacks are trusted synchronous observers under a
nonblocking/nonreentrant contract.

Keep the lifecycle interface narrow. CE-SW-37 now makes compiled modules and
an installed-SDK consumer the first implementation batch. Its native module
ABI is scoped to trusted installed extensions; it does not require a universal
model abstraction or claim an in-process callback sandbox. Built-in static
modules retain the compact standard deployment.

## Revised delivery order

Each batch is approximately five related increments delivered as one usable
capability, including services, adapters, operator wiring, failure tests,
native Windows/Linux verification and documentation. Internal commits may be
reviewed separately. M1–M4 are implemented in CE-SW-38; CE-SW-39 implements the
continuous operational path and M6's package/profile tooling. Sustained release
qualification remains an explicit gate.

| Order | Architecture batch | Usable result | Dependency |
|---|---|---|---|
| M1 — implemented | Compiled modules and registered capabilities | Installed public SDK, private implementations and independently added engines using typed capability bindings | Current native implementation |
| M2 — implemented | Canonical transitions and indexed evidence | One recoverable commit/root contract, asynchronous storage and verified historical indexes | M1 contracts |
| M3 — implemented | Complete engine execution and change paths | Finite durable VLM plans and engine-owned SW5/SIM3 trial/change paths with configuration/state recovery | M1 and M2 |
| M4 — implemented | Source, peer, subscription and retention lifecycle | Reusable active capacity, versioned checkpoints, archive policy, leases and explicit replay floors | M1 and M2; can proceed alongside M3 |
| M5 — implemented operational path | Continuous mesh service | Canonical shared-host live delivery, bounded TLS admission, byte/work fairness, scoped lifecycle/artifacts/rebootstrap and native multi-peer fixtures | M2 and M4; CE-SW-39 |
| M6 — tooling and functional evidence; release gate open | Qualified native release | Native host/service packages, installed integrity/compatibility checks, process/fault matrix and measured profiles; prolonged deployment qualification still required | Integration of preceding batches |

See [CE-SW-37's delivery sequence](CE-SW-37_modular_runtime_and_evidence_lifecycle.md#14-delivery-sequence)
for approximately five related increments per batch and exact closure gates.
The four-delivery CE-SW-39 changeset connects transport to canonical sessions,
adds authority-preserving prefix compaction, makes the session continuously
operable and supplies executable qualification/package gates. The next closure
work is the dedicated M6 operating envelope: mixed sustained workloads,
declared device profiles, longer history scale, actual pressure/crash behavior
and supported upgrade/rollback procedures. Production readiness is not inferred
from the new architecture or a passing short test.

### Next closure work after CE-SW-40

| Order | Concrete work | Completion evidence |
|---|---|---|
| 1 | Resolve the remaining 100k latency uncertainty with exact counts above predeclared limits and timings for durability stages, then qualify a declared device/filesystem profile | Preserve the existing passing and failing runs; distinguish a coarse percentile-bound miss from actual threshold exceedances; declare measurement changes and budgets before execution; retain exact source/profile/device witnesses |
| 2 | Run the existing one-million-observation profile on a declared dedicated device and compare 10k/100k/1m index and allocation costs | Completed report plus device/filesystem/dependency declaration; retained failure evidence if a gate is missed |
| 3 | Run the new elapsed mixed 24-hour/seven-day profiles, then add real TLS peer lag/churn/rotation and actual storage pressure to that operating envelope | Full-duration native reports on both platforms; separate dispatch/commit/projection/remote-ACK measurements and failure classifications |
| 4 | Extend the conservative package matrix through explicit engine/runtime state migrations and qualify supervisor installation/uninstallation; remove bootstrap producer quiescence only with a pinned continuation contract | Preserved accepted suffix, exact old/new implementation evidence, migration/crash/rollback fixtures and deployment-device qualification |

The initial optimized 100k Windows/Linux executions are complete; their mixed
hosted outcomes determine the first item above. Further runs need a declared
diagnostic or device-qualification purpose, and every outcome remains evidence.
Elapsed/CPU differences alone do not identify a cause, and the passing subset
does not qualify the operating envelope. The one-million and prolonged profiles
remain subsequent evidence gates.

CE-SW-40 already implements complete-page receiver expiry, explicit cleanup of
overlapping receipt copies, protected current cursors/projections, artifact and
superseded-checkpoint reclamation, and missing-file accounting recovery. Permanent
ownership, retry, tombstone and selected-package witnesses still consume finite
metadata quota. Automatic deletion of those witnesses is not an implied policy.

Implementation decisions are explicit: a sealed native AVL backend replaces
the recommended SQLite dependency; old evidence migrates without pretending to
upgrade old live session state; verified canonical bases now permit reviewed
prefix-pruning while preserving latest authority and exact operation receipts
by default. Explicit receipt expiry requires a new operation namespace and is
not enabled for existing modular sessions. Arbitrary retained identity
cardinality remains finite and quota-bounded. The runtime reserves maintenance
headroom and stops admission when its declared policy cannot reclaim more.

The detailed A–E capability plans below are retained as subplans: A (VLM) and
C (SIM3 changes) map to M3; B maps to M2/M4/M6; E maps to M5. D remains a
separate scientific reference/model qualification workstream that uses the
new contracts. Scientific input meaning, isolated trials and independent
reference evidence remain prerequisites for scientific live application.

## Batch A — Replayable executable digital plans

**Outcome:** the existing `bread-first.cevplan` executes as a finite workload
inside a mixed native session. The operator can inspect its stage/task, restart
mid-stage, and obtain the same CEVLM/2 output and original plan receipts as the
standalone executor and an isolated replay.

1. **Pinned registration and typed binding.** Introduce a small versioned
   execution manifest around unchanged CEVPLAN/1, CEVLM/2 and artifact bytes.
   Bind the actual VLM interpreter, behavior version, source/epoch, logical
   clock and execution bounds. Register all bytes immutably and preflight the
   complete program before admitting effects.
2. **Actual VLM plan engine.** Use the existing `plans::Session` interpreter;
   do not translate its actions into SW5 inputs. Add explicit model-specific
   summaries and a finite-completion contract. Completing this engine must
   allow other engines to continue without advancing the finished plan or
   repeatedly publishing its completion.
3. **Durable continuation and evidence.** Persist plan/base identity, committed
   tick count, stage progress, complete machine identity and receipt tip.
   Reuse operation IDs, cancellation and commit-before-projection. The initial
   bounded restore may reconstruct the interpreter from retained pinned
   artifacts with publication suppressed, then verify the exact saved state
   and receipt tip before resuming.
4. **Isolated replay qualification.** Execute the same pinned program through
   the same adapter in a fresh instance and retain its state/receipt comparison.
   This establishes replay equivalence without inventing an optimization
   candidate or granting live application authority.
5. **Complete operator path.** Integrate registration, session run/resume,
   replay qualification, typed progress and trace links with the existing
   CLI/IPC surface. Supply accurate full and lean build commands and actual
   Windows/Linux recovery evidence.

**Proposed initial scope:** at most eight stages and 128 logical ticks, an
initially quiescent CEVLM/2 base, no runtime plan replacement and no candidate
mutation. These bounds are implemented in the CE-SW-38 VLM binding. Check
ProcessMachine task, journal and event capacity in addition to stage count.
The existing three-stage, twelve-tick example is the first complete workflow.

**Completion gates**

- A multi-stage CEVPLAN example actually executes its VLM actions. Its result
  matches the existing standalone executor for the same pinned base,
  artifacts and action sequence.
- Uninterrupted, restarted and isolated-replay runs have identical
  final model state and semantic action receipts. Run IDs, timestamps and
  transport receipts may legitimately differ and are compared separately.
- Restart after every tick of the example, including both sides of stage
  completion, preserves the result. A crash before/after checkpoint or evidence
  publication cannot skip a committed action or apply it twice.
- Completion occurs once while another session engine continues. Isolated
  replay cannot mutate the live instance or advance its publication cursor.
- Execution and recovery use retained artifacts even if the original source
  paths disappear. Replaying a saved machine alone is insufficient unless the
  plan interpreter's stage position and receipt tip are also reconstructed.
- Unknown actions, artifact changes, incompatible schemas/clocks, stale source
  state and insufficient remaining horizon fail before mutation.
- Cancellation suppresses future actions and application; progress remains
  queryable and restart does not resolve a new file version.
- Services choose plans and outcomes. Projector only materializes committed
  progress. Native controller operation does not require Python or an external
  numerical solver.

**Scope boundary:** this batch does not introduce VLM candidate optimization,
general biological plan semantics, SIM3 live apply, scientific input programs
or remote control. A later plan-variant evaluator must pin each alternative
program, an equal horizon and meaningful objectives. It must not claim
optimization by inserting an identity candidate into the existing SW5/SIM3
experiment schema.

## Batch B — Sustained evidence lifecycle and measured recovery

**Outcome:** source rollover and retained history no longer consume active
capacity indefinitely, and an operator can see and manage storage/replay costs.

1. **Direct history/receipt lookup.** Add a rebuildable on-disk index behind
   existing storage ports. Verify canonical records and digests on access.
   Cover duplicate lookup, receipt verification and index corruption/rebuild.
2. **Source/epoch retirement.** Persist lifecycle transitions, release eligible
   active slots and reject unexpected writes to retired epochs. Cover
   enrollment, observation/replay cursors and checkpoints as well as Projector
   materialization. Preserve tombstones, historical queries and source
   ownership; restart or full replay must not resurrect a retired active epoch.
3. **Retention and archive lifecycle.** Produce a maintenance plan listing
   protected roots/checkpoints, lagging subscriptions and affected objects/
   bytes. Execute an explicit policy with restart recovery; neither automatic
   array eviction nor unreferenced-looking data alone authorizes evidence loss.
   Define abandoned/revoked subscription retirement and a retained-history
   boundary. Older cursors require an explicit resnapshot/rebootstrap result.
4. **Capacity and lag views.** Expose active sources/epochs, metric slots, bucket
   pressure, archive growth, pending receipts, subscriber lag and observed
   throughput through typed bounded diagnostics.
5. **Native workload qualification.** Publish machine-readable Windows/Linux
   reports for mixed engines, historical duplicates, source churn, slow peers,
   restart, disk pressure and interrupted maintenance.

**Completion gates**

- Historical lookup work is bounded by the requested key/index range rather
  than scanning every unrelated record. Full audit no longer performs a full
  metadata scan for each observation.
- Tests rotate through more than 32 historical epochs while enforcing an
  explicit active-source bound across enrollment, projection and recovery.
  Restart and full audit respect retirement without removing required evidence.
- Referenced checkpoints, pending subscriptions and provenance survive
  reclamation and every injected maintenance cut. Corruption fails closed.
  A cursor outside the retained-history boundary cannot receive a false
  acknowledgement or silently skip missing evidence.
- Qualify at least 100,000 committed observations and 100 epoch transitions
  across a declared mixed workload. These are future minimum workload targets,
  not results already achieved; SIM3 remains within its current tick limit
  until that limit is separately qualified.
- Record hardware, OS, build flags, workload/seed, memory, disk growth,
  throughput, p50/p95/p99 operation latency and recovery duration. Declare
  platform budgets before the qualifying run and retain failures as well as
  passing reports. A queue's capacity is not a throughput measurement.

This batch gates wider live mesh operation. Selective compiled-module
extraction may accompany the measured bottleneck it resolves; it is not a
reason to replace the runtime or introduce a plugin framework.

## Batch C — Recoverable SIM3 policy application

**Outcome:** existing isolated SIM3 link/teaching candidates can become
recoverable digital policy changes with observable outcomes.

1. Define a versioned identity covering mutable configuration, model behavior,
   source state and checkpoint together.
2. Implement complete checkpoint/restore and isolated continuation behind a
   model-specific port, including configuration changes.
3. Stage supported link/teaching changes with stale-source checks and a durable
   apply receipt; reuse the live session's change authority.
4. Bind outcome measurement and retain/revert decisions to the same plan,
   source and horizon. Reverting a reversible link policy preserves elapsed
   simulation time and changes future ticks. Pending teaching can be cancelled;
   an executed one-shot teach retains its effect and receipt. Restoring
   configuration does not undo that effect. Any compensating mutation needs
   its own explicit semantics; a historical fork uses a new identity.
5. Integrate commands/views and both-platform crash, restart, cancellation and
   capability-conformance tests.

**Completion gates:** injected cuts before and after intent, target checkpoint,
commit and projection recover one valid state with no duplicated accepted
mutation. Forged capability flags do not enable application. Live/trial
semantics agree for unchanged inputs. Automatic apply/revert stays disabled
until all gates pass. A tick-limit increase needs its own storage/runtime
evidence; it is not included merely by widening a counter.

## Batch D — Qualified scientific programs and isolated trials

**Outcome:** compare scientific candidates under an immutable declared
protocol, with evidence sufficient to distinguish a numerical regression
fixture from an independently qualified external reference.

1. Add model-specific input programs with explicit units, time samples,
   interpolation/hold policy, validity domain and immutable identity.
2. Run isolated provider trials from pinned full checkpoints; preserve
   process ownership, cancellation and exact source/behavior/protocol identity.
3. Extend quantity contracts with the meaning required by each model, such as
   species, compartment/volume and transform identity where applicable.
   Compatible units alone do not authorize a coupling.
4. Add reviewed extraction and dataset-lineage records for a real read-only
   reference when available. Pin raw data, producer/instrument or simulation
   identity, protocol, transforms, uncertainty and independent split evidence.
5. Integrate training-only selection, frozen holdout/final evaluation,
   dimensionally meaningful objectives and reproducible qualification reports.

**Completion gates:** wrong quantity/species/clock/transform is rejected;
baseline and candidates share a declared input protocol; reserved observations
do not influence selection; fresh-process continuation reproduces the pinned
state. Qualified-reference status requires reviewed evidence beyond a file
hash. Without it, reports remain external-unverified. Declared optimizer
risk/uncertainty scores remain distinct from estimated statistical uncertainty.

Scientific live application remains a separate capability decision after these
gates. A passing synthetic fit grants no new application authority.

## Batch E — Continuous evidence mesh operation

**Outcome:** continuous evidence exchange with bounded slow-peer behavior and
an operable identity lifecycle.

1. Add explicit live subscription ownership and bounded fan-out/credit.
2. Resume after disconnects and lag using the indexed receipts and lifecycle
   rules qualified in B.
3. Version broader source/schema scopes only where their quantity and
   provenance contracts are defined.
4. Implement deployment key provisioning, rotation/revocation recovery and,
   where needed, transferable original-author signatures.
5. Qualify multiple native peers under churn, lag, policy changes, storage
   pressure and restart, retaining per-peer diagnostics and evidence reports.

**Completion gates:** slow peers cannot grow unbounded queues or indefinitely
prevent unrelated sources from making progress; per-peer fairness and latency
limits are declared and measured. Replay preserves exact source/epoch ownership;
revoked scopes cannot continue ingress; restart reconciles receipts without
false acknowledgement.
TLS relay receipts remain labeled as relay receipts unless an original-author
signature is actually verified.

Discovery, QUIC and remote experiment/solver control require separate use
cases and authorization designs. They are not prerequisites for this batch.

## Delivery and reassessment

For each batch, retain the reviewed commit, supported capability matrix,
failure-boundary evidence, both-platform results and any outstanding
qualification limits. Update the implementation checkpoint only after the
capability is wired into a usable operator path.

Re-rate the relevant category when its evidence changes: A improves executable
plan integration; B establishes operational lifecycle and scale; C establishes
SIM3 mutation parity; D can improve scientific qualification for a specific
model and reference; E improves continuous mesh operation. More code or more
test assertions alone do not establish a higher rating.

Keep native Windows/Linux parity, bounded queues, source/epoch/clock identity,
commit-before-projection and independent experiment selection as standing
invariants. Preserve the [native development instructions](../../native/AGENTS.md)
and keep the current roadmap separate from historical specifications and
scientific/physical release decisions.
