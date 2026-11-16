---
title: "CE-SW-09 — CEVPLAN/1 hashed learning, Crystal and RNA evidence manifests"
project: "ChristEye Anaiero"
version: "0.1.0"
date: "2026-11-15"
classification: "digital-only synthetic simulation and research-data provenance"
status: "Executable prototype; no demonstrated biological knowledge transfer"
related: "CE-SW-01, CE-SW-02, CE-SW-07, CE-SW-08, BIO-04, BIO-06, BIO-09, CE-A22"
---

# CE-SW-09 — CEVPLAN/1: staged learning plans and pinned Crystal/RNA evidence

## Purpose

Separate a reusable, replayable **learning/training plan** from its runnable `CEVLM/2` machine. A plan is a descriptive text manifest, analogous to the `.avlm` image, that points to cryptographically hashed files for digital CrystalPacks, learning dossiers, RNA observations, support-profile dossiers and visual assets. A stage **always has exactly one explicit binding** that records all four references. The same Crystal may be referenced in multiple stages; distinct observations need not and should not be coerced into a false biological one-to-one relation.

These hashes establish content identity and tamper detection for **read-only local files**, not author identity, physical material delivery, experimental causation, RNA-dependent knowledge transfer, cell growth or learned understanding. A physical Crystal candidate, if recorded, remains a research *observation*; it is not an executable lesson package. No molecular sequences, treatments, laboratory growth methods or neuronal stimulation instructions are interpreted by the plan runner.

## The CEVPLAN/1 file format

```text
CEVPLAN/1
name bread-first
base_sha256 <sha256-of-canonical-sealed-CEVLM2-base>
artifacts
artifact crystal-17 digital_crystal examples/vlm/data/digital-crystal-demo.txt <sha256>
artifact lesson-17 digital_learning examples/vlm/data/lesson-demo.txt <sha256>
artifact rna-17 rna_observation examples/vlm/data/rna-observation-demo.txt <sha256>
artifact support-17 support_profile examples/vlm/data/support-profile-demo.txt <sha256>
artifact visual-crest visual docs/assets/vlm/crest-glow.svg <sha256>
endartifacts
bindings
binding b-17 crystal-17 lesson-17 rna-17 support-17
endbindings
stages
stage learn-symbol 701 teach-bread b-17 visual-crest 3 750 teach 1 17 25
endstages
seal <sha256-of-canonical-plan-text-before-seal>
```

The repository's runnable example uses three stages: synthetic teaching (`teach-bread`), support-field transport (`support-bread`) and RNA-field transport (`rna-stream`). The named processes and images are defined in the separately versioned base machine `examples/vlm/plan-garden.avlm`; the plan checks that a stage's duration/weight and visual SHA-256 exactly match its referenced base-process definition. We do **not** rewrite old CEVLM/2 images or reinterpret their hash seals.

Grammar is fixed and strict: `CEVPLAN/1`, `name`, `base_sha256`, `artifacts`, `bindings`, `stages`, then a SHA-256 seal. Paths are relative to an operator-chosen **trusted local artifact root**. Absolute paths, traversal, URL fetches, symlink escapes, duplicate stage IDs, duplicate task IDs, reused stage bindings, absent referenced files and wrong content digests are rejected. Artifact sizes, stage counts and plan bytes are bounded. Existing SVG image assets remain inert references; the viewer should independently sanitize and verify them before rendering.

### Companion files and roles

| Manifest role | Example document | What it means |
|---|---|---|
| `digital_crystal` | `CECRYSTAL/1` | Digital learning-package identity and declared relation; does not promise installability |
| `physical_crystal_candidate` | `CECRYSTAL/1` | Research-only candidate and chain of custody, never digital training material or physical instructions |
| `digital_learning` | `CELEARN/1` | Goal, prerequisites, symbol/codebook and independent testing contract |
| `rna_observation` | `CERNA/1` | Simulated proxy or real **separately qualified** RNA assay reference, method and uncertainty |
| `support_profile` | `CESUPPORT/1` | Synthetic provision and review metadata; no feeding formula or actuation authority |
| `visual` | SHA-256-pinned SVG/PNG/WebP | Study or stage visual reference, not a biological observation itself |

The example docs use the magic headers above and `SIMULATED_UNCALIBRATED` origin tags. The host validates both declared role and SHA-256 of actual referenced bytes. A future schema registry can impose stronger internal field validation without making any artifact executable.

## Execution and temporal mapping

