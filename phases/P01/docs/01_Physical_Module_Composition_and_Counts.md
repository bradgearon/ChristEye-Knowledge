---
title: "Physical Module Composition And Counts"
document_id: "CE-AN-P01-01"
program: "ChristEye Anaiero"
phase: "P01"
revision: "4.0 / 2026-11-15"
status: "Engineering research specification — not a validated biological protocol"
owner: "We Develop Software LLC"
---

# Physical Module Composition And Counts

> **ChristEye Anaiero · Phase P01 · 15 November 2026 · WDS research property**  
> [Phase index](README.md) · [Bibliography](../references.md) · [Registers](../registers/README.md)

*Reference module numbers, preparation variants, physical/digital census, and procurement logic.*

Status: scoped engineering design and supplier-referenced research. Values tied to one platform are not transferable to another without compatibility qualification. Current tissue work: NOT EXECUTED. \[R01, R13-R16\]

# What counts as a module?

The software registry contains 24 service modules (A01-A24), many specialist StarChips and only a limited number of physically distinct neuronal preparations. A software service, even if called healing, Heart, memory, or filesystem, starts with 0 assigned biological cells. A living cartridge has a physical sample, its own observations, support identity and biological release record. \[R13, R14\]

# P01 physically relevant module census

| **ID**    | **Module**                          | **When**             | **Neurons/well** | **Astrocytes/well** | **Total/well** |
|-----------|-------------------------------------|----------------------|------------------|---------------------|----------------|
| C         | Core learner                        | R1                   | 80,000           | 20,000              | 100,000        |
| O         | Observer learner                    | R2                   | 80,000           | 20,000              | 100,000        |
| B         | Recipient learner                   | R2 separate campaign | 80,000           | 20,000              | 100,000        |
| S         | Support-policy learner              | R3                   | 80,000           | 20,000              | 100,000        |
| G_local   | Local glia support                  | R1+                  | included         | included            | 0 additional   |
| Prod      | Cell bank/maturation service        | Before R1            | not per module   | not per module      | not additive   |
| 3D-C      | Future 3D organoid Soil             | P06                  | NOT SET          | NOT SET             | NOT SET        |
| Immuno    | Anti-disease / microglial research  | P06+                 | 0 in R1          | 0 in R1             | 0              |
| OPC       | Oligodendrocyte lineage study       | P06+                 | 0 in R1          | 0 in R1             | 0              |
| Neuromod  | Brainstem/neuromodulatory candidate | P06+                 | 0 in R1          | 0 in R1             | 0              |
| Crystal-P | Physical Crystal candidate          | P07                  | NOT SET          | NOT SET             | NOT SET        |

C — Supplier example: one Axion 24-well MEA well; motor-neuron + matched astrocyte reference, not full learning mesh.

O — Same starting population as C; monitoring identity must be earned by training and test.

B — Same preparation; teaching A→B, then physically separate electronic route, test B alone.

S — Same starting preparation; can advise allowed actions only, never autonomous fluid or molecule changes.

G_local — Astrocytes already included inside each module; no external G cartridge credited.

Prod — Supplier bank and laboratory lot history; count determined by enrolled wells, yield, and reserves.

3D-C — Different geometry, transport, and source; cannot inherit Axion planar values.

Immuno — Quality surveillance digital first; any added immune-lineage class is a separate scope and lineage qualification.

OPC — Only if axonal maintenance question justifies added population.

Neuromod — Named regional differentiation and functional validation required; no generic brainstem added.

Crystal-P — Carrier identity and boundary unproven, no seeding number can be assigned.

80,000 + 20,000 is the worked 4:1 example from the BrainXell/Axion 24-well MEA protocol, not Anaiero live qualification. That protocol permits 50,000–100,000 neurons/well and commonly 2:1–6:1 neuron:astrocyte ratios. Exactly 80k/20k is a candidate reference setpoint \*\*only\*\* if the full vendor-compatible cell pair, medium, substrate and Axion geometry are chosen and approved by the host lab. \[R01, R02\]

# Per-phase physical quantity and when it actually enters

