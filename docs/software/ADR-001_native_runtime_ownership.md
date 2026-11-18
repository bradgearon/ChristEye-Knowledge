---
title: "ADR-001 — Anaiero Native Runtime Ownership and Transactional Observation"
status: "Proposed, implementation pending"
date: "2026-11-16"
product_version: "0.015.0"
reviewed_commit: "5f5ab33a6668b712c54dc15327a568e77af67a98"
---

# ADR-001: runtime composition, ownership and evidence delivery

## Implementation note — 2026-11-17

The front matter and rationale below retain the original proposal baseline.
CE-SW-31–36 now implement mixed native sessions, persisted cycle/operation
intent, source/epoch-aware evidence, commit-before-projection, owned scientific
processes and authenticated evidence replication. The original reverse-layer
dependencies named below have been removed and are guarded against regression.
Per-engine recoverable publication does not imply an atomic group snapshot.
Compiled library isolation and representative deployment qualification remain
open. See the [implementation checkpoint](native_implementation_status.md) and
[current development plan](native_development_plan.md) for exact scope and
next acceptance gates.

## Decision

1. **Single public anaiero executable.** The existing subcommands remain; anaiero runtime start becomes the host supervisor through a separate application composition root. ce_* backends remain compatibility shims until their libraries are extracted.
2. **Pure models, interfaces at the contract boundary.** Models only hold typed state; domain services implement deterministic decisions; application use cases coordinate transaction boundaries; infrastructure implements wire, storage, clock, transport, memory mapping and OS. No new upward includes are permitted.
3. **Canonical observation projection, not one universal cell class.** SIM1/VLM/SIM3/SW5 retain their own internal models. A versioned ObservationEnvelope/1 records culture, engine, source epoch, metric/event kind, clock, provenance and evidence reference. Translation is explicit and provenance-preserving.
4. **One ordered owner per culture and one source-epoch cursor.** Cross-thread producers use bounded ingress. The projector accepts source-qualified events only after admission and, when durability is enabled, persistence.
5. **Culture state and outbox commit together** under a recoverable digital transaction. A committed lineage event cannot disappear due to a full transient bus. On failure, the outbox is retried idempotently.
6. **No physical actuation through application services.** A simulated digital movement is not an instrument instruction. Read-only qualified acquisition is isolated and separately reviewed.

## Why

Existing code has split simulation models, post-mutation callback failures, and a volatile projector without replay. The SW4 RuntimeService directly includes WeightedQueue; WindBus adapter imports RuntimeService; the SW5 checkpoint adapter imports CellBehaviorService. These are documented migration debts, not reasons to rewrite all simulation logic.

## Host parity

[ADR-002](ADR-002_native_windows_linux_parity.md) requires Linux and Windows to be equally supported low-resource, native C++20 controller hosts. OS-specific APIs live only in infrastructure. Neither host is a second-tier fallback.

## Lifecycle

~~~text
STOPPED -> STARTING -> RECOVERING -> READY -> DEGRADED -> STOPPING -> STOPPED
                            |         |
                            +-- FAULT-+
~~~

- STARTING: load configuration, validate profiles, acquire single-writer leases.
- RECOVERING: verify committed culture checkpoint/outbox, rebuild projection and source epoch cursors.
- READY: event-driven normal operation; snapshot/subscribe permitted under scopes.
- DEGRADED: stale/unqualified sources or persistence backpressure; still display last-known state with explicit quality; never silently overwrite.
- FAULT: committed-data conflict or corruption; fail closed and require operator recovery, not evidence truncation.
- STOPPING: stop accepting new commands, drain/commit within bounded deadline, checkpoint and revoke IPC endpoints.

## Identity and causality

A logical cell identity is (culture_id, engine_id, subject_id, epoch). A real instrument source identity is separate and must be authenticated by an adapter or session. Producer event ordering is (source_id, source_epoch, sequence). Processing order may differ across sources; no claims of global causality without verified time anchors and uncertainty. Evidence digests identify immutable data, not author signatures.

## Runtime ports

- IObservationIngress: submit validated typed observations with explicit admission receipt.
- IEventJournal: commit/replay immutable records with source epoch and durable cursor.
- IStatePublisher: push immutable revisioned snapshots and deltas.
- ISimulationMovement: move digital state through bounded spatial/graph/resource adapters.
- ISourceIdentity: resolve authenticated local/remote identity and capability scope.
- IClock: monotonic runtime time, source clock domains and uncertainty anchors.
- IOutbox: stage and acknowledge changes coupled with simulation commit.

These are target interfaces; existing IEventCommit, ISpatialPort and IMovementPort are narrower prototype adapters that should be migrated, not rebranded as completed replacements.

## Consequences

- Backward compatibility requires adapters and a versioned migration map rather than direct renaming of legacy serialized objects.
- Persistence incurs I/O cost; the event-loop thread never blocks on fsync.
- Backpressure requires explicit credit and replay. No unbounded memory queue or dropped lineage event.
- One lightweight native **Windows or Linux** supervisor per controller simplifies failure domains. Platform adapters provide IOCP/named pipes/SCM or epoll/Unix-domain sockets/systemd, with shared C++20 business logic and no emulation.
- Authentication, source qualification and biological validity remain separate concerns.

## Revisit

Reconsider this ADR after the 0.018 native Linux and Windows services has measured end-to-end p99 latency, memory, crash recovery and source synchronization on target hardware. Until then, limits are engineering targets, not qualified real-time guarantees.