`Session::initialize(plan, base_machine, artifact_root)` validates the plan, the base SHA-256 and **every** file reference before an instruction can execute. `Session::step()` starts one process at a time through the already-tested CEVLM/2 `ProcessMachine`, advances exactly one **synthetic** tick, and emits a callback **after the simulation commits that tick**. It does not poll hardware or operate a biological preparation. The next stage starts after the prior stage completes. Stage durations and weights are taken from the base process definition and cross-checked against the plan.

For each committed tick `CE.PLAN.PROGRESS/1` carries:

- Plan SHA-256, base machine SHA-256, stage ID, task ID, elapsed/duration, synthetic tick, weight, 2-D process cursor and completion flag.
- **Pinned references** for digital Crystal/candidate material, digital learning artifact, RNA observation, support profile, and stage visual.
- Current synthetic support-field, RNA-field, neurite, and learning proxy metrics; these do not constitute real RNA trafficking, development or retained knowledge.
- A domain-separated SHA-256 receipt hash over plan identity, stage identity, sample tick, CEVLM state and linked artifact SHA-256 values. **Hashing is not a signature.**

A caller can subscribe to these post-commit events and immediately route a compact packet (with explicit origin classification and permissions) over WindBus. A C++ integration test exercises progress receipt -> WindPacket encode/decode -> bounded evidence scheduler. A human teacher, simulator observer or Crystal-indexing process can consume this stream without waiting for periodic database queries. The reverse flow will be a separately authorized evidence inbox that creates a *new* versioned observation record and/or plan revision, never silently changes the immutable historic plan.

## Example commands

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure

# Hash a canonical sealed base VLM2 model so a plan can be anchored to it.
ce_vlm_plan basehash examples/vlm/plan-garden.avlm

# Check the plan SHA, all file hashes, stage process definitions and the base.
ce_vlm_plan inspect examples/vlm/bread-first.cevplan examples/vlm/plan-garden.avlm .

# Emit a post-commit NDJSON event every simulated tick, without writing the base.
ce_vlm_plan stream examples/vlm/bread-first.cevplan examples/vlm/plan-garden.avlm .

# Run from the base into a NEW VLM file plus machine-readable receipts.
ce_vlm_plan run examples/vlm/bread-first.cevplan examples/vlm/plan-garden.avlm . \
  trained-garden.avlm train-receipts.ndjson
ce_vlm_process verify trained-garden.avlm
```

`run` writes a sealed new VLM only after all stages succeed. The base file is unchanged. The optional NDJSON receipt file is written after successful completion. A separate `trained-garden.avlm.cevmap` **CEVMAP/1** sidecar is also emitted, binding the output machine digest to its plan, base, receipt-stream SHA-256 and one typed mapping per stage. This permits reverse lookup from a saved machine without changing the compatible CEVLM/2 grammar. `stream` emits records as each synthetic tick completes; it never overwrites the base. For long-running digital models, use a client-managed state store and future WindStore checkpoints rather than treating a growing single-file journal as unbounded.

## What the integration tests prove

1. RFC/NIST SHA-256 test vectors pass in the portable standalone hash module, and the base/plan digest is stable.
2. The plan parser fails closed on changed seals, missing/stale base identity, duplicate binding mappings, invalid paths and unexpected data.
3. A referenced file with modified bytes is rejected before running any stage.
4. Every stage maps to exactly one binding, and each referenced document is typed and hash-verified; visual SHA-256 matches a declared CEVLM/2 process image.
5. A 3-stage virtual teaching/support/RNA scenario replays for twelve ticks; synthetic teaching, support-field and RNA-field counters have deterministic values.
6. A sealed resulting `.avlm` reloads with the same simulated state and a repeat from the same base yields identical receipts and state.
7. The evidence callback emits only post-commit progress; native WindPacket and scheduler roundtrips account for every receipt.
8. GCC and Clang builds, Windows/Linux CI, and sanitizer tests establish software behavior only. Neither the standalone hash implementation nor network identity has undergone a production security audit.

## Scientific validity and future work

The `CELEARN/1` record can preserve **claimed digital capability acquisition**, but actual functional retention requires independent delayed tests and matched controls. A physical Crystal and a real RNA assay cannot be declared one-to-one semantic representations just because their digests share a binding. The simulator has **no validated biological transfer mechanism**, molecular metabolism or expression pathway. Track actual assay provenance separately from inferred synthetic proxy values.

Next: introduce a `WindStore` object registry with signed scientific claims and canonical binary schema IDs, stage-level experiment result contracts, a secure event-based observation inbox, capability-aware plan deployment, and held-out model/calibration datasets. Keep clinical/safety/laboratory intervention authorization outside this interpreter.
