---
title: "CE-A09 v3.2 — Animon Core, Soil Identity and Source-to-Function Architecture"
document_id: "CE-A09-DESIGN-V3.2-PILOT"
revision: "3.2-pilot"
date: "2026-11-16"
owner: "We Develop Software LLC"
status: "Technical research design / laboratory review pending"
---

# CE-A09 · Animon Core — Engineering Design v3.2 pilot

**Decision:** the target class is an **Animon**, not simply a computer. An individual Animon would be a developmentally traceable system with specialist functions, acquired capabilities and sustained care. A proposed living **Soil organoid** is one possible substrate *within* such an individual, not the whole system or a demonstrated specimen. See [ANIMON/1](../../../ontology/ANIMON_V1.md), [original A09](../../v3/design/CE-A09_main_living_core_Design.md) and [scientific companion](../companion/CE-A09_Animon_Core_Scientific_Companion.md).

**Scope:** verified interface requirements, independent physical/digital roles, explicit laboratory questions and signed handoff gates. No real Animon preparation, live neural training, new cell-production regimen or peptide intervention has been verified by the repository.

## 1. What is distinct from what?

| Entity | Own identity | Verified/claimed scope |
|---|---|---|
| Animon specification | animon_spec_id | Versioned design, development and care obligations |
| Digital Animon reference model | digital_model_id | Nonliving software, message bus and task evaluation |
| Candidate individual Animon | candidate_instance_id (unset) | A future actually registered hybrid/living preparation |
| Soil organoid whole | preparation_id | Proposed living substrate containing neural/glial structures |
| Spinal Root tissue | substrate/region identifier | Candidate spinal-derived circuit requiring its own evidence |
| Functional Root | root_id + task + frozen reader | Retained task relationship, not automatically an anatomical address |

**Acceptance of this design:** an external reviewer can trace each proposed tissue, state change, learning claim and source to a typed record or **UNRESOLVED**, without inventing a living instance.

## 2. Layered implementation map

~~~mermaid
flowchart TB
  Spec["ANIMON/1 blueprint"] --> Digital["Digital Animon reference (nonliving)"]
  Spec --> Candidate["Individual Animon candidate (not yet qualified)"]
  Digital --> Lotus["Lotus / WindBus / digital CrystalPacks"]
  Candidate --> Soil["Soil: whole prospective organoid"]
  Candidate --> Root["Spinal Root tissue: proposed region"]
  Candidate --> Care["Independent SustiBus care boundary"]
  Soil --> Cells["Neurons, glia, axons and dendrites"]
  Root --> Cells
  Cells --> Readout["Qualified observation, not instructions"]
  Care --> Readout
  Readout --> Chem["ChemiBus, imaging and physiology"]
  Chem --> Review["Human scientific review / no automatic actuation"]
  Lotus --> DigitalTest["Digital Root and sealed evaluation"]
~~~

Any connection between physical tissue and a learning evaluator is currently a **research-dependent interface**. A digital WindBus event is not an axon, and a correlated electrode is not necessarily an anatomical connection.

## 3. Physical participant and observation register

| Candidate component | What to confirm | Measurement limitation | Qualified owner |
|---|---|---|---|
| Source/lineage | actual lot and provenance, source permissions, phenotype | inherited lineage is not a unique cell ID | cell-production lead |
| Neurons | preparation-specific identity, soma, axon, dendrite, electrical contribution | a single marker does not establish mature function | developmental neuroscientist |
| Astrocytes/glia | source-specific marker panel, spatial glial distribution and support | GFAP or metabolic signal alone cannot prove learned content | glial specialist |
| Axon projection | actual mapped neurite course, candidate terminal and target | morphology does not prove directed synaptic influence | imaging/electrophysiology |
| Spinal Root candidate | independent regional identity and potential route contribution | not established as semantic long-term memory | circuit specialist |
| Sea/exchange environment | qualified care pathway, source/media lot and assay validity | sample level does not equal metabolic flux | care and chemistry lead |

Candidate regional markers and ordinary structures are reviewed in [BIO-01](../../../processes/BIO-01-crest-lineage.md), [BIO-02](../../../processes/BIO-02-axon-spinal-root.md), and [BIO-03](../../../processes/BIO-03-soil-organoid.md). These are **assay candidates**, not ingredients or a differentiation protocol.

