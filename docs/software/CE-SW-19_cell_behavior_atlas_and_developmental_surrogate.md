---
title: "CE-SW-19 — Evidence-linked Cell Behavior Atlas and Digital Developmental Surrogate"
project: "ChristEye Anaiero"
version: "SW5.0"
status: "Executable digital reference only; uncalibrated synthetic behaviors"
classification: "Reproducible simulation, immutable observational provenance, no physical intervention"
related: "CE-SW-06, CE-SW-08, CE-SW-14, CE-SW-15, CE-SW-18, CE-A09, CE-A17, CE-A22"
---

# CE-SW-19: Designing increasingly faithful computational cell behavior

## 1. Goal and scientific boundary

The project's next step is an **evidence-linked digital cell atlas**: a versioned behavioral parameter record for each simulated cell archetype, a deterministic service that advances a group of passive cell-state objects, and explicit links to observations used to calibrate or refute each rule.

Software success is not biological fidelity. This first release **does not encode a validated cellular transfer function, culturing protocol, cloning procedure, biological stimulation, disease model or mechanism for acquiring language in neurons**. All numeric quantities in `CEVCELLATLAS/1`, `CEVCELLRUN/1`, and `CEVCELLSTATE/1` are **dimensionless digital proxies**. The simulation's `support` is a numeric input and not nutrient composition or feeding instructions. The `signal` is a synthetic activity feature and not a physical stimulation pattern. `digital_learning` is a counter, not memory or understanding.

### What would make the simulation closer to real life?

We need to fit each mechanism **to the type of evidence it actually predicts**, and then test it on independently held-out specimens/measurements:

| Behavior target | Potential observation domain | Required questions before claiming a fit |
|---|---|---|
| Cell identity and developmental stage | Single-cell annotations and expression; cell-lineage tracking | Same species, region, preparation, batch and stage? Annotation or directly traced lineage? |
| Cell shape and migration | Time-resolved microscopy / segmented morphology | Are trajectories measured? Are imaging conditions, resolution and cell-tracking uncertainty recorded? |
| Neurite geometry and contact | Morphometric reconstructions and connectome traces | Are projection and receiving cell identities actually linked? Structural contact is not functional integration |
| Neural activity and responsiveness | Electrophysiology and calcium imaging | Does stimulus/acquisition time align? Is the response observed or inferred, and what is its uncertainty? |
| Glial and other support relationships | Validated independent observations | Which compartments/cells are measured? Avoid interpreting concentrations as nutrient flux or learning |
| Retained task-specific function | Prespecified task, delayed response tests, negative controls | Can a frozen independent decoder replicate the result without reading labels or using a training leak? |

Cell-type labels, morphology, electrophysiology and transcriptomic measurements provide *different views* and cannot be merged automatically into a single individual or a mechanism. Source-material hashes prove byte identity, not genotype-to-phenotype prediction or a physical simulation's accuracy.

### Standards that the provenance layer should link to

- **NeuroML:** simulator-independent neuronal morphology, membrane and network model descriptions. https://docs.neuroml.org/
- **NWB:** observations, timebases and metadata for neural physiology, imaging and behavior. https://nwb.org/
- **CZ CELLxGENE Census:** annotated single-cell expression matrices and standardized cell metadata (not a specification of how the cell behaves). https://chanzuckerberg.github.io/cellxgene-census/
- **OME-NGFF/OME-Zarr:** metadata and image-array standards for bioimaging, appropriate for source observations and 3D segmentation assets. https://ngff.openmicroscopy.org/
- **SONATA:** neuronal simulation nodes, edges and related configuration; for network-model interoperability. https://github.com/AllenInstitute/sonata

This release only **records and validates a description of each reference**; it does not ingest those large formats or derive physiology from them. Actual importer plugins should be independently reviewed read-only transformations, each producing a canonical, evidence-qualified feature record with a versioned uncertainty budget and unit/time semantics. Use the existing SIM3.2/SIM3.3 frozen training/holdout evaluation workflow for model selection.

## 2. Data-only models; domain services own logic

