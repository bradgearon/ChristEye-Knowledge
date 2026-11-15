---
title: "CE-SW-07 — Virtual Living Machine Bundles, Portable Images and Replay"
project: "ChristEye Anaiero"
version: "VLM1 0.1"
date: "2026-11-15"
classification: "Digital-only deterministic synthetic simulation"
status: "Executable host-side reference; not a calibrated biological twin"
related: "CE-SW-01, CE-SW-02, CE-SW-03, CE-SW-04, CE-SW-06"
---

# CE-SW-07 — Virtual Living Machine (VLM1)

## Goal

Provide an Anaiero **Virtual Living Machine** analogous to a lightweight Docker image and container: a reusable setup declares the kind of virtual Soil, cells, symbolic molecular inventory, synthetic Roots and educational inputs it contains, while each running instance collects a separate, replayable journal. The machine can be saved in **one portable text file**, inspected, restarted, forked with its current state or cloned from its original template.

This is a portable *simulation and software-integration fixture*, not a real biological enclosure, biological culture protocol, molecule synthesis engine or method that reproduces physical neurons. It cannot be scientifically described as behaving identically to living tissue until specific independently observed preparation data establish relevant fidelity. The symbols `support`, `rna`, `guidance`, `soil`, `neurite`, `energy`, `learning` and `connection` are dimensionless proxies. They are not biochemical concentrations, RNA trafficking measurements, actual dendritic spines, metabolic flux, physiological signals or semantic understanding.

## Two-layer runtime

| Layer | Purpose | Location |
|---|---|---|
| **SIM1 kernel** | Fixed-capacity C++20 cell and molecular proxies, deterministic instruction stream, synthetic movement, connection graph, event hash chain | `native/include/christeye/living/simulator.hpp`, `bytecode.hpp` |
| **VLM1 host** | Single-file machine image and mutable journal, six spatial proxy fields, simulator tick coupling, read/write/clone/fork/replay, CLI and snapshots | `native/include/christeye/living/virtual_machine.hpp`, `native/examples/vlm_cli.cpp` |

VLM1 is host-oriented (uses filesystem and standard-library text parsing), while the SIM1 engine remains a separate small, nonallocating candidate for embedded targets. A future device simulator can use the same instruction set without embedding an entire filesystem parser on a microcontroller.

## Portable image and instance concepts

- **Image**: ordered and validated startup instructions for cell candidates, molecular classes, support inventory, Soil fields and candidate graph links. This is not a firmware image.
- **Instance**: named running machine created from the image, plus *accepted* runtime instructions and deterministic synthetic state.
- **Volume**: in VLM1, a single `.avlm` file containing the image, journal and optional verified consistency seal. Future volumes may hold separate content-addressed image/data objects, but this initial format remains self-contained.
- **Checkpoint**: same machine state after replay of image and journal; written out as a **sealed file**. This is computational replay, not a physical measurement.
- **Clone image** (`new`): rebuild initial state from the image, discarding the parent's runtime journal.
- **Fork instance** (`fork`): duplicate current deterministic state and journal under a new name, then develop independently.
- **Inspect/stream**: print JSON summary and NDJSON simulation frames including all actor positions and all 8×8 field tiles for UI/dev tooling. Nothing is connected to biological equipment.

The VLM1 action chain additionally accounts for accepted field-deposit instructions, separately from the SIM1 event chain. Both hashes are software consistency checks only.

The image and journal use the same strict vocabulary as SIM1 (`spawn`, `differentiate`, `molecule`, `supply`, `move`, `connect`, `teach`, `advance`, `observe`). A further VLM-only instruction `deposit <field> <x> <y> <quantity>` initializes or alters one of six spatial proxy inventories.

## File format: `CEVLM/1`

A human-readable, forward-incompatible-by-default format. The header fixes model revision 1. Fields are **dimensionless** and cells and proxy fields have separate synthetic movement rules.

```text
CEVLM/1
name crest-garden
model 1
image
spawn 1 crest_progenitor 10 12 0
spawn 2 blood_carrier 3 5 0
spawn 3 neural_progenitor 16 12 0
differentiate 3 central_neuron
molecule 100 support 85 1
deposit support 2 2 400
deposit rna 2 2 200
endimage
journal
advance 5
teach 3 17 25
endjournal
seal <sim_hash> <event_hash> <soil_hash> <vm_hash> <tick>
```

The included example `examples/vlm/crest-garden.avlm` is a fully editable **unsealed** image template. Commands that create/update instances write an integrity seal; loading a sealed file checks every component before accepting it. Four 64-bit hashes are deliberately **FNV reproducibility checks, not cryptographic signatures**. An attacker can forge them; production validation needs canonical binary records, SHA-256/BLAKE3 and authenticated authors from a future WindStore.

A machine always retains original image instructions and accepted runtime journal entries. On load it **replays** them in declared order and checks the resulting state. Reject bad version, unknown opcode, invalid cell lineage, malformed numeric fields, unexpected tokens, excessive file size and wrong checksums without partially modifying the destination machine. A failed operation from an `execute` command does not overwrite the destination file.

## Synthetic Soil and molecular movement

