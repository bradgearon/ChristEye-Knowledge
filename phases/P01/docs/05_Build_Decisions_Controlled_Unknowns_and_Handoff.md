---
title: "Build Decisions Controlled Unknowns And Handoff"
document_id: "CE-AN-P01-05"
program: "ChristEye Anaiero"
phase: "P01"
revision: "4.0 / 2026-11-15"
status: "Engineering research specification — not a validated biological protocol"
owner: "We Develop Software LLC"
---

# Build Decisions Controlled Unknowns And Handoff

> **ChristEye Anaiero · Phase P01 · 15 November 2026 · WDS research property**  
> [Phase index](README.md) · [Bibliography](../references.md) · [Registers](../registers/README.md)

*Concrete procurement decisions and acceptance checklists for initiating the next phase.*

Status: scoped engineering design and supplier-referenced research. Values tied to one platform are not transferable to another without compatibility qualification. Current tissue work: NOT EXECUTED. \[R01, R13-R16\]

# Equipment choices, not mixed assumptions

| **Boundary**                    | **Candidate**                                                                                            | **Why**                                                                                       | **Hold until**                                                                                           |
|---------------------------------|----------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------|
| Phase-1 reference activity      | Axion Maestro Edge and 24-well CytoView MEA with compatible BrainXell spinal motor/astrocyte preparation | Supplier provides grounded numeric seeding example and reference service schedule \[R01,R02\] | Culture lead selects exact product/lot and shows compatibility.                                          |
| High-density mechanistic branch | MaxOne+ 26,400-electrode platform with optical access and 1,020 readouts [R03](../references.md#r03)                         | Rich spatial sampling, stimulation and raw data for detailed Mesh science                     | Vendor confirms simultaneous recording/stimulation, support/optics and a new live seeding specification. |
| Sensing/optics                  | Microscope + registered fiducials + appropriate reporter system                                          | Observe geometry, selected glial activity and drift                                           | Phototoxicity/medium/reporters characterized for each preparation.                                       |
| Digital host                    | CPU/GPU, data recorder, timebase, source registry, sealed E, independent watchdog L                      | Buildable before live tissue; supports capture/Crystal replay                                 | Digital tests pass; no live profiles until L permit qualified.                                           |
| Fluid service                   | Separately identified feed / sample / waste path for each biological module                              | Separates cause from common chemical exposure                                                 | Cell-free test verifies materials, dead volume, sampling and containment.                                |
| Qualified cell source           | Consented provenance verified human-derived selected neurons and astrocytes                              | Reproducibility and legal material provenance                                                 | Lot COA, cell marker panel, SOP and laboratory approval.                                                 |

# The controlled parameter register

| **ID** | **Missing decision**                             | **Blocked gate**     | **Owner**              | **Close with / evidence**                                                   |
|--------|--------------------------------------------------|----------------------|------------------------|-----------------------------------------------------------------------------|
| U01    | Reference hardware mode                          | R1                   | Culture+hardware lead  | Select Axion vendor reference vs MaxOne research mode                       |
| U02    | Exact neuron/astrocyte lot + subtype purity      | G1                   | Culture lead           | Supplier release and pair compatibility                                     |
| U03    | 3D organoid initial cell count and geometry      | P06                  | Organoid lead          | Preparation-specific data with size/diffusion/coverage validation           |
| U04    | Complete medium formulation and change schedule  | G2                   | Culture lead           | One supplier or laboratory SOP with compatible composition                  |
| U05    | Coating / matrix / support volumes               | G2                   | Culture lead           | Chosen source+platform protocol and blank compatibility                     |
| U06    | Electrical stimulation profile library           | G3                   | Electrophysiology lead | Vendor limits + cell-free interlock + live response qualification           |
| U07    | Input/output electrode map and decoder           | G3                   | Systems lead           | Stimulus-to-response calibration and locked digest                          |
| U08    | Noise threshold, activity drift tolerance        | G3                   | Electrophysiology lead | Measured pilot distribution, reference/sham characterization                |
| U09    | Closed-loop delay/exposure budget                | G4                   | Systems lead           | End-to-end measurements and eligibility hypothesis tests                    |
| U10    | Learning pilot replicates and confirmed controls | G4                   | Stats lead             | Preplanned unit, variance, target and attrition                             |
| U11    | Retention test horizons and readout              | G5                   | Science lead           | Preregistered 45m and later qualified test windows                          |
| U12    | Bridge bandwidth, codebook, disconnect           | G6                   | Systems lead           | Measured A/B timing, route disable and cache clear                          |
| U13    | Astrocyte reporter specificity                   | A1/A3 research       | Imaging lead           | Validated reporter, kinetics and effects                                    |
| U14    | Physical Crystal carrier and reader              | G7                   | Mechanism lead         | Material entity, task relation, reader contract, content-specific causality |
| U15    | Molecular-peptide input                          | Research only        | Molecular biology lead | Molecular target/receptor, selectivity, off-target and review               |
| U16    | Surveillance / microglia-like cells              | P06+                 | Culture lead           | Defined immune lineage, model question, validated assay and consequence     |
| U17    | Human organoid ethical review                    | Before live organoid | Institution            | Consent, oversight, complexity & end-point review                           |

# Build sequence: first phase to next phase

1.  Create the canonical module and preparation registry (R0 digital, one C reference physical candidate, O/B/S not yet enrolled).

2.  Choose one vendor-supported reference geometry and source pair; obtain source documents, COAs and independent source/consent approvals.

3.  Freeze an actual cell census with sourced platform-specific values, status and pilot control layout; maintain separate 3D future record with null counts.

4.  Build the cell-free H/E/L harness and test schema, clock and fault state with zero biological material (starts during P01).

5.  Implement ModuleRecord, MoleculeRecord, CultureService, WindEvent, TrialRecord, RootRecord, ClaimRecord and CrystalCandidate; validate source/recipient permissions.

6.  Obtain hardware/fluids/optics supplier answers and update status of U01-U17; close only with evidence rather than naming a plausible value.

7.  Approve a qualified laboratory plan, physical safety limits, viability/identity and maturation criteria.

8.  Proceed to P02 cell-free commissioning. Only after it passes should P03 receive a qualified living reference.

# An example decision record

> decision_id: P01-DEC-001  
> question: Reference initial viable count per biological sample  
> chosen_source: R01 (BrainXell/Axion 24-well co-culture)  
> example_neurons_per_well: 80000  
> example_astrocytes_per_well: 20000  
> status: SUPPLIER_EXAMPLE_NOT_LIVE_RELEASE  
> applies_to: Axion/BrainXell planar reference only  
> not_applicable_to: MaxOne; 3D organoid; human transplant  
> required_to_promote: chosen line + source lot + vendor/lab compatibility + G1/G2 release

# Evidence classifications at shipment

| **Artifact**                                 | **Evidence class**                         | **Status**                                                          |
|----------------------------------------------|--------------------------------------------|---------------------------------------------------------------------|
| Supplier count 80k+20k                       | F — supplier example for named preparation | Documented, not validated as Anaiero preparation.                   |
| C/O/S/B functional division                  | D — architecture decision                  | Software simulated; physical role training not yet established.     |
| Context and order task                       | D — defined falsifiable task               | Digital exact-rule oracle exists; biological learning hypothetical. |
| Astrocyte role in slow retention             | X/H — cross-model candidate                | Mouse evidence; no organoid mechanism established.                  |
| Digital Crystal clone/replay                 | D — implemented prototype                  | Digital testing only; no biological state transferred.              |
| Physical Crystal and peptide-coded knowledge | H — speculative                            | Not a device capability, no molecular memory encoding.              |
| Care and operator interlocks                 | D — required safety architecture           | Live implementation requires independent verification.              |

# Selected source register

[R01](../references.md#r01) BrainXell/Axion, Neuron/Astrocyte 24-well MEA Co-culture Protocol v10.0. Published platform-specific 50k-100k neurons/well; 2:1-6:1 neuron/astrocyte ratios; 80k+20k worked example; supported medium/service schedule. https://www.axionbiosystems.com/sites/default/files/resources/BrainXell%2024-well%20MEA%20Co-culture%20%20Neuron_Astrocyte%20Protocol%20v10.pdf

[R02](../references.md#r02) Axion Biosystems, BrainXell 24-well MEA co-culture protocol landing page. Vendor protocol source and specified compatible MEA architecture. https://www.axionbiosystems.com/resources/culture-protocol/neuronastrocyte-24-well-mea-co-culture-protocol-v100

[R03](../references.md#r03) MaxWell Biosystems, MaxOne single-well HD-MEA specifications. 26,400 electrode sites, 1,020 simultaneous readouts, 20 kHz/channel, 19-mm inner ring, 3.85x2.10-mm active area, API and raw data. https://www.mxwbio.com/products/maxone

[R04](../references.md#r04) ISSCR, Standards for Human Stem Cell Use in Research, Stem Cell-based Model Systems. Require documented source, density, microenvironment, reproducibility, quality metrics and variability across preparations. https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems

[R05](../references.md#r05) ISSCR, Guidelines for Stem Cell Research and Clinical Translation, August 2025 v1.2. Ethics, provenance, oversight and reporting. 2025 revision focuses on embryo models. https://www.isscr.org/resources/guidelines-for-stem-cell-research

[R13](../references.md#r13) Brad Gearon, ChristEye Physical Learning Device Design v0.4 (provided). Current reference device; C, O, S, B, H, L, E and R0-R3 sequence. research/source_documents/R04_ChristEye_Physical_Learning_Device_Design_v0_4_3\_.docx

[R14](../references.md#r14) ChristEye, Research and System Design, revised synthesis (provided). Current device and Crystal research boundaries; experiments EX01-EX12. research/source_documents/R02_02_Research_and_System_Design_1\_.docx

[R15](../references.md#r15) ChristEye, Formal Models and Reconciliation (provided). Corrected equations, accounting, identifiability and unsupported analogies. research/source_documents/R03_03_Formal_Models_and_Reconciliation_1\_.docx

[R16](../references.md#r16) ChristEye, Technical Companion (provided). Biochemical and structural candidate roles and evidence labels. research/source_documents/R01_01_Technical_Companion_1\_.docx

All primary source identifiers and the original documents are also preserved in the repository research directory. Earlier source syntheses do not substitute for biological confirmation.