- `native/include/anaiero/models/cell_behavior.hpp`: passive bounded `Cell`, `Atlas`, `Parameters`, `Link`, `RouteIntent`, `Context`, `World`, `Metrics`. No mutation, growth or bus logic in these records.
- `native/include/anaiero/contracts/cell_behavior.hpp`: typed event and movement interfaces. No biological actuator commands are present.
- `native/include/anaiero/services/cell_behavior_service.hpp`: single-owner deterministic developmental simulation; transition checks; synthetic maturation/process/activity/learning, viability and pairwise *candidate* network integration, history retention.
- `native/include/anaiero/infrastructure/cell_atlas_file.hpp` and `cell_source_record.hpp`: validate strict SHA-256-pinned parameters and scientific source declarations.
- `native/include/anaiero/infrastructure/cell_position_port.hpp`: synthetic position movement through `ISpatialPort`, independently testable for rejection and bounds.
- `native/include/anaiero/infrastructure/cell_checkpoint.hpp` and `cell_run_plan.hpp`: persistent source-pinned snapshots, immutable plan digest and resuming within a staged digital training session.
- `native/include/anaiero/infrastructure/cell_bus_adapter.hpp`: post-commit synthetic events through **existing** WindPacket/1 into the SW4 weighted ColonyState projector. Cross-process mesh and live equipment drivers are not implemented here.
- `native/examples/cell_behavior_cli.cpp`: internal `ce_cell_behavior` backend, exposed publicly as `anaiero cells ...`.

The core uses fixed capacities of **64 cell records, 128 directed candidate links, 32 guidance intents, and 256 receipts per tick**. No allocation is performed by the behavior update rule itself; sealed-file parsing, command output, and synchronous persistent writes live outside this path. The single-owner model permits deterministic replay but is not thread-safe. The runtime's latest-state projector remains the separate concurrent-reader component.

## 3. Portable configuration and provenance formats

### `CEVCELLATLAS/1` (immutable parameter record)

```text
CEVCELLATLAS/1
name crest-blood-neural
domain SYNTHETIC_ONLY
model 1
reference_sha256 <sha256-of-evidence-descriptor>
reference_file synthetic-observations.cevsource
reference_format SYNTHETIC
cell crest_progenitor <maintenance> <support_response> <maturation> <process_extension> <activity_response> <plasticity> <mobility> <contact_radius> <stress_response>
... exactly one record for each of the nine archetypes ...
end
seal <sha256-of-preseal-bytes>
```

Nine archetypes: crest progenitor, blood carrier, neural progenitor, peripheral/central neurons, astrocyte, Schwann, microglia, endothelial. Rates and weights are limited digital integers (`0..32`) with arbitrary update semantics; they **are not** culture concentrations or biological timescales. No genotype or sequence fields are executable here. The package is rejected if its digest, type list, bounds, model version or ordering contract is violated.

### `CEVCELLSOURCE/1` (read-only observational reference)

```text
CEVCELLSOURCE/1
name synthetic-observations
standard SYNTHETIC
raw_sha256 <sha256-of-source-data-or-zero-if-unavailable>
modality multi-proxy
units DIMENSIONLESS
clock SYNTHETIC_TICK
uncertainty UNCALIBRATED
classification SYNTHETIC_ONLY
end
seal <sha256-of-preseal-bytes>
```

Other recognized standards are NWB, NEUROML, CELLXGENE, OME_ZARR and SONATA. Non-synthetic references must remain `RESEARCH_UNVERIFIED` at this tier. The SHA link is compared to the atlas's `reference_sha256`, and the runtime requires the referenced `.cevsource` sidecar in the same trusted directory; it is **not** an endorsement of the source or a calibrated model. Qualified real acquisitions require instrument, preparation, assay, consent, timestamp, sample lineage and uncertainty metadata in separate reviewed records. Human donor/source material should not be stored in public fixtures.

### `CEVCELLRUN/1` (digital developmental scenario)

```text
CEVCELLRUN/1
name reference-raising-and-learning
domain SYNTHETIC_ONLY
atlas_sha256 <sha256-of-sealed-atlas>
stage developing 6 75 0 0 0
stage guided-contact 6 75 0 64 1
stage retention-probe 6 75 0 0 0
end
seal <sha256-of-preseal-bytes>
```

