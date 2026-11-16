---
title: "CE-SW-08 — VLM2 Process Timelines, Weighted Actions and Visual References"
project: "ChristEye Anaiero"
version: "0.1.0"
date: "2026-11-15"
status: "Executable deterministic digital surrogate, not a calibrated biological model"
related: "CE-SW-06, CE-SW-07, CE-SW-01, CE-SW-02"
---

# CE-SW-08 — Process-linked Virtual Living Machines

## Purpose

Extend the portable CEVLM/1 **single-file** virtual colony into `CEVLM/2` so a synthetic `spawn`, `molecule` or `deposit` can be attached to a named process and an optional viewer-facing image. Each process has **duration**, **weight**, **movement** and **completion** semantics. The process state is deterministic, saved by replayable journal, streamed as NDJSON to the website viewer, and tested through native WindPacket/Scheduler integration. The original CEVLM/1 machine format remains supported by the existing `ce_vlm` executable.

The model makes software invariants reproducible; it does **not** reproduce biological kinetics, prove neural language or semantic information in molecules, encode real peptide synthesis, or specify tissue growth, feeding or stimulation. All support, RNA, molecule and neurite quantities are synthetic dimensionless proxies.

## New format

```text
CEVLM/2
name process-garden
model 1
assets
asset crest-glow sprite docs/assets/vlm/crest-glow.svg
asset support-flow heatmap docs/assets/vlm/support-flow.svg
endassets
processes
process crest-birth spawn 4 850 1 0 crest-glow
process support-bread deposit 4 720 1 0 support-flow
endprocesses
image
spawn 1 neural_progenitor 10 12 0
endimage
journal
start 101 crest-birth spawn 10 crest_progenitor 10 12 0
start 103 support-bread deposit support 2 2 400
advance 2
do teach 1 17 25
endjournal
seal <v2_state_hash> <v1_kernel_state_hash> <tick>
```

A full example lives at [`examples/vlm/process-garden.avlm`](../../examples/vlm/process-garden.avlm), with five process definitions and five inert SVG visual markers in [`docs/assets/vlm`](../assets/vlm/). This format is intentionally **versioned and separate** from CEVLM/1 to avoid silently changing the meaning of previously sealed files.

### Parser grammar and caps

- `asset <id> <icon|sprite|heatmap> <relative-path.svg|.png|.webp>`: inert visual reference. No absolute paths, traversal, URLs, scripts, file reads or untrusted rendering inside the native kernel. The viewer must sanitize/serve its own approved assets.
- `process <id> <spawn|molecule|deposit|differentiate|connect|teach|supply|move> <duration_ticks> <weight> <dx> <dy> <asset_id>`: duration 1–512 synthetic ticks; weight 1–1000 (processing/event order, **not** biological strength); per-tick dx/dy -8..8, finite signed world coordinates.
- `start <unique_task_id> <process_id> <ordinary_vlm1_action...>`: action must match the process verb; create an **active pending task** whose progress and sprite cursor are visible, not yet mutate its destination cell/field.
- `advance <ticks>`: advance the simulated world one tick at a time; move active process cursors; resolve simultaneous due process actions in descending weight order, tie-breaking by task ID. Each task completes once. A `spawn` uses the final process cursor as its new synthetic coordinate; a `deposit` affects the final field tile, if in bounds; other VLM1 operations use their declared operands.
- `do <ordinary_vlm1_action...>`: commit an immediate simulated action while timed tasks are in progress, including teaching. No `do advance`: time passes through the process-aware `advance` path.
- The source image still accepts old VLM1 image actions, and process definitions/assets are embedded in the same portable file. A failed operation or bad file **does not mutate the active machine**. Invalid simultaneous completions roll back the affected `advance` transaction.
- Caps: 64 definitions, 64 inert assets, 128 task instances, 1024 journal commands, 4096 per-task tick events, existing VLM1 cell/molecule/evidence bounds. No dynamic or executable plugins are accepted from the config.

## Observation, movement and visualization

For each tick, the process engine publishes an ordered synthetic timeline entry:

`tick, task_id, process_id, operation, sprite_asset_ref, elapsed, duration, weight, x, y, completed, state_hash`.

