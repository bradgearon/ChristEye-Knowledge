# CE-SW-41 — Measuring and operating the native runtime over time

This increment extends [CE-SW-40](CE-SW-40_operating_envelope_and_package_lifecycle.md).
It gives operators four ways to answer questions the previous short tests could
not settle. The [evidence index](../../qualification/reports/2026-11-18-runtime-lifecycle/README.md)
separates development measurements, recovery fixtures and outstanding release gates.

| Operator question | What the runtime now does | Evidence needed for a deployment claim |
|---|---|---|
| Which part of a commit is slow, and did the stated latency limit pass? | Records disjoint commit stages on request, counts threshold exceedances exactly, retains the declared device and reports the legacy histogram alongside the exact decision. | A completed run on the claimed build/device, at the claimed scale; retain failures as well as passes. |
| Will old history still be readable, and can the store reclaim space without breaking a retry? | Exercises returning and new source IDs, pinned history, expiring receiver history, three age ranges of lookups, compaction, reopen and audit in one store. Checks byte and, where available, file-node headroom. | End-to-end measurements at each advertised size, including spare space needed during compaction. |
| Does the five-engine host keep working through a peer outage or process termination? | Captures per-phase timings, exact replay acknowledgments, checkpointed campaigns and a distinction between completing outstanding work and remaining continuously alive. | Short TLS and process-cut fixtures establish the exercised behavior. Elapsed 24-hour and seven-day runs under their declared conditions establish duration. |
| Can we upgrade and roll back without discarding work, or bootstrap a lagging peer while the source advances? | Verifies exact packages against a separate conservative state contract; retains bounded package witnesses. Uses a durable, leased capture whose protected checkpoint and suffix can be handed to a subscription. | Actual package pair plus OS supervisor and failure/recovery tests on each supported platform; no general schema migration implied. |

## 1. Storage latency and attribution

The original profile keeps its original histogram decision. The new profile
compares each duration to the same numeric limit. For example, with 159
samples, a p99 budget permits one sample strictly over the limit; equality
passes. A power-of-two histogram bucket can straddle the limit, so its upper
bound alone cannot determine this exact result. Neither decision rewrites a
previous report. The [measurement guide](../../qualification/native/README.md)
explains what is included in a timed commit and in the overall wall clock.

Two changes reduce identifiable work without changing the canonical receipt
or publication order. A serialized commit can reuse a bounded set of index
nodes it has already verified during admission. Compaction builds a fresh,
balanced index directly from canonical sorted rows; it verifies that tree
before publishing the replacement root. Both ordinary commits and compaction
check space for the pending write. A rejected preflight has no latency or
throughput result. On filesystems that cannot report free file nodes the
report marks the count unavailable.

## 2. Retained history and its capacity limit

The [scale guide](../../qualification/native/SCALE.md) describes the actual store
workload, receiver window, quotas and expiry. Bounded active source slots do
not imply that historic source identities, replay fences or receipts can be
deleted. Protected data can fill the store. The operator can inspect ordinary
and maintenance capacity, review reclamation, and stop accepting writes
without silently invalidating retry or historical lookup guarantees. A compact
store may occupy *more* accounted bytes; compare regular-file allocation,
directory allocation, and remaining free space separately.

A live bootstrap continuation binds the source and checkpoint, accepted tip,
principal, subscription generation and lease. It protects the checkpoint and
the required suffix while normal source commits continue. Acceptance installs
the subscription and transfers protection atomically. Expiry or cancellation
releases the temporary protection under the same bounded-directory rules.
See the [mesh operations guide](../../native/docs/continuous-mesh.md) for the
difference between a pending continuation and the legacy latest-tip exchange.

## 3. Running, recovering and interpreting campaigns

The five workloads are **Cell Behavior (SW5)**, **Multi-Colony (SIM3)**, the
finite **Virtual Living Machine (VLM) plan**, **passive voltage** and **scalar
environment**. They are digital engines in the same native session. Campaign
inputs include the original session text, the fully admitted definition, the
profile and build, and the required device declaration. Recovery validates
those identities, exact operation receipts and bounded progress. Inspect the
attempt history as well as the final state: replaying an accepted operation
must not count a second observation or turn a recovered run into an
uninterrupted one. Dispatch, queue, canonical commit, projection and the real
TLS receiver acknowledgment have different timing boundaries.

Use the [campaign guide](../../qualification/native/MIXED.md) for commands and
the short, 24-hour and seven-day profiles. Run long profiles on a persistent
machine with a dedicated device declaration. The isolated-volume fixture
separately exercises actual full-disk write failure and recovery; the safe
refusal test alone is not evidence for that experiment. Neither proves
durability under physical power loss.

## 4. Packages, supervisors and rollback

The [package guide](../../deployment/native/package-lifecycle.md) identifies
the supported migration: a change in exact build identity with an unchanged
verified state contract. A candidate with different state-bearing code or an
unrecognized schema is refused. Precontract packages cannot acquire
compatibility merely by matching a version string. A rollback is reviewed
against the *current* store, so work accepted under the newer package survives
the switch back. Package witnesses have a reviewed retention policy which keeps
protected selections and rollback evidence. The OS service-switch fixture runs
separately from the store migration fixture; a passing package test cannot
stand in for an installed supervisor test.

## Remaining acceptance work

The shared Linux [100k maintenance profile](../../qualification/reports/2026-11-18-runtime-lifecycle/scale-100k-frozen/report.json)
completed all gates; its final compaction took 376.49 seconds. The shared
[frozen synthetic 10k run](../../qualification/reports/2026-11-18-runtime-lifecycle/synthetic-10k-frozen/report.json)
failed commit latency. [Systemd and SCM](../../qualification/reports/2026-11-18-runtime-lifecycle/package-hosted-corrected/README.md)
passed the supported release pair. Collect comparable 100k results on both
supported systems and full million-row results on adequately provisioned
devices, including the scale maintenance profile. The [real isolated Linux
tmpfs and Windows NTFS VHD jobs](../../qualification/reports/2026-11-18-runtime-lifecycle/pressure-hosted/README.md)
both passed disk-full admission and post-admission failure recovery. Complete
the elapsed 24-hour and seven-day campaigns, and assess physical power-loss
durability with a separately specified experiment.
