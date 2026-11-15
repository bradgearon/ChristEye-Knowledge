---
title: "Program phases and document standard"
document_id: "CE-AN-P01-00"
program: "ChristEye Anaiero"
phase: "P01"
revision: "4.0 / 2026-11-15"
status: "Engineering research specification — not a validated biological protocol"
owner: "We Develop Software LLC"
---

# Program phases and document standard

> **ChristEye Anaiero · Phase P01 · 15 November 2026 · WDS research property**  
> [Phase index](README.md) · [Bibliography](../references.md) · [Registers](../registers/README.md)

*How to turn every Anaiero subsystem into a buildable, testable set of documents.*

Status: scoped engineering design and supplier-referenced research. Values tied to one platform are not transferable to another without compatibility qualification. Current tissue work: NOT EXECUTED. \[R01, R13-R16\]

# Design premise

Anaiero uses a modular digital control and education architecture with optional qualified neuronal/astrocytic preparations. The first physical objective is not a general mind: it is source-verified, electrically readable, and delayed-testable adaptive behavior. Biological Crystal transfer, peptide-coded data, self-directed tissue formation, and consumer deployment remain distinct unproven claims. \[R13-R16, R06-R07\]

# Program phases and release gates

| **Phase** | **Purpose**                                                  | **Work**                                                                                                           | **Exit artifact**                                                                 |
|-----------|--------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------|
| P00       | Basis, claims and oversight                                  | Lock definitions, source registry, claimed capabilities, permissions, ethical scope                                | Release plan, role dictionary, authority matrix                                   |
| P01       | Reference composition and interface contracts (THIS RELEASE) | Select grounded planar cell reference; identify per-module quantities, chemistry, structures, I/O and measurements | Cell budget, molecule inventory, WindEvent, unknown-parameter lock list           |
| P02       | Cell-free instrument and digital test harness                | Run host, E evaluator, H router, L limit controller, audit and simulated neural response                           | Zero-cell system passes clocks, permissions, fault injections and event integrity |
| P03       | Qualified planar living core                                 | Qualified partner characterizes C and verifies identity, support and MEA input/output                              | Source G1, maturation G2, interface G3 release records                            |
| P04       | Acquisition, Roots, and consolidation                        | Train frozen-reader reference task; compare delayed performance and interference to controls                       | Documented acquisition and explicit retention horizon; negative outcomes retained |
| P05       | Observer, support, recipient and joined teaching             | Independently qualify O, B, then S; shadow bridge; advice; teacher-off tests                                       | Useful marginal contribution; protected old skills; independent B test            |
| P06       | 3D Soil and diversified regional modules                     | Separate organoid line/shape, diffusion, coverage, regional mix and service qualification                          | Independent release for each preparation and interface version                    |
| P07       | Physical Crystal and molecular Word hypotheses               | Nominate bounded material carrier; establish reader, content linkage, retention and independent use                | Physically grounded results or recorded failure; no assumed portable memories     |
| P08       | Scaling, field lifecycle and governance                      | Serviceability, remote use, upgrade packs, ethics and supplier qualification                                       | Validated digital release; independent decision for any future live deployment    |

P02 digital engineering can proceed alongside the P01 biological reference work. P07 molecular research may begin as a distinct investigation without implying it is a precursor to organoid learning. Do not merge independently numbered experimental series from old research editions. \[R13-R16\]

# Mandatory design document layout (one per module)

| **\#** | **Required section**            | **Minimum substance**                                                                                         |
|--------|---------------------------------|---------------------------------------------------------------------------------------------------------------|
| 01     | Document authority              | ID, version, evidence F/X/H/D/A, authors, references, approved build revision, open unknowns.                 |
| 02     | Functional contract             | User need, measurable outputs, non-goals, digital/live/simulated mode.                                        |
| 03     | System boundaries               | Source, receiver, owner, permitted routes, safety interlock, separation/replay plan.                          |
| 04     | Bill of materials               | Vendor, part, compatible device, alternatives, sourcing, lot traceability, quantity and unit.                 |
| 05     | Cell census                     | Tissue identity, input viable count, subtype fractions, area/volume, final observed census, replication unit. |
| 06     | Molecular inventory             | Chemical identity, process role, stage, compartment, receptor, status, lot, assay and SOP reference.          |
| 07     | Anatomical/structural inventory | Organoid/planar geometry, ECM, neurites, synapses, glial contacts and measured connectivity.                  |
| 08     | Developmental stages            | Production source, maturation, acceptance, training, retention, renewal; gate between stages.                 |
| 09     | Nourishment & exchange          | Separate support route, supply identity, consumption/waste, service schedule, alarms and sensor freshness.    |
| 10     | Electronic interfaces           | Electrodes, sampling and stimulus profiles, clocks, ground, readout quality, vendor permit and shutdown.      |
| 11     | Information protocol            | Codebook, WindEvent, training target, decoder state, state/label isolation, information budget.               |
| 12     | Learning program                | Baseline; teaching; probes; rest/intervening activity; delayed evaluation; curriculum progression.            |
| 13     | Data and software               | JSON schemas, calibration versions, signature and hash, provenance, reproducibility.                          |
| 14     | Diagnostics                     | Structural, electrophysiological, optical, health and task performance measures; unknown states.              |
| 15     | Controls and statistics         | Independent preparations, negative controls, power/precision, preregistration, sealed holdout.                |
| 16     | Failure and hazards             | Potential faults, observable signatures, response authority, no silent auto-rescue.                           |
| 17     | Resource budgets                | Cell supply, plates/chips, storage bandwidth, imaging, operator time, fluid service, energy.                  |
| 18     | Verification and release        | Bench/cell-free tests, living gates, evidence class, acceptance decisions, nonclaims.                         |
| 19     | Traceability                    | Cited primary data, supporting ChristEye documents, deviations and superseded claims.                         |
| 20     | Change control                  | Exact changed component, null dependencies, regression plan, independent approval and rollback limitations.   |