`ce_vlm_process stream` produces NDJSON with process progress, sprite file paths, cell positions, SIM1 synthetic neurite/RNA/energy counters, candidate connections, full six-channel 8×8 synthetic field tiles and a state hash. A Pages viewer can draw icons, arrows and heatmaps for each frame; an asset is a viewer reference, **not** a neuron image. No observer may silently label this as direct biology.

For independent scientific experiments, named model assumptions, comparison datasets and withheld evaluation are necessary before any part of this synthetic process can be interpreted as a reliable biological predictor.

## Upgrade old virtual gardens

`ce_vlm_process upgrade` accepts a legacy CEVLM/1 file, takes process and asset definitions from a CEVLM/2 catalog template, and writes a **new** V2 machine. The original file is untouched. The importer replays its image and runtime journal, preserving the synthetic cell/field/lesson results while creating new V2 process-aware event hashes. V1 and V2 hashes are not identical by design; this is a new execution model.

## Commands

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure

# Explore a V2 setup; create an independent running machine.
ce_vlm_process inspect examples/vlm/process-garden.avlm
ce_vlm_process new examples/vlm/process-garden.avlm copy.avlm copy-garden
ce_vlm_process run copy.avlm running.avlm 4
ce_vlm_process stream running.avlm 5 > live-frames.ndjson
ce_vlm_process verify running.avlm

# Start a timed process and later inspect its progress.
ce_vlm_process start copy.avlm waiting.avlm 200 crest-birth spawn 50 crest_progenitor 10 12 0
ce_vlm_process run waiting.avlm finished.avlm 4

# Add an immediate digital teaching action.
ce_vlm_process execute finished.avlm taught.avlm teach 1 17 25

# Import a CEVLM/1 machine into a V2 process catalog without overwriting it.
ce_vlm_process upgrade examples/vlm/crest-garden.avlm examples/vlm/process-garden.avlm upgraded.avlm upgraded-garden
```

CLI `stream` advances a private in-memory copy and does not modify the input file; `run`, `start`, `execute`, `new`, `fork` and `upgrade` write a **new sealed** file. Use a fresh destination path to keep provenance straightforward. The FNV hashes are reproducibility checks, not signatures and not tamper resistance; network durability and authenticated artifact signing belong to WindStore.

## Integration quality gates

1. Strict parser rejects invalid process definitions, unknown asset refs, traversal paths, duplicate task IDs, malformed section structure, and unknown verbs.
2. Pending tasks do not prematurely modify the synthetic cell/field model.
3. Duration and per-tick 2-D motion produce the same final cursor and committed action after restart/replay.
4. Weighted simultaneous completion ordering is deterministic and priority is not confused with scientific evidence or biological strength.
5. Invalid completion rolls back the entire requested `advance` batch.
6. Sealed V2 files round-trip byte identically and fail closed when corrupted; `new` resets journal, `fork` retains it.
7. V1 import preserves cell identity and aggregate proxy fields/lesson outcomes, but deliberately records V2 hashes.
8. Synthetic events can be encoded as typed WindPacket observations and delivered through the bounded evidence scheduler, with origin classification `SIMULATED_UNCALIBRATED`.
9. Linux/Windows CI exercises both native V1 regression tests and new V2 process tests; sanitizer validation is a separate hardening gate.
10. The site renderer receives only process/asset references after path/consent checks; no arbitrary external assets or executable code are loaded from `.avlm` files.

## Next milestones

- **Process catalogue digests**: cryptographic content-addressed signed process/library manifests, asset bundles, policy-authenticated imports, WindStore persistence.
- **Spatial models**: separately versioned, calibrated candidate migration and transport laws with uncertainty and control datasets; never turn illustrative constants into wet-lab instructions.
- **Multi-colony scheduler**: isolated per-VM quotas and event-based synchronization between virtual colonies.
- **Website viewer**: animate SVG markers over synthetic cells and Soil heatmaps; compare two cloned instances and their state deltas, while identifying all data as simulated.
- **Experiment instrumentation**: compare simulator output against read-only, independently qualified datasets. Reproducible software state is a necessary engineering condition but insufficient scientific validation.