| **Release**                   | **Living wells in a minimal illustrative configuration**                             | **Amount at reference 80k/20k**                      | **Scientific limitation**                                                                    |
|-------------------------------|--------------------------------------------------------------------------------------|------------------------------------------------------|----------------------------------------------------------------------------------------------|
| R0                            | 0; full digital mock / recorded replay                                               | 0 cells                                              | Can fully test source, host, evaluator, controller, schemas and fault paths.                 |
| R1                            | 1 C physical well as a demonstration; additional matched controls for learning tests | 80k neurons + 20k astrocytes per living well         | One well n=1, no confirmatory claim. Controls and batches determine procurement.             |
| R1 illustrative 24-well pilot | 4 separately declared conditions × 6 wells each = 24 wells                           | 1.92M neurons + 0.48M astrocytes plated; 2.40M total | Illustrative planning only; same batch/donor wells do not prove across-line reproducibility. |
| R2                            | 1 O well or 1 B well per separate comparator arrangement                             | Each 80k neurons + 20k astrocytes                    | Not automatically simultaneously joined; requires appropriate independent recording.         |
| R3                            | 1 S well added after O meets criterion                                               | 80k neurons + 20k astrocytes                         | Added module must improve an independently evaluated outcome.                                |
| P06                           | 3D organoid branch                                                                   | NOT SET                                              | 3D count, nutrient exchange and recording coverage require independent qualification.        |

# Reference count arithmetic and procurement

> reference_per_well = {"neurons": 80000, "astrocytes": 20000}  
> number_of_wells = 24 \# illustrative 4 groups x 6 wells  
> neurons_total = number_of_wells \* 80000 \# 1,920,000  
> astrocytes_total = number_of_wells \* 20000 \# 480,000  
> \# Additional cells for loss, sourcing and qualification require actual supplier/lot estimates.

Lot purchasing is based on live recovery, culture attrition, manufacturing waste, matrix coverage and confirmed repeat/batch counts, not just the arithmetic above. A 1M/5M cell cryovial label is a procurement unit, not the plated amount. Spinal motor versus interneuron identity cannot be inferred from a general neural marker. \[R01, R04, R10\]

# Why the first 2D reference is not the finished spinal learning composition