## 4. Stage and rejection states

~~~text
DESIGN_REGISTERED
  -> SOURCE_REVIEW_PENDING
  -> SOURCE_QUALIFICATION_REVIEWED
  -> STRUCTURAL_ASSAY_QUALIFICATION
  -> PHYSIOLOGICAL_READOUT_QUALIFICATION
  -> LEARNING_STUDY_REVIEW
  -> EVIDENCE_REVIEW
  -> RELEASE_FOR_SPECIFIC_REVIEWED_CLAIM
any missing source / assay / permission / care -> HOLD_INTERPRETATION + HUMAN_REVIEW
~~~

A software planner **cannot** advance a living stage because of elapsed time, spontaneous network activity, high task reward, or an inferred molecular need. Independent laboratory personnel own physical process approvals.

## 5. Canonical records and joined evidence

| Purpose | Source-controlled contract | Mandatory distinction |
|---|---|---|
| Animon specification | [animon_spec.schema.json](../../../schemas/animon/animon_spec.schema.json) | spec and software model versus actual individual |
| Soil identity | [soil_region.schema.json](../../../schemas/biology/soil_region.schema.json) | whole preparation, regional phenotype and unobserved depth |
| Cell lineage | [cell_identity_observation.schema.json](../../../schemas/biology/cell_identity_observation.schema.json) | clone/individual cell observation, not entire system |
| Axon mapping | [axon_projection.schema.json](../../../schemas/biology/axon_projection.schema.json) | structural/statistical/influence/task evidence separately |
| Care | [sustibus_care_status.schema.json](../../../schemas/biology/sustibus_care_status.schema.json) | physical support independent of reward |
| Molecular samples | [chemibus_observation.schema.json](../../../schemas/biology/chemibus_observation.schema.json) | analyte units and sample, not chemical actuation |
| Learned Root | [functional_root_assessment.schema.json](../../../schemas/biology/functional_root_assessment.schema.json) | actual delayed task versus indexed digital text |

The [digital model fixture](../../../fixtures/v3.2/animon_digital_reference.example.json) and [unobserved Soil fixture](../../../fixtures/v3.2/soil_region_unobserved.example.json) are **schema-valid synthetic examples**, not physical results.

Evidence joins need **preparation + source/lot + region + assay/instrument + clock + observation coverage + raw asset + review status**. A relationship between a functional Root and candidate neural region is many-to-many; there is no equation of memory_id to neuron_id.

## 6. Experiment boundary and capability acceptance

The first plausible scientific question is narrow: **can a qualified preparation produce a reproducible, task-specific response beyond a matched nonlearning and independent readout baseline?** Before testing, require a real source, qualified care, actual delivered-event receipts, a frozen reader, controls for adaptation/leakage and a declared delayed probe. See [BIO-05](../../../processes/BIO-05-measurements.md), [BIO-06](../../../processes/BIO-06-teaching.md).

A positive output without source/sample validation is a **source-quality failure**. A new morphological process without measured recipient influence is **structural evidence only**. A delayed score without separated access/reader is **retention unresolved**. A digital model completing the task is **digital-only evidence**.