# Mandatory scientific companion layout

| **Chapter**               | **Required content**                                                                                                  |
|---------------------------|-----------------------------------------------------------------------------------------------------------------------|
| A. Mechanism map          | Specific molecule, receptor, process, tissue/compartment and timescale; distinction between observed and proposed.    |
| B. Evidence ledger        | F finding / X synthesis / H hypothesis / D design / A analogy, per statement. Named preparation and endpoint.         |
| C. Quantitative model     | Named state variables, units, identifiability, boundary conditions, sensitivity and mathematically valid alternative. |
| D. Experimental contrasts | Hypothesis, opposite predictions, negative controls, confounds, blinding, unit, sample-size planning.                 |
| E. Measurement plan       | Instrument, calibration, location, timebase, sample identity, perturbation consequences and uncertainty.              |
| F. Falsification          | Precise measured result that would reject or narrow the claim; null is not automatic equivalence.                     |
| G. Source appendix        | Inline source ID next to factual claim; DOI/URL and human-readable citation at end.                                   |
| H. Gate handoff           | What the qualified lab must determine, what software can validate, and required evidence to proceed.                  |

# Cross-document rule: no unproven transfer of numbers

- Reference cell counts are platform + plating geometry + source + culture-specific. A vendor seeding example is not a universal 3D organoid bill of materials.

- One physical component may instantiate several ChristEye roles; each assignment needs mechanism, scale, units and test.

- The biological Heart is response formation; L is an independent physical limit mechanism. Lotus can be both tissue-level coordination and H software orchestration, with separate identity records.

- Neural input stimulation, neurotrophic signaling, nourishment, and digital content delivery are different channels. A nutrient or peptide is not automatically a semantic data packet.

- No automatic propagation of physical Crystal claims into digital Crystal software. Cloned source line does not reproduce learned engram.

# Phase 1 ship checklist

- Canonical component inventory with source-confirmed, derived, and blocked quantities.

- One published reference 2D cell pairing plus alternate platform design locked as NOT COMPATIBILITY-QUALIFIED.

- Complete molecule/structure register with analytical identity and context, not recipe-by-metaphor.

- Message schemas and start/stop/handoff state machine for H, L and E.

- Daily observation and qualified maintenance logging; true living work remains a partner-laboratory SOP.

- A reproducible software validation for quantities, event contracts, and scientific status flags.

# Selected source register

[R01](../references.md#r01) BrainXell/Axion, Neuron/Astrocyte 24-well MEA Co-culture Protocol v10.0. Published platform-specific 50k-100k neurons/well; 2:1-6:1 neuron/astrocyte ratios; 80k+20k worked example; supported medium/service schedule. https://www.axionbiosystems.com/sites/default/files/resources/BrainXell%2024-well%20MEA%20Co-culture%20%20Neuron_Astrocyte%20Protocol%20v10.pdf

[R03](../references.md#r03) MaxWell Biosystems, MaxOne single-well HD-MEA specifications. 26,400 electrode sites, 1,020 simultaneous readouts, 20 kHz/channel, 19-mm inner ring, 3.85x2.10-mm active area, API and raw data. https://www.mxwbio.com/products/maxone

[R04](../references.md#r04) ISSCR, Standards for Human Stem Cell Use in Research, Stem Cell-based Model Systems. Require documented source, density, microenvironment, reproducibility, quality metrics and variability across preparations. https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems

[R05](../references.md#r05) ISSCR, Guidelines for Stem Cell Research and Clinical Translation, August 2025 v1.2. Ethics, provenance, oversight and reporting. 2025 revision focuses on embryo models. https://www.isscr.org/resources/guidelines-for-stem-cell-research

[R13](../references.md#r13) Brad Gearon, ChristEye Physical Learning Device Design v0.4 (provided). Current reference device; C, O, S, B, H, L, E and R0-R3 sequence. research/source_documents/R04_ChristEye_Physical_Learning_Device_Design_v0_4_3\_.docx

[R14](../references.md#r14) ChristEye, Research and System Design, revised synthesis (provided). Current device and Crystal research boundaries; experiments EX01-EX12. research/source_documents/R02_02_Research_and_System_Design_1\_.docx

[R15](../references.md#r15) ChristEye, Formal Models and Reconciliation (provided). Corrected equations, accounting, identifiability and unsupported analogies. research/source_documents/R03_03_Formal_Models_and_Reconciliation_1\_.docx

[R16](../references.md#r16) ChristEye, Technical Companion (provided). Biochemical and structural candidate roles and evidence labels. research/source_documents/R01_01_Technical_Companion_1\_.docx

All primary source identifiers and the original documents are also preserved in the repository research directory. Earlier source syntheses do not substitute for biological confirmation.

