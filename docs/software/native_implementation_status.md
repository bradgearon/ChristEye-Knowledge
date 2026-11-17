# Native implementation checkpoint

This index records operational integration separately from component existence.
Verify the current engineering branch and CI when continuing work.

## Standing delivery preference

The user requests roughly **5× the previous incremental scope per changeset**.
Deliver a substantial complete capability with implementation, adapters,
runtime/CLI wiring, meaningful Windows/Linux tests and documentation. Keep
related work in one changeset with reviewable internal commits.

## Current integrated batch — CE-SW-31

| Capability | Component | Integrated controller |
|---|---|---|
| SW5 + SIM3 lifecycle | Implemented | Configured mixed-cadence session |
| Partial-cycle retries | Corrected and regression-tested | Persistent intent + per-step continuation |
| Async observation evidence | Bounded worker + ordered service | Separate journal owner, completion wakeups, commit-before-projection |
| Checkpoints and restart | Model-pinned CAS catalog | Session manifest, durable source identities, replay and validated continuation |
| Generic optimization | Real SW5 evaluator + generic ranking | Active SW5 ANALYZE/APPLY/REVERT, recorded evidence and crash recovery |
| Local operator control | Same-user IPC | Cached complete views, bounded commands, revision WATCH, cadence and cancellation |
| Process/service lifecycle | Native Windows/Linux adapters | Console signals, Windows SCM entry, systemd notify and manual deployment examples |
| Storage budget | Existing bounded WindStore | Admission reserve and recoverable capacity fault; no compaction |

See [CE-SW-31](CE-SW-31_integrated_native_sessions.md) for protocols, commands,
acceptance tests and exact limits. PRs #37–41 supplied the component foundation;
this batch composes those parts into an operating native session.

## Next substantial capabilities

1. **Reusable experiments and learning-plan execution:** common experiment
   identity, existing CEVPLAN/artifact references, configurable policy and
   objectives, actual isolated SIM3 evaluator, evidence and outcome feedback.
2. **Supervised scientific engine sessions:** bounded optional process exchange,
   real solver lifecycle/capabilities, actual units in the running path, pinned
   NEURON and first environmental model integration.
3. **Secure mesh:** authenticated transport/enrollment, verified-session policy,
   durable subscriptions, reconnect/replay and bounded artifact transfer.
4. **Storage/scale:** segmented journal, verified retention/archival, sustained
   operation measurements and failure qualification.

The Projector stays observational. Optimization stays independent. Native
Windows and native Linux remain first-class, with all domain decisions in
services and transport/storage/process mechanisms behind typed ports.