- Motor neurons + spinal astrocytes are an established electrophysiology and maturation starting point. They are not by themselves evidence for rich context learning. [R10](../references.md#r10)

- The proposed future target adds functionally characterized excitatory and inhibitory spinal interneurons, possible motor outputs and compatible regional astrocytes. Use a new composition version for each addition. [R13](../references.md#r13)

- Inhibitory identity and net inhibitory action must be measured separately; immature GABA signaling may not be inhibitory in the assumed direction.

- Astrocytes inside each living C/O/S/B preparation are local support participants. An electronically bridged support learner S cannot replace astrocytic processes in C.

- Microglia-like cells, oligodendrocyte-lineage cells, and neuromodulatory cell types are later comparisons, not mandatory first-release ingredients.

# The 24 software subsystems begin with zero cell counts

| **Software ID** | **Function**                           | **Biological starting count** | **May address live module**                               |
|-----------------|----------------------------------------|-------------------------------|-----------------------------------------------------------|
| A01             | Episodic and working memory            | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A02             | Long-term storage                      | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A03             | Healing/integrity adviser              | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A04             | Growth/capacity planner                | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A05             | Refresh/renewal                        | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A06             | Learning stabilization assessment      | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A07             | Learning enhancement curriculum        | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A08             | Health surveillance                    | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A09             | Main core software boundary            | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A10             | Usage Crystal compiler                 | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A11             | Connective controllers                 | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A12             | Lotus governor                         | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A13             | Crystal cloning                        | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A14             | Crystal consumption/replay             | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A15             | Organoid development planning          | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A16             | Root registry                          | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A17             | Daily care record/controller boundary  | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A18             | Bread/Meat/Water/Metabolic abstraction | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A19             | Central data router                    | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A20             | Filesystem/snapshots                   | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A21             | Affective support/Honey policy         | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A22             | Independent limits                     | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A23             | Molecular Word / Crystal research      | 0                             | Only through typed H/L gate; never assume dedicated cells |
| A24             | Observer stream software               | 0                             | Only through typed H/L gate; never assume dedicated cells |

# Structural stages and what actually forms

| **Stage**            | **Physical structure / transition to observe**                                                     | **Qualification evidence**                                                              |
|----------------------|----------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------|
| S0 source            | Authenticated lot and regional/subtype composition; source not equivalent to prior learned history | Consent, source lineage and genomic/identity record \[R04-R05\]                         |
| S1 receiving         | Attachment to chosen substrate, known cell locations, local ECM; morphology is not connectivity    | Coverage, cell viability, background and registered images \[R01, R13\]                 |
| S2 neurites          | Axon/dendrite polarization and growth; developing synaptic contact                                 | Spatial characterization, subtype evidence, measured physiology \[R10, R13\]            |
| S3 functional mesh   | Distinguishable spontaneous/evoked neuronal activity and response routes                           | Electrode map, SNR, artifact rejection, independent electrical characterization [R03](../references.md#r03) |
| S4 acquisition       | History-sensitive task response under locked reader and matched controls                           | Above-baseline held-out discrimination, feedback accounting [R06](../references.md#r06)                     |
| S5 Roots             | Delayed response with teaching disabled; protect old task after new task                           | Absolute delayed score and calibrated uncertainty \[R06, R13\]                          |
| S6 joined use        | Electronic bridge between independently characterized modules                                      | Off/on/shuffled/common-input checks and protected tasks \[R11, R14\]                    |
| S7 candidate Crystal | Defined physical material relation to retained content, if any                                     | Carrier, boundary, reader, alternative causes; not assumed \[R15-R16\]                  |

# Blocking choices before any live construction

- Select Axion 24-well supplier-protocol reference \*\*or\*\* MaxOne high-density single-well reference as the primary R1 platform; they use separate numerical baselines.

- Confirm selected neurons/astrocytes are supplier-validated together in one medium on that platform and select exactly one reference protocol version.

- Obtain institutional source/consent, culture SOP, control assignments, release and stop rules.

- Source biology cannot be made a fixed machine-generated count without measured actual yield; the release document must identify claimed counts, plated counts and post-attachment counts independently.

- The only present fixed live count is the Axion planar worked example; all 3D-specific counts remain a blocked controlled parameter.

# Selected source register

[R01](../references.md#r01) BrainXell/Axion, Neuron/Astrocyte 24-well MEA Co-culture Protocol v10.0. Published platform-specific 50k-100k neurons/well; 2:1-6:1 neuron/astrocyte ratios; 80k+20k worked example; supported medium/service schedule. https://www.axionbiosystems.com/sites/default/files/resources/BrainXell%2024-well%20MEA%20Co-culture%20%20Neuron_Astrocyte%20Protocol%20v10.pdf

[R02](../references.md#r02) Axion Biosystems, BrainXell 24-well MEA co-culture protocol landing page. Vendor protocol source and specified compatible MEA architecture. https://www.axionbiosystems.com/resources/culture-protocol/neuronastrocyte-24-well-mea-co-culture-protocol-v100

[R03](../references.md#r03) MaxWell Biosystems, MaxOne single-well HD-MEA specifications. 26,400 electrode sites, 1,020 simultaneous readouts, 20 kHz/channel, 19-mm inner ring, 3.85x2.10-mm active area, API and raw data. https://www.mxwbio.com/products/maxone

[R04](../references.md#r04) ISSCR, Standards for Human Stem Cell Use in Research, Stem Cell-based Model Systems. Require documented source, density, microenvironment, reproducibility, quality metrics and variability across preparations. https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems

[R06](../references.md#r06) Robbins et al., Goal-directed learning in cortical organoids, Cell Reports 2026. Adaptive feedback learning on task, but no retained improvement after tested 45-minute rest. https://pubmed.ncbi.nlm.nih.gov/41720084/

[R10](../references.md#r10) Taga et al., Human spinal astrocyte influence on motor neuron maturation, Stem Cells Translational Medicine 2019. Spinal regional co-culture maturation, not general learning. https://doi.org/10.1002/sctm.19-0147

[R13](../references.md#r13) Brad Gearon, ChristEye Physical Learning Device Design v0.4 (provided). Current reference device; C, O, S, B, H, L, E and R0-R3 sequence. research/source_documents/R04_ChristEye_Physical_Learning_Device_Design_v0_4_3\_.docx

[R14](../references.md#r14) ChristEye, Research and System Design, revised synthesis (provided). Current device and Crystal research boundaries; experiments EX01-EX12. research/source_documents/R02_02_Research_and_System_Design_1\_.docx

All primary source identifiers and the original documents are also preserved in the repository research directory. Earlier source syntheses do not substitute for biological confirmation.

