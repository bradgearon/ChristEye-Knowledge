---
title: CE-SW-13 — SIM3.1 Unified Living Laboratory and Replay Viewer
project: ChristEye Anaiero
version: 0.1.0
status: Native deterministic simulator and browser replay prototype
classification: DIGITAL_ONLY / synthetic, uncalibrated research surrogate
---

# CE-SW-13 — Unified Living Laboratory

## Purpose and integration

SIM3.1 builds on SIM3 (`CEVRUN/1`, CE-SW-12) and SW2.1 (durable WindStore). It adds versioned *synthetic* laws, immutable checkpoint-history indexing, SHA-pinned **read-only** CEVLM/2 overlays, synthetic source-profile evaluation, and a self-contained static 3D-projection replay viewer. These are deterministic data and software contracts, **not** an actual living digital twin or proof of RNA/spine physiology, knowledge transfer, genetic phenotype or biological viability. No equipment control, clinical inference, chemical dosing, infectious agent optimization, or neural stimulation occurs.

## Native interfaces

The new C++20 `lab_core.hpp` supplies `CEVLABFRAME/1` snapshots, `index_history` to WindStore, and explicitly synthetic `CEVMODEL/1` comparisons. `lab_vlm_import.hpp` reads existing `.avlm` sources after SHA-256 verification and projects cells, local molecules, six 8×8 symbolic Soil fields, connections, and active process state **without overwriting or merging physical identities**. `ce_colony_lab` implements `stream`, `export`, `overlay`, `index`, `compare`, and `compare-index`.

The persisted simulator saves a sealed checkpoint and per-tick history through the existing SIM3 runner. WindStore records each checkpoint as immutable content-addressed evidence with idempotent indexing. Viewer NDJSON exports are an interchange view over the committed snapshot history; imported source overlays remain read-only.

```
CEVRUN/1 configuration → SIM3 executable laws → committed snapshot history
                                                    │
                               WindStore object/event index
                                                    │
                        CEVLABFRAME/1 NDJSON replay
                                                    │
                      tools/sim3_viewer (browser)
```

## Viewer

`tools/sim3_viewer/index.html` provides an interactive *2.5D projection* with orbit, zoom, cell inspection, synthetic colony/circuit links, lineage, tombstones, and replay/pause. Source `.avlm` overlays can be loaded separately. It uses browser canvas rather than a full voxel or finite-element volumetric renderer. Files are loaded locally by the user's file picker; the viewer makes no upload or outbound request.

`replay_contract.js` is a pure browser/Node validator for file-loaded frames. It checks declared simulated origin, format, bounded numeric fields, stable run/configuration/law identity, unique cells and colonies, lineage references, known channel endpoints, and **contiguous monotonic synthetic ticks**. Malformed or cross-run files are rejected before rendering; it cannot verify the cryptographic snapshot hash without the corresponding canonical binary checkpoint, and an all-zero SHA field in the *UI fixture* is a placeholder, not evidence.

`viewer-demo.ndjson` and `overlay-demo.ndjson` are tiny **hand-authored UI fixtures**, not native runner outputs. The latter lets the viewer exercise six symbolic Soil heatmap layers with a selectable source colony. Actual data should be produced using the native CLI below. The fixture's zero hashes must never be presented as authenticated provenance or scientific validation.

```bash
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure

# Run the existing synthetic SIM3 colony, then export its committed history.
ce_colony_runner step examples/connected-gardens.cev ./run-state 5
ce_colony_lab export examples/connected-gardens.cev ./run-state ./replay.ndjson

# Export the read-only source projections for overlay rendering.
ce_colony_lab overlay examples/connected-gardens.cev ./run-state ./overlay.ndjson

# Optional: index checked snapshot history and overlay objects in WindStore.
ce_colony_lab index examples/connected-gardens.cev ./run-state ./store

# Start a local static viewer (no NPM bundle required), then choose replay.ndjson.
python -m http.server 8000 --directory tools/sim3_viewer
# Open http://localhost:8000

# Fast independent file-contract tests (Node 18+).
node tests/test_sim31_viewer_contract.cjs
```

The native runner's uncalibrated support/learning/stress models are adjustable only through explicitly versioned CEVRUN law parameters. A source material SHA-256 is a provenance key, **not a genetic/phenotypic decoder**. Any future model calibrated from blood, hair, or genetic material requires independently measured biological properties, privacy governance, uncertainty, comparison cohorts, and validation before scientific claims.

## Quality gates

- Native GCC/MSVC CMake and SIM3.1 tests pass on Linux and Windows.
- Every replayed snapshot has a consistent configuration digest, model revision, ordered tick, unique cell identity, and referential lineage.
- Invalid/oversized replay files, duplicate ticks, unknown colony links, invalid overlay layer sizes, and invalid origin labels fail closed in the browser validator.
- In the native runner, restarting from an existing committed checkpoint yields the same history and synthetic cell/lineage results, with deceased cells retained as tombstones.
- Missing/corrupted snapshots or wrong source hashes reject import/indexing.
- The viewer never modifies the original .avlm or SIM3 store, calls hardware, or claims biology is proven.

## Next milestones

SIM3.2: calibrated *candidate* model contracts, reliable object-level media streaming, optional WebGPU rendering for larger populations, and stronger source/audit provenance. The current module is a research software surrogate only.