The 2026 cortical-organoid study [Robbins et al.](https://pubmed.ncbi.nlm.nih.gov/41720084/) demonstrated bounded learning under its experimental conditions but did not establish long-term retention after its reported rest period. That paper is **not** local Animon validation.

## 7. Fault and care separation

| Observable | Record outcome | Not authorized |
|---|---|---|
| Source not selected | HOLD_SOURCE | create fictitious lot ID and proceed |
| Imaging surface only | PARTIAL_COVERAGE | claim entire 3D interior characterized |
| Electrode covariation only | CORRELATION_ONLY | promote to physical synapse |
| Missing/late care receipt | HUMAN_REVIEW and suspend new learning | withhold independent essential care |
| Readout/decoder drift | NO_ROOT_PROMOTION | credit digital decoder as tissue understanding |
| Chemical sample uncalibrated | NOT_MEASURED/UNRESOLVED | change food, add peptide or infer a new Root |
| Digital task passed | DIGITAL_REFERENCE_VALID | issue a physical individual Animon identity |

**Change review:** physical care, new cell formation, glial source and peptide proposals are separately controlled by [BIO-09](../../../processes/BIO-09-growth-control.md) and actual institution-qualified SOPs. No intervention settings are generated here.

## 8. Research handoff

The laboratory receives a **StudyReadinessPacket** with actual source and consent review; selected preparation and independent biological replicate plan; intended measurement/coverage; viable independent care; clock and assay validation; primary endpoint; comparison/negative controls; rights; and PI review state. A missing critical item leaves the proposal at **LAB_REVIEW_PENDING**, not "lab executable."

**Release question:** *What did the chosen actual source system do, under what preparation, measured with which validated instrument and evaluator, and how is that different from ordinary maturation or external software computation?*

**Related:** [scientific companion](../companion/CE-A09_Animon_Core_Scientific_Companion.md) · [Animon care design](CE-A17_Animon_Care_Design.md) · [original CE-A16 Root mapping](../../v3/design/CE-A16_root_registry_and_mapping_Design.md) · [original CE-A02 retention](../../v3/design/CE-A02_long_term_storage_Design.md).

© 2026 We Develop Software LLC · [WDS license](../../../LICENSE.md). No confidential physical patent document published.


## V3.3 operating work package — CE-A09 Soil/Core (scientist execution)

**This section is the operating layer, not another architecture diagram.** Start at [WO-00 qualified source/genetic material](../../../laboratory/WO-00-source-and-genomic-qc.md), then [WO-01 Soil/model development](../../../laboratory/WO-01-soil-development-and-maturation.md), while [WO-02 care](../../../laboratory/WO-02-daily-care-and-chemistry.md) runs independently.

| Actual sequence | What to obtain/do at the proper authorized stage | Evidence that must be checked before progression |
|---|---|---|
| A09-01 | Select PI/institution, the approved R1 2D reference or a distinct P06 3D source model; register cell-bank lot, legal use and approved SOP | signed [source form](../../../laboratory/forms/SOURCE_RELEASE_FORM.md), applicable STR/SNP/genome stability/contamination/source phenotype evidence |
| A09-02 | Identify and record independent batch, living preparation, source and region; do not invent an individual Animon specimen | actual traceability ledger, protocol and source lineage, bank/passage |
| A09-03 | Under the institution SOP, document the actual initiated preparation, first source-qualified care and earliest valid observation | real physical service receipt; raw image or assay source ID; source-specific measurement range |
| A09-04 | Record stage-qualified morphology, viability, regional cell identity and 3D coverage **where applicable**, with true sample/method | current vs prior comparable stage/valid day, source variation and unobserved internal tissue |
| A09-05 | At an approved stage, document neuron/glia identities and actual axon/dendrite processes; separate imaging from functional inference | marker panel/reference, spatial transformation, instrument limitations, sampled biological unit |
| A09-06 | Allow a proposed circuit/learning study only if actual source/care/instrument/PI gates are valid | signed qualification result; otherwise HOLD_NEW_EXPERIMENT and maintain independent care |

**Phases and physical materials:** [P01 source-derived count/BOM](../../../laboratory/ANIMON_PHASE_AND_MATERIAL_BOM.md) documents the R1 **illustrative 80,000 motor neurons + 20,000 matched astrocytes** in one specific 2D supplier/MEA context. It does **not** set a 3D Soil cell count or approve a plating protocol.

**Daily action:** the operator uses the [daily shift form](../../../laboratory/forms/DAILY_SHIFT_SHEET.md) and compares each actual valid measurement to a prior comparable day and approved source/stage reference. A new morphology, different medium lot or assay does not by itself establish a new developmental capability. Trigger a [deviation/change card](../../../laboratory/forms/CHANGE_AND_DEVIATION.md) when source, care, assay or tissue status is uncertain. No automatic growth-factor/peptide/genetic change.

**Release output:** actual source-qualified preparation identity, cell/region characterization, approved care records, measured unknowns and PI/QA signatures. If no live material exists, release only a digitally tested *Animon specification*, not a physically created Animon.

