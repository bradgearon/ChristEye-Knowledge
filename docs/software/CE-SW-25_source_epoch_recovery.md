---
title: "CE-SW-25 — Multi-Source Epoch Recovery and Durable ColonyState"
project: "ChristEye Anaiero"
product_version: "0.015.x"
status: "Native bounded local prototype; no remote mesh authentication"
related: "CE-SW-11, CE-SW-18, CE-SW-23, ADR-001, ADR-002"
---

# SW6.2 — Source epochs, read-state recovery and delivered observations

## The problem

SW4/6.1 had useful volatile state projection and reliable receipts for a single optimized digital route, but its ordinary observation sequence was indexed only by **source ID**. An instrument-like producer or synthetic colony that restarted with sequence 1 could collide with its preceding incarnation. Restart did not rebuild arbitrary sources from the committed observation journal.

## Implementation

1. `Observation`, `MetricValue` and `SourceCursor` acquire an **appended source_epoch** (uint64; legacy zero is preserved). A source cursor is now indexed by `(source_id, source_epoch)`, preventing a false duplicate when a simulation restarts.
2. The **existing 120-byte WindPacket observation schema** is unchanged. It explicitly refuses to encode a nonzero source epoch. The new versioned epoch wire schema **0xCE600041** uses a 56-byte WindPacket header + 72-byte payload containing the epoch, strict reserved bits and all original fields. It is never interpreted as a physical device authentication token.
3. `WindStoreEpochObservationJournal` writes each full encoded frame as an immutable SHA-256 CAS object, then records a committed `OBSERVATION_RECORDED` event in SW2.1 WindStore. Its logical identity includes session, source, epoch, sequence and immutable bytes. A conflicting re-use is rejected.
4. `EpochObservationDeliveryService` **preflights** local source policy and read-model admission, then journals before projecting. It requires contiguous per-epoch sequences beginning at 1, permits an identical last event, retains one failed projection in a bounded retry slot, and rejects newer events until that slot succeeds.
5. `EpochRuntimeProjectionPort` uses the existing SW4 weighted queue and projector for all configured sources. A source's older-epoch metrics are explicitly shown as **STALE** when a newer epoch from the same source has been seen. Observations can be read concurrently through the existing mutex-protected snapshot.
6. On restart, the service verifies and replays the committed journal frames for all enrolled sources in their committed interleaving order. Source cursors, metric values and revisions are reconstructed without guessing source clocks.

**Important:** a valid session name, a source slot and an epoch are locally configured metadata—not authenticated network identity. The existing `SourcePermission` local allowlist must be configured before replay. Sources outside that allowlist or unqualified data requesting digital learning are rejected before new evidence is journaled.

## Ordering, capacity and guarantees

The service has fixed room for **32 (source,epoch) cursors**, SW4 retains 256 metric records and the existing weighted queue holds 96 work items; WindStore is still bounded by 4096 committed events and a 4 MiB journal. The current path rejects capacity exhaustion rather than silently erasing history. Hot packet encoding uses fixed-size local buffers; disk sync remains on the calling thread and must be moved to a worker before a production low-latency deployment.

This is **at-least-once retry with idempotent local projection**, not a global transaction spanning SIM3, WindStore and another participant, not an authenticated remote ACK, and not a qualified real-time system. Callers must not use external learned or care-changing actions through this bus. A live instrument interface would be a separate, independently authorized, read-only observation adapter.

### Recovery assumptions

- The same event session name is reused for the same local observation journal.
- Each source epoch starts with sequence 1. A new source instance uses a new epoch; the test does not silently invent one.
- The journal is available and readable at startup; corruption causes failure, not fabricated recovery.
- A change in source permissions may intentionally block old replay until reviewed.
- For a source with many restarts, the bounded cursor capacity eventually requires checked compaction/snapshot migration, which is future work.

## Native test and CLI

`anaiero science epoch-demo examples/cells/crest-blood-neural.cevatlas ./epoch-store`

The test stores events from two sources, restarts one under epoch 2, and reconstructs the view in a fresh runtime. Its expected result is four committed observations, three source epochs, two current values and two superseded stale values. Repeating the command demonstrates restart idempotence.

Windows and Linux CI additionally cover duplicate IDs, forbidden gaps, tampered v2 packets, legacy v1 compatibility, authorization rejection before journal admission and durable subscriber cursor recovery.

Next: secure identity enrollment, epoch lifetime/compaction, persistent state snapshots across every engine and native platform service IPC.