Stage fields are `[name, simulated_ticks, synthetic_support, synthetic_stress, digital_signal, lesson_active]`. These are **simulation command inputs**, not physical stages, culture media, dosing, conditioning, or electrical waveforms. Stages have fixed timing and the result is reproducible. When the simulation is interrupted, `CEVCELLSTATE/1` stores a pinned plan digest and tick, so restarting with a different plan is rejected; re-running the same already completed plan is idempotent. For physical biological preparations, the observer path has no operation that applies any of these inputs to instruments.

### `CEVCELLSTATE/1` (durable digital state)

Each snapshot includes atlas digest, plan digest (`none` if unbound), simulated tick/generation, all cell states and parent references, candidate graph links, route intents and a SHA-256 seal. The host writes an atomic checkpoint **after each accepted digital tick**, and restores it only after verifying the atlas binding and history invariants. Nonviable cells remain as tombstones with death tick. This is a bounded, single-machine digital checkpoint—not a durable full journal of physical biology. For an append-only history, put the snapshots into SW2.1 WindStore.

## 4. Synthetic behavior and network development

Each digital tick follows explicit bounded rules: independent synthetic support/activity inputs are applied through a domain service; energy and viability update; maturation and process proxies change; activity and task-associated learning proxies update; mobility goes through an infrastructure position adapter; a directed contact may be added only if an explicitly requested digital pairing passes state, process, distance and guidance checks. Eligibility is not proof of synaptic formation, functionality or learned competence.

`transition(id, target)` and `daughter(parent, child)` are **digital bookkeeping operations** constrained by minimal archetype relationships and synthetic maturity. They are not biological differentiation/reproduction recipes. `stress` is a general abstract feature, not a pathogen, toxin, or disease simulation. A physical Animon's essential care remains exclusively controlled by the qualified, independent CE-A17/A22 care/safety service.

## 5. Validation boundaries and test coverage

- Parser rejects altered hashes, out-of-bounds parameters, mismatched source descriptors, invalid archetype lists and run plans.
- Deterministic state progression and root-like candidate connections; replay from an exact checkpoint reproduces the same future.
- Explicit digital lineage transitions and synthetic daughter creation preserve ancestry and reset the candidate's learning counter.
- Under source-depletion *synthetic assumptions*, cell states can become nonviable; tombstones persist.
- An infrastructure-rejected digital movement rolls back the entire step, not just the position.
- The `CellBusAdapter` sends post-commit events through WindPacket/1 and the weighted ColonyState RuntimeService; the projector produces coherent latest synthetic state without a physical actuator.
- Running a second time with the same sealed plan is idempotent; mismatched plans fail without mutating the state file.
- GCC/Clang and hosted Windows/Linux CTest must pass. No passing test constitutes evidence that the model replicates a real cellular system.

## 6. Realistic-model development roadmap

1. **Source adapters:** read-only import of selected public NeuroML, NWB, OME-Zarr and transcriptomic records; preserve raw hashes and standardized metadata, and detect unavailable features instead of guessing.
2. **Independent structural calibration:** fit observed morphology, motility, maturation markers, activity variability and connectivity on training data, retaining age/species/region and confidence. Avoid using observed lineage labels as simulation predictions.
3. **Multi-compartment / cross-cell behaviors:** reuse validated specialized models rather than assuming a single synthetic process counter accurately describes axons, dendrites, spines or glial signaling.
4. **Validation:** compare temporal trajectories, graph topology, variability, failure cases and delayed independent probes against frozen data, with SIM3.2/SIM3.3 training/holdout partitioning and transparent negative results.
5. **Runtime and scale:** store snapshots in WindStore, publish revisioned projector deltas to Linux clients, batch high-frequency signals and stress-test queue latency/memory on target controller. Keep physical actuation absent.

**Scientific success criterion:** a simulation predicts prespecified, independently acquired observations with calibrated uncertainty and reproducible improvements over simpler baselines. A photorealistic animation, a Crystal hash, a synthetically learned symbol or a functional data bus does **not** establish such accuracy.
