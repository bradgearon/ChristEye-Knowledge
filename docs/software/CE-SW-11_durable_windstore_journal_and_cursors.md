---
title: "CE-SW-11 — Crash-Recoverable WindStore Journal and Evidence Cursors"
project: "ChristEye Anaiero"
status: "Host-only SW2.1 reference implementation; not a security-certified data-center database"
version: "0.1.0"
date: "2026-11-15"
related: "CE-SW-01, CE-SW-09, CE-SW-10, CE-A20, CE-A22"
---

# CE-SW-11 — Durable WindStore and restartable evidence consumers

This upgrades the **storage boundary**, not the biological model. The native C++20 host implementation is `native/include/christeye/living/durable_registry.hpp`; platform file operations are isolated in `durable_io.hpp`. It archives simulated/unverified research data and preserves deterministic evidence histories. It is **not** an instrument controller, molecular synthesis engine, neuronal stimulation tool, qualified RNA measurement, or demonstration of biological memory transfer.

## 1. Crash-consistent commit protocol

The store remains compatible with the existing `CEWREG/1` event line format and SHA-256 content-addressed objects, but adds an authoritative atomic `HEAD` file and durable OS operations. One writer owns a store root; read-only copies/replicas need their own later design.

```text
<registry>/
  writer.lock                 exclusive OS-backed lock (held while open)
  objects/<sha256>            immutable bytes; verify SHA-256 on access
  events.log                  CEWREG/1 event records, hash-linked
  HEAD                        CEWHEAD/1 committed offset/sequence/tip SHA
  cursors/<consumer>.cursor   CEWCURSOR/1 acknowledged sequence/tip SHA
```

**Object commit:** write a temporary file, sync the file, atomically rename to SHA-256 object ID, sync its parent directory where supported, then verify bytes at that hash. An unfinished temporary object is not visible as a committed CAS object.

**Event commit:** append one complete hash-chained event to `events.log`, sync it, write a sealed `HEAD.tmp` containing the new committed byte offset, event count and tip hash, sync and atomically rename `HEAD.tmp` over `HEAD`, sync the directory where supported. Only **after** `HEAD` is committed do subscribers receive the new event. A synchronous delivery callback is not a network delivery acknowledgment.

**Restart:** acquire the exclusive writer lock, validate `HEAD` and committed journal prefix, check every referenced object digest, and **truncate only bytes beyond HEAD**. If a committed prefix or `HEAD` is corrupted, fail closed. An unsupported or truncated `CEWREG/1` legacy store without a `HEAD` is rejected rather than guessed. A complete, validated V1 journal can be upgraded in place under exclusive operator control; the historical bytes and hash chain remain unchanged.

**Crash after HEAD commit but before API acknowledgment:** reopening finds the committed event; retrying the same idempotent publish returns its original event hash without a duplicate. Crash before the HEAD rename leaves an orphan object or uncommitted journal tail, never a visible committed event. The head hash is an integrity checksum, **not a digital signature**.

### Durability and portability limits

Linux/Unix uses `fsync` for objects, append log, HEAD temp and parent directories plus `flock` for exclusive ownership. Windows uses `FlushFileBuffers`, `LockFileEx` and `MoveFileExW` with `MOVEFILE_WRITE_THROUGH`; not all Windows filesystems expose a durable directory flush. These operations do not override disk firmware caches, network filesystem behavior or hardware power-loss characteristics. For regulated archival use, verify operating system/filesystem guarantees on actual deployed hardware; consider an established transactional engine with verified recovery behavior.

**Important V1 coexistence rule:** shut down all `ce_wind_registry` V1 processes before upgrading a directory. Legacy V1 clients do **not** honor SW2.1's new writer lock. A distinct/new registry root is the default safer choice. Never operate V1 and V2 writers against the same root. Existing V1 code should reject V2 roots containing a `HEAD` before attempting writes.

## 2. Persistent subscriber cursor

`acknowledge(consumer, sequence, event_hash)` writes a sealed `CEWCURSOR/1` file using an atomic temp-file commit. It rejects unknown event hashes, invalid identifiers and cursor regression. A restart can call `resume(callback, ctx, consumer, entity_filter, kind_filter)` to replay events **after** that cursor, then receive committed new events via callback.

**No automatic acknowledgement:** the consuming application explicitly acknowledges *after* its own durable downstream handling. A crash before ack causes at-least-once replay; the consumer must deduplicate by event hash. Cursor subscriptions are still same-process callbacks, not authenticated remote subscriptions; an externally managed network service will wrap this interface in SW3.

## 3. CEVPLAN/1 compatibility

The generic native `register_plan(plan, artifact_root)` contract accepts the existing validated CEVPLAN `Plan` type: verify its schema and every pinned learning/Crystal/RNA/support/visual object, persist immutable objects, then atomically commit a `PLAN_REGISTERED` evidence event. Artifacts may exist as unreferenced CAS objects after a crash; no plan is visible until the event HEAD commits. The old plan and evidence hashes remain unchanged.

## 4. CLI

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure

# New store or validated legacy migration (with old writer stopped).
ce_wind_durable verify ./durable-store

# Import an immutable digital research artifact; prints SHA-256.
ce_wind_durable import ./durable-store examples/vlm/data/rna-observation-demo.txt

# Publish a simulated observation referencing that immutable object.
ce_wind_durable publish ./durable-store RNA_OBSERVATION b-17 <sha256> SIMULATED 12 sim-clock

# Review event history and persistent consumer cursor.
ce_wind_durable events ./durable-store
ce_wind_durable cursor ./durable-store observer-1
ce_wind_durable catchup ./durable-store observer-1 b-17 RNA_OBSERVATION
ce_wind_durable ack ./durable-store observer-1 <sequence> <event-sha256>

# Validate and register existing CEVPLAN from pinned local artifacts.
ce_wind_durable register-plan ./durable-store examples/vlm/bread-first.cevplan .
```

The default `ce_wind_registry` binary stays available for legacy testing, but **must not write SW2.1 roots**. Prefer the durable API for new systems.

## 5. Executed tests and acceptance gates

The new test `native/tests/durable_registry_tests.cpp` verifies: idempotent CAS writes and event retries; single-writer lock refusal; object integrity; V1 migration without history rewriting; fail-closed truncated legacy journal; four crash/commit boundaries (including an appended uncommitted tail and a committed record with lost acknowledgment); sealed-head corruption; committed journal tampering; explicit on-disk audit; persistent cursor acknowledgment, restart, and rejection of rollback/forged cursors; generic plan-registration contract; and successful replay after restart.

**Release gates:** Linux and Windows CMake/CTest must pass on the repository's exact PR head; separate sanitizer runs must pass; a crash harness should be extended with actual process termination and file-system fault injection. The code does not yet provide simultaneous multi-host writers, remote identities, signatures, permissions, chunking, GC, replication, or direct hardware acquisition.

## 6. Next steps

- Extend SW2.1 with committed snapshots and a bounded chunk manifest for large optical/MEA/audio files; test retention and restore without depending on one ever-growing journal.
- Build SW3 signed identity, capability authorization, QUIC/TLS 1.3 transport, per-peer cursors and event-triggered Merkle reconciliation; maintain strict separation from physical intervention permits.
- Add formally defined measurement/model fidelity records linking biological *observations* and uncertainties to simulation predictions and withheld evaluation data. Do not equate synthetic RNA fields or Crystal names with biological semantics.
