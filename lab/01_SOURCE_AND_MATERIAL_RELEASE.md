---
title: "LAB-R1-01 — Biological source receipt, identity and material release"
document_id: "LAB-R1-01"
revision: "0.1"
date: "2026-11-16"
status: "Execution packet; live work requires signed source-specific lab SOP"
owner: "We Develop Software LLC"
---

# LAB-R1-01 — Biological and genetic source qualification

This is the first **concrete physical reference campaign**: a source-specific **2D, 24-well Axion Maestro Edge MEA** using compatible BrainXell spinal motor neurons and astrocytes. It is not the final Animon's 3D Soil or independently formed spinal Root system. [P01 source selection](../phases/P01/docs/05_Build_Decisions_Controlled_Unknowns_and_Handoff.md) · [supplier protocol](https://www.axionbiosystems.com/resources/culture-protocol/neuronastrocyte-24-well-mea-co-culture-protocol-v100) · [BrainXell protocol updates](https://brainxell.com/protocols/).

## G0 — before buying or handling physical material

**Required technician/PI actions** (a checkbox is valid only when the referenced evidence has been attached):

1. Register a campaign ID, responsible PI, culture lead, care operator, electrophysiologist, statistician, biosafety/source reviewer and equipment owner; require an actual facility and trained operators.
2. Select exact vendor **neuron and astrocyte** SKUs and the selected lab-approved revision of the *full* culture SOP. P01 cites the Axion/BrainXell v10.0 reference; BrainXell currently lists a co-culture v10.1. Obtain the current manufacturer method; document which revision was approved and why.
3. Obtain material-transfer restrictions, research-use consent/permission provenance, COA, cryovial lot, source lineage, frozen or thawed bank history and documented qualified storage conditions.
4. Open a controlled well map, environmental instrument and plate service register. Freeze assay/analysis IDs, channel map and versioned raw-data archive layout.
5. Identify source-qualified health and contamination testing SOPs, emergency response and an **independent** physical care service capable of functioning without Lotus or the learning evaluator.
6. Verify the vendor source, selected surface and electrode platform are compatible in the actual facility. If any prerequisite is missing: **HOLD_G0**; only the responsible scientist may release it.

## What material and identity evidence to obtain

| Physical/genetic source | Exact P01/vendor candidate | What the scientist collects as evidence | Failed/missing consequence |
|---|---|---|---|
| Cryopreserved motor-neuron population | BrainXell spinal motor neurons, product family **BX-0100-XX** | selected actual subtype/SKU, COA, viable quantity estimate, original cell source/lot and restrictions | quarantine/review wrong or missing source identity |
| Cryopreserved astrocytes | BrainXell astrocytes, **BX-06XX-XX** | selected actual line, COA, lot, compatibility with the neuron source, custody | no silent substitution |
| Genomic identity record | authorized vendor/bank DNA identity record or approved reference aliquot | donor/line ID coded without personal identifiers, STR or equivalent authentication report, method and comparison identity | HOLD when cell source cannot be authenticated |
| Genomic stability (where applicable) | qualified pluripotent-bank / derived-line material | passage history and source-specific genomic/structural stability report, or approved lab analysis plan | any unexpected change requires source-quality review |
| RNA identity specimen | approved independent, preferably sacrificial validation aliquot | selected transcript panel, experimental unit, actual collection/storage/analysis SOP and reference controls | do not remove material from study wells without approval |
| Protein/phenotype specimen | approved parallel validation well/sample | neuronal/spinal/glial marker assay, source and region, spatial limits and reproducibility controls | one marker does not qualify Root learning |
| Hygiene and contamination | source- and institution-qualified assays | mycoplasma/sterility testing identifier, result, method, expiry and deviation process | suspected contamination triggers lab quarantine SOP |
| Cell lineage tracking | derived external observation IDs | cell_instance_id, source_lot_id, cell_track_id and confidence | do not confuse assigned digital IDs with a genetically encoded per-daughter ID |

**Genetic material decision:** For the first 2D reference, procure **authenticated research-use cell populations**. Do not start by collecting donor biological tissue, manufacturing a new cell line, inserting DNA barcodes or cloning an already learned network. If the lab needs a DNA identity check, authorize an **identity aliquot and a validated external/institutional STR/SNP assay**. This is an authentication record, not genetic engineering. The [ISSCR basic characterization guidance](https://www.isscr.org/basic-research-standards/basic-characterization) supports verified line identity, and [genomic stability guidance](https://www.isscr.org/basic-research-standards/genomic-characterization) explains when additional genomic review is relevant.

## Material and instrument release list

- **Hardware:** Axion Maestro Edge; compatible PDL-coated CytoView 24-well MEA and optical observation equipment. Record model, serial, maintenance/firmware, calibration status and plate lot.
- **Vendor-specified neural support:** compatible BrainFast Motor, Astro, D4 and SK supplements; selected source-approved DMEM/F12, Neurobasal, B27, N2 and GlutaMAX; selected Geltrex/Cultrex and documented trophic-factor formulation where specified by the vendor (BDNF, GDNF, TGF-β1). **These are a bill-of-materials qualification list, not a mix-and-dose recipe.**
- **Qualified instruments:** cell-count/viability method, source/identity/contamination assays, incubator environmental monitor, MEA capture and media/sampling method. Log which can be measured daily and which are **NOT_MEASURED**.
- **Traceability:** each physical service needs material identity, actual supplier/lot, operator, SOP version, time, requested vs verified event and any deviation.

## P01 reference population and plate plan

P01's reference is **80,000 viable neurons + 20,000 astrocytes per well** at a 4:1 ratio, for one Axion/BrainXell planar setting. It derives from the vendor's broader published neuron-seeding and neuron:astrocyte range. This **is not an approved seeding parameter** until the current complete supplier/lab SOP, actual COAs and experiment goal are selected. No 2D count carries to MaxOne or 3D Soil.

P01 suggests a 24-well **four-condition × six-well** exploratory comparison. A PI could review contingent teaching, exposure-matched/yoked, untrained/time-matched and sham/reader controls. These conditions are **proposal labels** until the source/physical exposure and randomization/analysis plans are approved. Six wells on one plate are *not* six independent source batches.

## G1 release artifacts — attach actual evidence

- [ ] Signed institution, scientific owner, source/legal review and operator roles.
- [ ] Full chosen current BrainXell/Axion SOP plus compatible source lot and selected plate geometry.
- [ ] COA and source custody for neuron and astrocyte vials; appropriate identity and contamination qualification.
- [ ] Cell-count/viability assay method and acceptance range, approved by the lab.
- [ ] Optional DNA, RNA and protein sample plan with independent source and named analysis method.
- [ ] Plate/well layout with control design, independent preparation hierarchy and blinded evaluator intent.
- [ ] Selected environmental ranges and care SOP revision; physical maintenance independent of task reward.
- [ ] Signatures, dates, exact unresolved parameter IDs and HOLD decisions.

**If any required authorization or actual source is absent, G1 remains HOLD.** No invented lot, specimen, gene profile, medium recipe or measured baseline is acceptable. Next: [daily operations](02_DAILY_OPERATION_AND_QC.md) and [neural assay / evaluation](03_MEA_AND_EVALUATION.md).

© 2026 We Develop Software LLC · WDS Public Research Reference License. Confidential physical patent excluded.
