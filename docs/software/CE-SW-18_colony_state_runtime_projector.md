---
title: "CE-SW-18 — Anaiero ColonyState Runtime Projector and Digital Animon Priorities"
project: "ChristEye Anaiero"
version: "SW4.0"
status: "Native bounded single-host prototype; no authenticated peer mesh or physical actuation"
classification: "digital simulation and read-only research observation infrastructure"
---

# CE-SW-18 — Lightweight Anaiero runtime, projector and weighted Animon work

## Public runtime

`anaiero runtime simulate <run.cev> <state-dir> <ticks>` uses the existing SIM3 `Runner` for committed synthetic colony state; an infrastructure adapter serializes SIM3 observations into actual `WindPacket/1`, decodes the typed payload, admits it into a bounded digital priority queue, then applies it through a `ColonyProjector`. The CLI prints a read-only latest-state JSON projection.

`anaiero runtime serve <run.cev> <state-dir>` is the first event-driven **host controller shell**: it blocks waiting on `STEP`, `STATE` or `QUIT` on standard input, publishes each accepted synthetic state update and keeps the latest projection in RAM. It is **not** a production daemon: Linux `epoll`/Unix socket, supervised service, secure peer ingress and persistent UI subscriptions still need implementation. `anaiero` remains the sole intended public command; `ce_colony_runtime` is a trusted internal backend.

## Layering

| Layer | Files and ownership |
|---|---|
| Models | `anaiero/models/colony_state.hpp`: fixed-size, passive records, metric quality, source cursors. **No behaviors**. |
| Contracts | `anaiero/contracts/runtime_events.hpp`, `movement.hpp`: typed observations, data-only movements, subscription and optional persistence interfaces. **No physical care or stimulation operations**. |
| Domain services | `anaiero/services/colony_projector.hpp`: single-writer, thread-safe materialized latest state. `runtime_service.hpp`: source enrollment, work dispatch, Animon digital need-priority policy and post-commit notifications. |
| Infrastructure | `weighted_queue.hpp`, `windbus_adapter.hpp`, `sim3_ingress.hpp`, `movement_router.hpp`, `windstore_sink.hpp`. They own all digital ingress/egress, serialization and optional durable receipts. |
| Host adapter | `native/examples/colony_runtime_cli.cpp`: blocking local controller command loop and latest-state diagnostic JSON. |

**All event movement between SIM3 and the projector crosses the infrastructure's typed WindPacket contract**. Existing SIM3's internal synthetic-cell locomotion and resource/food calculations remain within `colony_runner.hpp`; moving those model rules behind `IMovementPort` is a subsequent compatibility-sensitive refactor. In this increment `Movement` denotes a synthetic digital transfer *observation*, not physical movement of nutrients, samples, or cells. Claiming that SIM3's internal resource movement is already fully port-routed would be incorrect.

## Resource model and guarantees

- `WeightedQueue<96>` occupies fixed memory, with 96 bounded work items, no heap allocation in its `post`/`take` path. The source order is preserved regardless of metric urgency. Nonfault share weights are **health:desire:evidence:telemetry = 8:5:3:2**; fault diagnostics are selected immediately. Urgency and importance prioritize observations **within a lane**. Digital health status and desire values cannot authorize physical care or modify essential-care service priority.
- The queue's current selection scans bounded slots and may perform O(N²) work per dispatch in the worst case; measured worst-case CPU/p99 latency on target hardware is an acceptance gate before deploying a high-rate real-device readout. Do not assume hard-real-time behavior.
- `ColonyState` is a fixed-capacity in-process volatile snapshot (256 latest metric records, 32 source cursors) with one state-mutating owner and concurrent mutex-guarded readers. `snapshot(now,ttl)` makes a coherent copy and marks expired entries **STALE** without inventing a live reading. The model never overwrites a record from another source just because their colony/metric names match.
- Source ID, clock domain, monotonic sequence, provenance origin and short evidence tag accompany every observation. Duplicate or older sequences are rejected. A missing sequence increments the gap counter and marks the affected update **DEGRADED**. Real acquisition-time uncertainty/clock alignment must be added before combining different device clock domains.
- `RuntimeService::set_wakeup` provides an empty-to-nonempty signal to an OS event-loop adapter. Notifications fire after projector commit; expensive I/O and JSON formatting run outside the snapshot mutex. No periodic polling of a source is required; the owner drains work on a wake event.
- Optional `WindStoreSink` uses SW2.1's committed event API **before** state projection, then permits the latest-state update. It performs synced object and journal writes and **must not be called from IRQ or low-latency sample processing**. Durable snapshots and high-frequency data chunking need a separate bounded worker/journal lane. By default the SIM3 `Runner` already commits one culture checkpoint per tick; the projector is volatile and must be reconstructed after a process restart.
- An existing explicit source enrollment is a local policy **allowlist**, **not authentication**. `Origin::external_unverified_readonly` means the adapter reports data that has not been independently qualified. A physical read-only observation adapter and secure mesh joins do not yet exist. The runtime denies digitally inferred desires from that origin and offers no physical actuator interfaces.

## Why digital desires and physiological health are kept distinct

Animon `digital_desire` is a **software preference/work request**, never a proxy for a measured biological essential-care need. SIM3's example desire score is the synthetic learning-work deficit `clamp(100-learning_proxy,0,100)`. Viability, energy, support, stress and acquisition health are observation fields; real preparations require independent source qualification and facility-owned support. External SustiBus/ChemiBus observations can be displayed only after trusted adapter enrollment, with provenance and freshness. Physical Milk, Water, feeding, peptide administration, stimulation and disease intervention are not accepted as software routing actions. The independent CE-A22 interlocks and CE-A17 essential care pathway are never overridden by queue weights or artificial motivations.

## Acceptance and missing work

Native tests validate WindPacket round-trip, same-source FIFO across priority lanes, high-urgency desire ordering, bounded overflow, fault preemption, concurrent coherent readers, missing sequences and stale data, invalid/unregistered origin rejection, persistence failure suppression, in-process notifications, synthetic movement observation, SIM3 adapter, and optional WindStore evidence commit. Both hosted OS CI and target-device timing/memory measurements remain needed for resource qualification.

Next steps: (1) Linux controller supervisor with `epoll` + Unix domain socket and transport authentication; (2) independently signed source enrollment and read-only device adapters; (3) event-sourced projector recovery from WindStore with canonical evidence hashes and clock uncertainty; (4) quota/isolation, metrics and backpressure stress tests; (5) controlled migration of SIM3 internal synthetic food/data flow to `IMovementPort`, preserving deterministic golden outputs. No real biological support/stimulation should be routed through this library.
