# Native readiness and development plan

**Review date:** 2026-11-17 (America/Chicago).

**Implementation baseline:** [`c5a4e8a`](https://github.com/bradgearon/ChristEye-Knowledge/commit/c5a4e8a04616c76decab52d237de53a3f3b09f23), through CE-SW-36.

**Purpose:** assess implemented native software, identify the next usable
capabilities, and make their completion criteria explicit.

Use the [implementation checkpoint](native_implementation_status.md) for what
currently runs. This document owns the forward development order. Historical
CE-SW specifications retain the scope and limitations of their original
increments; their old TODOs are not the current backlog. The
[project phase roadmap](../roadmap.md) separately records scientific and
physical qualification.

## Assessment

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

## Findings that determine the order

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

Keep the lifecycle interface narrow. Extract stable host implementations into
compiled modules when an independent consumer or measured build problem
justifies the boundary. A dynamic plugin ABI, callback sandbox and universal
model abstraction are not prerequisites for the next plan capability.

## Revised delivery order

Each batch is approximately five related increments delivered as one usable
capability, including services, adapters, operator wiring, failure tests,
native Windows/Linux verification and documentation. Internal commits may be
reviewed separately. These are proposed capabilities, not implemented features.

| Priority | Batch | Usable result | Dependency |
|---|---|---|---|
| 1 | A — Replayable executable digital plans | Run an existing CEVPLAN as a finite native-session workload, resume mid-stage and qualify its isolated replay | Current session, operation and evidence foundation |
| 2 | B — Sustained evidence lifecycle and measured recovery | Operate through source churn, retained history, slow peers and maintenance within declared bounds | Current segmented storage and evidence contracts; can proceed alongside A |
| 3 | C — Recoverable SIM3 policy application | Apply supported digital SIM3 policy changes and evaluate their outcomes with configuration/state recovery | Existing SIM3 evaluator plus a new configuration/state checkpoint contract; longer runs need B |
| 4 | D — Qualified scientific programs and isolated trials | Execute physical-time input programs, compare pinned providers and qualify declared external references | Typed program/quantity contracts; independently available reference evidence |
| 5 | E — Continuous evidence mesh operation | Sustain bounded live subscriptions with lag recovery and deployment identity lifecycle | B; broader scientific scopes also depend on D |

The principal changes from the previous checkpoint are to make operational
lifecycle a named second priority, separate SIM3 apply from plan import, move
direct receipt indexing ahead of mesh expansion, and require explicit input
semantics and isolated trials before scientific live optimization.

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
mutation. These are future admission bounds, not existing guarantees. Check
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
