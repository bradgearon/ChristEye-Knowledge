# Native implementation checkpoint

This index records operational integration separately from component existence.
Verify the current engineering branch and CI when continuing work.

## Standing delivery preference

The user requests roughly **5× the previous incremental scope per changeset**.
Deliver a substantial complete capability with implementation, adapters,
runtime/CLI wiring, meaningful Windows/Linux tests and documentation. Keep
related work in one changeset with reviewable internal commits.

## Current integrated batch — CE-SW-32, CE-SW-33 and CE-SW-34

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

- Historical archives and SIM3 tick snapshots consume disk; source retirement,
  archive deletion and index repartitioning are separate policies.
- Session publication is recoverable per engine, not an atomic group snapshot.
- SIM3 can evaluate candidates but cannot apply them live or restore arbitrary
  checkpoints; its existing model tick ceiling remains 10,000.
- CEVPLAN references are validated provenance; general VLM plan execution is
  not substituted for the explicit engine input schedules.
- Scientific sessions expose modeled passive voltage; biological calibration,
  scientific experimental forks and an environmental model are not included.
- Core modules remain largely header-oriented. Recursive dependency checks and
  the controller build profile improve boundaries without claiming binary ABI
  isolation or a dynamically loaded plugin system.

## Next substantial capabilities

1. **Secure mesh:** authenticated transport/enrollment, verified-session policy,
   durable subscriptions, reconnect/replay and bounded artifact transfer.
2. **Broader scientific models and calibration:** first environmental provider,
   more model-specific quantities, external measurement provenance and held-out
   fidelity criteria before enabling scientific optimization.
3. **Plan and policy expansion:** explicit typed adapters for additional learning
   plan semantics and SIM3 changes, preserving each model's source and clock.
4. **Operational scale:** source retirement, archive lifecycle policy, distributed
   backpressure, longer mixed-workload measurements and compiled module seams.

The Projector stays observational. Optimization stays independent. Native
Windows and native Linux remain first-class, with all domain decisions in
services and transport/storage/process mechanisms behind typed ports.