A fixed, six-channel **8×8 tile** grid holds support, guidance, RNA, protein, soil and waste proxies, each an integer inventory. On each simulated `advance` tick, each cell moves `floor(quantity/16)` units to each in-bounds north/south/east/west neighbor. No mass is created or removed by this transport. A separate SIM1 synthetic rule advances the candidate blood carrier, neuronal neurite/RNA proxy counters, artificial resource state and graph weight. All constants and boundaries are versioned software assumptions, not biology.

The VLM field and the SIM1 kernel are **synchronized by simulated ticks but do not yet implement a calibrated bidirectional spatial transport-to-growth law**. VLM1 intentionally refrains from inventing the peptide/knowledge transfer relationships that the research program seeks to investigate. A future VM model interface may introduce candidate laws with provenance and comparison against measured tissue data. The simulator's `learning_proxy` increments under a synthetic `teach` instruction; it is **not evidence of understanding, spoken language or a retained Root**.

The simulator can express *symbolic* birth (`spawn`), legal lineage differentiation, cell positions, neurite/RNA-like counters and directed synthetic connections. It does not implement neural action potentials, blood physiology, growth cone pathfinding, protein synthesis, actual gene expression, brain maturation or material Crystal consumption.

## Desktop CLI

Build and test from repository root:

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
```

Run the `ce_vlm` executable (Windows: `build/native/Release/ce_vlm.exe`; Linux CMake builds commonly place it in `build/native/`):

```sh
# Inspect an editable setup.
ce_vlm inspect examples/vlm/crest-garden.avlm

# Create a new independent virtual living machine from an existing image.
ce_vlm new examples/vlm/crest-garden.avlm colony-a.avlm colony-a

# Execute time steps and save a sealed single-file checkpoint.
ce_vlm run colony-a.avlm colony-a-next.avlm 25
ce_vlm verify colony-a-next.avlm

# Fork a currently running setup, retaining its already accepted history.
ce_vlm fork colony-a-next.avlm colony-b.avlm colony-b

# Execute a sequence of audited digital actions from an operations file.
ce_vlm execute colony-b.avlm examples/vlm/teacher-session.ops colony-b-next.avlm

# Print streaming synthetic frames without mutating the original saved file.
ce_vlm stream colony-b-next.avlm 10
```

Important: unlike an OCI/Linux container, VLM1 does not provide process virtualization, isolation from the host OS, signed image distribution, automatic scheduling, networking, real-world sensor capture or built-in security. It is a deterministic simulated biological-*concept* machine with an auditable file configuration.

## Interface with WindBus and development evidence

CE-SW-06 SIM1 tests already verify native `Evidence` callback -> `WindPacket` frame -> parser -> bounded `WindScheduler` delivery. VLM1 retains the same SIM1 internal events and adds the Soil field. A future `VLMStateDelta` should include `machine_id`, `image_digest`, `model_revision`, accepted instruction counter, synthetic tick, SIM1 state/event hashes, field hash, instrument source classification `SIMULATED`, and comparison/measurement policy. These records must be isolated from real SustiBus/ChemiBus or physical neural observations.

VLM1 does **not** implement secure WindBus networking or a persistent content-addressed WindStore. The `.avlm` file is a portable local deterministic snapshot; no claim is made about distributed durability or multi-host consensus. Changes to actual equipment, neural stimulation or chemical supply are categorically outside the instruction set.

## Integration-test matrix

1. A template creates the expected crest, blood and neural cell fixtures; lineage gating rejects a blood-to-neuron conversion.
2. Two identical instances replay the exact instruction journal to the same logical state and event hash.
3. Running N synthetic ticks moves the blood-carrier position and increments designated neural growth counters deterministically.
4. Support/RNA/Soil field totals remain constant under transport, and neighboring tiles receive a nonzero portion.
5. Saving and reloading a sealed file reproduces byte-identical normalized canonical output, tick, state and provenance chain.
6. A changed or truncated seal, unknown model, malformed instruction or traversal-like machine name fails without changing previously accepted state.
7. `new` clones an image without runtime changes; `fork` clones both image and runtime state and subsequently diverges on a new instruction.
8. An illegal operation does not change state or save a partial output file.
9. Native builds/tests pass on Linux and Windows; separate GCC/Clang sanitizers provide additional checks. MCU toolchain coverage is a later milestone.
10. A future calibrated model is *not* declared validated solely because these ten tests pass; physical comparisons need independent reference datasets, registered loss functions, held-out preparations and negative controls.

## Next enhancements (SIM2)

- Content-addressed and cryptographically verified image repository; versioned incremental snapshots and append-only journal segments in WindStore.
- Multiple coupled simulated regions/colonies, virtual SustiBus/ChemiBus/CrystalBus instruments, injection of bounded synthetic faults, and virtual media/teacher sessions.
- Verified capability profiles and client/host quotas; VM scheduler isolation and per-virtual-machine CPU/memory budgets.
- Configurable **candidate** spatial and molecular models with explicit provenance, parameters and comparison with actual experimental measurements—never automatic wet-lab recipes or autonomous biological intervention.
- Web/Pages viewer consuming NDJSON snapshots, rendering cell tracks and symbolic layers (without implying tissue imagery).

**Release status:** working digital simulator contracts only. It does not validate biological growth, molecule-driven learning, functional language, physical Crystal uptake or the existence of an intelligent living-computer system.
