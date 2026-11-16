---
title: "CE-SW-10 — WindStore Learning, Crystal and RNA Evidence Registry"
project: "ChristEye Anaiero"
version: "0.1.0"
date: "2026-11-15"
classification: "Digital-only storage / simulated and unverified research data"
status: "Executable host-side V1, single-writer local reference, not production durability"
related: "CE-SW-01, CE-SW-02, CE-SW-08, CE-SW-09, CE-A20, CE-A22"
---

# CE-SW-10 — WindStore Learning and Crystal Registry

## Purpose

Implement the next step after `CEVPLAN/1`: a **local content-addressed object store**, a hash-linked append-only **evidence inbox**, a **push callback** for new observations, a **plan importer**, and immutable **successor candidate manifests**. The registry can resolve any imported artifact by SHA-256, including digital Crystal, Bread/Meat learning dossier, RNA observation, support profile, SVG, sealed plan and successor proposal.

**This is a digital/software integration capability, not biological knowledge transfer or proof that RNA/spines store a particular word.** The plan runner and registry do not synthesize peptides, administer support, stimulate neurons, handle viruses or control living equipment. A `SIMULATED` event is never relabeled a biological measurement. The `IMPORTED_UNVERIFIED` origin prevents unauthenticated external evidence from appearing qualified.

## Storage profile

- `objects/<64 lowercase sha256 hex>` — immutable exact bytes, verified on every read. SHA-256 identifies bytes, **not author**.
- `events.log` — `CEWREG/1` followed by one strict immutable event per line. A record includes monotonically increasing `sequence`, `previous_hash`, `kind`, `entity`, referenced object SHA, `origin`, `source_tick`, clock ID and SHA-256 chain digest.
- Repeated identical publishes return the original receipt hash without notifying subscribers again. A different observation time is a new event.
- A registry owner replays and verifies each event, its previous digest and its referenced object before exposing state. A corrupted/truncated journal fails closed; V1 does not invent a repair, rewind or silent truncation.
- A single writer owns a directory. Concurrent multi-process writes, locking, secure sharing, authenticated peers, object garbage collection, encryption, access-control lists, database indexes, filesystem `fsync` durability, and power-loss recovery are **not** implemented yet. Successful flush/close is not the production `PERSISTED` receipt defined in CE-SW-01.
- Individual source objects are size-bounded (4 MiB in V1). The journal and local event capacity are also bounded. Large MEA/audio/image acquisition will use chunked object manifests in a later phase.

## API and events

C++20 header: `native/include/christeye/living/wind_registry.hpp`.

`open(path)`, `put(bytes)`, `put_file(path,expected_digest)`, `get(digest,bytes)`, `register_plan(plan,artifact_root)`, `publish(kind,entity,object_sha,origin,source_tick,clock)`, `subscribe(callback,context,after_sequence,entity_filter,kind_filter)`, `inbox(entity,after_sequence,kind)`, `verify()`.

Supported kinds: `PLAN_REGISTERED`, `RNA_OBSERVATION`, `LEARNING_RESULT`, `SUPPORT_OBSERVATION`, `CRYSTAL_EVIDENCE`, `SUCCESSOR_PROPOSED`.

The registry permits three untrusted/digital origin classes: `SIMULATED`, `IMPORTED_UNVERIFIED`, and `INFERRED`. A production `QUALIFIED_OBSERVATION` status must have a separate authenticated source adapter, calibration authority, approved method and provenance verification. No caller can simply set that status here.

Each local subscription replays missed events from its exclusive last-known sequence and receives later publications synchronously. It is **event driven, not a timed poll**. Filter by a plan/stage/binding/candidate entity and event kind. In V1 the callbacks are same-process and single-thread/owner: they are **not** authenticated remote push subscriptions or durable exactly-once delivery.

## Plan mapping and successor plans

`resolve_stage(plan, stage_id)` returns the exact `plan`, `crystal`, `learning`, `rna`, `support`, `visual` SHA-256 values. This is an unambiguous *digital* one-stage-to-one-binding mapping; multiple stages may use a Crystal across different observations. No mapping falsely asserts a biological one-to-one encoding between RNA and semantics.

`register_plan` runs the CEVPLAN parser and verifies all referenced paths/content digests before importing the sealed plan and artifact objects. It publishes `PLAN_REGISTERED` for the plan **object SHA**; note this differs from `plan.hash()` over the unsigned canonical manifest. The library maintains both identities distinctly.

`successor_manifest(parent_plan_object_sha,learning_sha,evidence_sha,candidate_id)` creates:

```text
CEVSUCCESSOR/1
parent_plan_object_sha256 <digest>
learning_object_sha256 <digest>
evidence_object_sha256 <digest>
candidate <id>
state PROPOSED_ONLY
seal <sha256-of-preseal-lines>
```

This deliberately does **not** mutate the parent, change a running machine, or install a new lesson. A separately approved compiler can later materialize a reviewed successor `CEVPLAN/1` with explicit new bindings, frozen independent evaluation and a new hash. `SUCCESSOR_PROPOSED` is only a candidate-review event.

## Example CLI

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure

# Creates a local registry if absent; verifies its journal.
ce_wind_registry verify ./store

# Archives a validated plan and every hash-pinned artifact it references.
ce_wind_registry register-plan ./store examples/vlm/bread-first.cevplan .

# Import an additional read-only RNA or learning observation file.
ce_wind_registry import ./store examples/vlm/data/rna-observation-demo.txt

# Publish a synthetic observation with an already verified object SHA.
ce_wind_registry observe ./store RNA_OBSERVATION binding-17 <sha256> SIMULATED 12 sim-clock

# Inspect hash-linked evidence receipts.
ce_wind_registry events ./store

# Propose a successor (does NOT rewrite the original plan).
ce_wind_registry successor ./store <parent-plan-object-sha> <new-learning-sha> <evidence-sha> candidate-next
```

No network I/O occurs. The input root is operator-supplied, and the example command paths use the working repository root.

## Tests and operational claims

Native tests exercise CAS idempotence, verified peer-object *copy* (not network sync), duplicate publish suppression, subscriber catch-up and filters, stage binding lookup, importing the actual CEVPLAN fixtures, immutable parent retention, successor candidate records, WindPacket encoding/decoding and weighted scheduler delivery, sealed journal replay, corruption detection and fail-closed startup. GCC and Clang/ASan/UBSan can verify software invariants; they cannot prove biological fidelity.

## Planned SW2.1 production hardening

1. Transactional journal and object commits with crash-consistent `fsync`/directory flush semantics per OS, partial write detection and recovery modes.
2. Separate signed credentials and scoped authorization for remote observe/subscribe/replicate APIs; authenticated network transport; rate quotas and replay defense.
3. Chunked large-object transfer by hash, bounded retention, multi-tenant isolation, garbage collection and per-peer replication cursors.
4. Schema-versioned accession and ClockAnchor records, signed calibration and source provenance, tenant-specific scientific confidence separate from transport authority.
5. More precise CEVPLAN successor compiler that stages a new plan without altering the parent, enforces independent held-out Root tests and publishes a reviewed promotion receipt.
6. Hardware read-only acquisition adapters and validated environmental monitoring data; no autonomous culture care or neuronal stimulation.
