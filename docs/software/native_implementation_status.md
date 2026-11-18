# Native implementation checkpoint

This index records operational integration separately from component existence.
Verify the current engineering branch and CI when continuing work.

The [native readiness and development plan](native_development_plan.md) records
the 2026-11-17 assessment, commit-pinned evidence, priority order and completion
gates. This checkpoint describes implemented behavior; the plan describes
future capabilities.

## Standing delivery preference

The user requests roughly **5× the previous incremental scope per changeset**.
Deliver a substantial complete capability with implementation, adapters,
runtime/CLI wiring, meaningful Windows/Linux tests and documentation. Keep
related work in one changeset with reviewable internal commits.

## Current integrated batch — CE-SW-36 environment and calibration

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

- Mesh subscriptions currently carry one complete simulated source/epoch stream
  with fixed colony and clock. A different publishing principal requires a new
  epoch; a different clock requires a new source ID. Relay receipts record the
  verified connection, not a transferable original-author signature.
- Mesh reconciliation runs on explicit connect/user requests, with a finite
  per-invocation page budget. Discovery, QUIC, continual live fan-out, remote
  experiment control and physical acquisition adapters are later capabilities.
- Artifact uploads remain local administration; remote download grants exact
  digests. Interrupted staging restarts at byte zero; completed receipts persist.
- Historical archives and SIM3 tick snapshots consume disk; source retirement,
  archive deletion and index repartitioning are separate policies.
- Session publication is recoverable per engine, not an atomic group snapshot.
- SIM3 can evaluate candidates but cannot apply them live or restore arbitrary
  checkpoints; its existing model tick ceiling remains 10,000.
- CEVPLAN already executes through its standalone CEVLM/2 interpreter. In
  integrated reusable experiments, those references are validated provenance;
  they do not replace the explicit engine input schedules or supply a durable
  VLM-plan engine in a mixed session.
- Scientific sessions expose modeled passive voltage and generic scalar
  concentration. Offline environmental parameter calibration is available;
  biological calibration, scientific experimental forks, live application,
  spatial environmental dynamics and inter-model scientific coupling remain
  separate capabilities.
- Core modules remain largely header-oriented. Recursive dependency checks and
  the controller build profile improve boundaries without claiming binary ABI
  isolation or a dynamically loaded plugin system.

## Next substantial capabilities

The detailed scope, dependencies and acceptance gates are maintained in the
[development plan](native_development_plan.md#revised-delivery-order):

1. **Replayable executable digital plans:** bind the existing CEVPLAN/CEVLM2
   interpreter as a finite native-session workload, with retained artifacts,
   durable mid-stage continuation, typed progress and isolated replay
   qualification. Preserve actual VLM semantics; do not translate them into
   SW5 proxy inputs.
2. **Sustained evidence lifecycle:** direct historical/receipt indexes,
   persisted source/epoch retirement, reference-aware archive policy,
   capacity/lag views and measured Windows/Linux workloads. Complete this
   before substantially expanding live mesh fan-out.
3. **Recoverable SIM3 policy application:** configuration-plus-state
   checkpoint identity, supported link/teaching changes, crash-safe apply and
   measured outcomes with explicit reversal rules for links and one-shot
   teaching. Keep apply disabled until these gates pass.
4. **Qualified scientific programs:** typed physical-time inputs, isolated
   provider trials, domain-specific quantity meaning and independently
   reviewed external-reference extraction before scientific live optimization.
5. **Continuous evidence mesh:** bounded live subscriptions, lag recovery,
   broader declared scopes, original-author signatures where required and
   deployment key lifecycle, building on the operational qualification above.

Each item is a substantial capability batch with services, adapters, operator
wiring, failure tests and both-platform evidence. The next batch's concrete
example is the existing three-stage CEVPLAN running beside another engine,
resuming mid-stage and matching the standalone executor's output and receipts.

The Projector stays observational. Optimization stays independent. Native
Windows and native Linux remain first-class, with all domain decisions in
services and transport/storage/process mechanisms behind typed ports.
