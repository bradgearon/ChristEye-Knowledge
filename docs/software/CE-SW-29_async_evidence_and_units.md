---
title: "CE-SW-29 — Bounded Async Evidence and Unit-Safe Scientific Coupling"
product_version: "Anaiero v0.015.x"
status: "Native C++20 asynchronous evidence prototype"
---

# CE-SW-29 — Async journal, checkpoint index and unit boundaries

## Durable ordering

AsyncEpochJournalWorker owns one bounded storage thread. The simulation owner
enqueues a complete typed observation through IAsyncObservationStore; it does
not wait for WindStore's fsync. The receipt returns only when the injected
epoch journal's committed event HEAD reports success. Only then can
AsyncObservationCommitService send the observation to the ColonyState port.

Results have stable tickets. Queue accepted is explicitly not a durable ACK.
The storage thread cannot write into ColonyState itself. Failed commits are
reported to the caller for retry; a crash after a journal HEAD commit can be
reconstructed through the existing epoch replay service. Shutdown drains
accepted work but may discard in-memory completion notifications if the
receipt queue is full; immutable committed evidence remains recoverable.

## Checkpoint and retention

The injectable ICheckpointCatalog stores an engine/epoch/revision metadata
manifest and its model-pinned payload as separate immutable WindStore CAS
objects. Both objects must exist before CULTURE_CHECKPOINT is published.
Readers verify full payload SHA and select the latest valid epoch revision.
An engine revision is immutable; a second conflicting value is rejected.

JournalRetentionReviewService checks verified snapshots and per-subscriber
cursor thresholds before producing a candidate pruning boundary. It does
**not** actually truncate or delete journal records. Destructive segmented
compaction and archival checks are still missing; pretending to prune now
could destroy future replay or consumer subscriptions.

## Scientific units

UnitConversionService uses exact integer unit conversion (uV/mV, ns/us/ms,
uM/mM) and rejects losing fractional precision, overflow and dimensions that
cannot be mixed. A dimensionless software proxy cannot be upgraded to a
voltage or concentration merely by changing its unit label. Conservation
checks reject mismatched source epochs and residuals beyond declared budgets.
These contracts are only the groundwork for independently calibrated NEURON
and PhysiCell coupling; they are not a validated biological transport model.

## Test acceptance

Windows/Linux tests verify asynchronous commit-before-projection, full
WindStore replay after process recreation, immutable checkpoint references,
duplicate revision rejection, unacknowledged subscriber retention gates,
and exact unit/conservation checks.

## Remaining work

Integrate this queue into the continuously running host (rather than the
standalone test coordinator); implement verified segmented retention;
qualify OS disk semantics, CPU/memory caps and asynchronous shutdown;
and provide externally validated units, model fixtures and uncertainty.
