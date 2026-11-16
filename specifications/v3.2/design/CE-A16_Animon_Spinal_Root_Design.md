---
title: "CE-A16 v3.2 — Animon Spinal Root and Axon Evidence Registry"
document_id: "CE-A16-DESIGN-V3.2-PILOT"
revision: "3.2-pilot"
date: "2026-11-16"
owner: "We Develop Software LLC"
status: "Engineering and scientific evidence design; no observed spinal Root"
---

# CE-A16 · Animon Spinal Root and Axon Mapping — v3.2 Engineering Design

**Purpose:** give an Animon research team an evidence-preserving way to connect *candidate spinal-derived tissue* to *candidate axon projections*, possible functional influence and separately measured task retention **without collapsing them into one claim**.

This document supersedes the working interpretation in the [v3.1 Root Registry Design](../../v3/design/CE-A16_root_registry_and_mapping_Design.md) while preserving that edition. It complements [CE-A16 scientific companion](../companion/CE-A16_Animon_Spinal_Root_Scientific_Companion.md), [CE-A09 Animon Core](CE-A09_Animon_Core_Design.md), [CE-A17 care](CE-A17_Animon_Care_Design.md), and [ANIMON/1](../../../ontology/ANIMON_V1.md).

**Scientific status:** D1 engineering specification and D2 review questions only. The project has not supplied an observed spinal preparation, confirmed axonal joint or proven location of general memory.

## 1. Two Roots, four kinds of claims

**Physical spinal Root** is a proposed region or distinct preparation with qualified spinal-derived neuronal/glial identity. **Functional Root** is a retained relation between a defined task context, sensory/input history and independently assessed response. One structural Root can contribute to many task relations, and a tested functional relation may depend on more than one region, electronic route or reader.

| Edge class | What it means | Ordinary evidence basis | What it does *not* mean |
|---|---|---|---|
| STRUCTURAL_CONTACT | Candidate anatomical projection, adjacency or terminal relationship | source-associated imaging, qualified regional identity, registration and coverage | Functioning or learned synaptic signal |
| STATISTICAL_ASSOCIATION | Measured relationship between two time/space signals | calibrated source/recipient streams with latency uncertainty and common-input checks | Directed influence or axon connection |
| INTERVENTION_SUPPORTED_INFLUENCE | Stronger research inference of directional effect in a specifically reviewed model | separate qualified laboratory study with independent controls and reviewed source data | Physical memory location or general computation |
| TASK_RELATION | A measured functional relation between stimuli/context and response, possibly delayed | independent task assessment and frozen reader, with meaningful controls | Unique somatic, spinal or molecular engram assignment |

All edges have *claim state* independent of the class: DESIGN, PROPOSED, UNRESOLVED, OBSERVED, ASSOCIATIONAL, CAUSAL_RESEARCH, or REJECTED. A design-stage edge whose intended class is STRUCTURAL_CONTACT is **not evidence of any actual contact**.

## 2. Boundary architecture and owners

~~~mermaid
flowchart TD
  Spec["Animon specification (no living specimen assumed)"] --> Source["Qualified source lot / Crest review"]
  Source --> Soil["Soil organoid region and source"]
  Source --> Spinal["Candidate spinal Root region"]
  Soil --> Img["Imaging/phenotype observation registry"]
  Spinal --> Img
  Soil --> Electrode["Qualified recording: time, artifacts, channel identity"]
  Spinal --> Electrode
  Img --> Graph["CE-A16 typed evidence graph"]
  Electrode --> Graph
  Task["CE-A02 task/Root assessment: frozen reader"] --> Graph
  Care["SustiBus independent health/support status"] --> Graph
  Graph --> Gate{"Scientific evidence gate"}
  Gate -->|Insufficient or contradictory| Hold["UNRESOLVED / keep source data"]
  Gate -->|Reviewed scoped evidence| Claim["Source-limited evidence certificate"]
~~~

**External care and biological inputs remain outside the graph writer's control.** CE-A16 is a **read-only scientific evidence registry** with software validation. It does not generate axons, change cell lineage, stimulate tissue or modify any medium.

## 3. Typed records and their joins

| Record | Current canonical contract | Join rule |
|---|---|---|
| Soil Region | [soil_region.schema.json](../../../schemas/biology/soil_region.schema.json) | actual preparation, source/lot, anatomical region, observation coverage |
| Cell Identity | [cell_identity_observation.schema.json](../../../schemas/biology/cell_identity_observation.schema.json) | sampled population or tracked cell, clone lineage and observation confidence |
| Axon Projection | [axon_projection.schema.json](../../../schemas/biology/axon_projection.schema.json) | candidate source/target, spatial registration and functional-test references |
| Spinal Root Substrate | [spinal_root_substrate.schema.json](../../../schemas/biology/spinal_root_substrate.schema.json) | proposed anatomical tissue identity and qualification stage |
| Root Evidence Edge | [root_evidence_edge.schema.json](../../../schemas/animon/root_evidence_edge.schema.json) | graph-level typed claim linking source and target refs |
| Functional Root Assessment | [functional_root_assessment.schema.json](../../../schemas/biology/functional_root_assessment.schema.json) | learned task, reader digest, delayed probes and protected tasks |
| Study Readiness Packet | [study_readiness_packet.schema.json](../../../schemas/animon/study_readiness_packet.schema.json) | lab source, assay, PI/care review slots and non-executable status |

**Identifier non-equivalence:** animon_spec_id ≠ preparation_id ≠ source_lot_id ≠ cell_track_id ≠ axon_projection_id ≠ functional_root_id. An electrode channel may summarize many cells; a source barcode may be shared by descendants. All relationship identifiers must be versioned and individually support invalidation without rewriting original measurements.

## 4. Reference inputs — synthetic and schema-valid

- [AxonProjection: no observed axon](../../../fixtures/v3.2/axon_projection_unobserved.example.json).
- [Root evidence edge: unobserved candidate contact](../../../fixtures/v3.2/spinal_root_edge_unobserved.example.json).
- [Functional Root assessment: no learned relation](../../../fixtures/v3.2/functional_root_unobserved.example.json).
- [StudyReadinessPacket: unapproved](../../../fixtures/v3.2/study_readiness_unapproved.example.json).

These records use declared *candidate types* solely to exercise a **software schema**. Every fixture states DESIGN, no actual evidence, missing physical measurement and NO promotion. Placeholder dates are not laboratory timestamps.

## 5. Source and axon measurement acceptance

A candidate structural trace requires **actual** source/target spatial definitions, qualified source tissue, image position/depth, registration transformation, uncertainty and valid observation history. Neuronal axons are *parts of neurons*, not independent replicating cells or objects that can be cloned apart from neuron identity.

For a candidate functional influence, the evidence must also distinguish direct/indirect biological pathways from **electrode crosstalk, shared electrical fields, common input, shared fluid environment, adaptive digital decoding and source-independent spontaneous events**. A correlated spike train is insufficient.

A positive spinal identity panel and an axon trajectory do not automatically prove a task-specific contribution. Candidate markers such as MNX1/ISL1/CHAT for selected spinal phenotypes, neurofilament/TAU/GAP43 for neuronal processes, or DLG4/SYN1 for selected synaptic organization are **assay annotations** only. This is not a list of growth reagents or a spinal differentiation recipe.

## 6. Graph acceptance as a software implementation

A claim registry can be implemented as a source-linked append-only record of **candidate → evidence → reviewer decision**, where changed interpretations create a new revision rather than modifying instrument raw files.

~~~text
SUBMITTED_CANDIDATE
  -> SCHEMA_VALIDATED
  -> SOURCE_AND_CLOCK_CHECK
  -> EVIDENCE_CLASS_SEPARATION
  -> INDEPENDENT_REVIEW
  -> ACCEPT_SCOPED | HOLD_UNRESOLVED | REJECT_UNSUPPORTED
~~~

**Suggested pure-function gate (illustrative):**

~~~python
def can_promote(root_edge):
    if root_edge["evidence_status"] in {"DESIGN","PROPOSED","UNRESOLVED","REJECTED"}:
        return False
    if root_edge["claim_promoted"] is False and not root_edge["reviewer_ref"]:
        return False
    required = ["observation_ref","source_asset_digest","instrument_calibration_ref"]
    if not all(root_edge[field] for field in required):
        return False
    if root_edge["edge_class"] == "INTERVENTION_SUPPORTED_INFLUENCE":
        return bool(root_edge["common_drive_control_ref"] and root_edge["reviewer_ref"])
    if root_edge["edge_class"] == "TASK_RELATION":
        return bool(root_edge["task_assessment_ref"] and root_edge["reader_version_ref"])
    return bool(root_edge["reviewer_ref"])
~~~

This code is *illustrative policy logic*, not a validator for experimental truth and not a substitute for a human PI. A source image can exist yet still be misleading; documentation must preserve the possibility of REJECTED after technical validation.

## 7. Scientific controls and failure table

| Scenario | Expected technical status | Alternative explanation |
|---|---|---|
| Spatial contact appears but no functional route is qualified | STRUCTURAL_CANDIDATE only | adjacent fibers, non-synaptic juxtaposition, artifact |
| Correlated recordings without adequate common-input checks | ASSOCIATIONAL or UNRESOLVED | shared drive/electrical interference |
| External host decoder changes while tissue source is stable | HOLD_ROOT_PROMOTION | digital adaptation masquerading as neural development |
| Spinal source identity not verified | SOURCE_UNQUALIFIED | different region/lineage rather than intended circuit |
| Root task improves in the linked system but not recipient alone | ASSISTED_USE_ONLY | external information/teacher channel |
| Old learned tasks decline during new study | PROTECTED_TASK_REVIEW | interference, source health or changed access |
| Care, source or sensor receipt invalid | HUMAN_REVIEW and suspend novel task claims | observed change due to physiological state |
| Synthetic example graph is queried | DESIGN_ONLY | there is no specimen, axon or learned Root |

Use meaningful negative controls, site-specific validated instrumentation and independent biological experimental units chosen by a qualified laboratory. The digital graph service may test **data behavior** with fixtures even when no living tissue exists.

## 8. Sampling, uncertainty and source identity

Every candidate anatomical/electrical measurement has independent **sample position, clock domain, depth and tissue coverage, signal quality, calibration and raw asset hash**. A sample ROI is not a globally persistent single-neuron ID unless validated; a tracked axonal branch is not a separate cell birth. The cell's role and the task relationship are time-scoped, and missing observation regions remain explicitly unknown.

A laboratory may choose suitable imaging and electrophysiology families, but the exact acquisition/culture method is its own reviewed SOP. Neither physical parameter nor optimization algorithm is given here.

## 9. Change and growth proposal card

A requested new spinal Root connection could mean different experiments: new source cells, axon extension, synaptic remodeling, a changed electronic bridge, or simply corrected graph mapping. Each must first be named **as a distinct hypothesis**, with source, observation, protected prior function, comparator and independent review. No "grow now", peptide or stimulus command exists in CE-A16.

Refer to [BIO-09 growth governance](../../../processes/BIO-09-growth-control.md), [BIO-02 spinal Root/axon science](../../../processes/BIO-02-axon-spinal-root.md) and [BIO-07 joining](../../../processes/BIO-07-joining.md).

## 10. Engineering and laboratory review gates

**D1 software/design acceptance:** schema-valid typed links, source references, honest missingness, reject invalid evidence promotion, reproducible append-only graph history and stable CE-A02 interoperability.

**D2 scientific handoff candidate:** actual qualified source and region, validated assay/space/time, declared functional readout, controls, independent care, preparation-level replicates, PI and ethics review slots. If missing, record UNRESOLVED and a responsible review owner.

**D3/D4:** actual instrument/qualified biological study requires separate institutional SOP and measured data; it has **not** been achieved by these documents.

**Next integration:** [CE-A16 scientific companion](../companion/CE-A16_Animon_Spinal_Root_Scientific_Companion.md) and [CE-A02 retention design](CE-A02_Animon_Long_Term_Retention_Design.md).

**Rights:** © 2026 We Develop Software LLC · [WDS Public Research Reference License](../../../LICENSE.md). Confidential physical patent document excluded.


## V3.3 operating work package — spinal Root and axon evidence

**Precondition:** use [WO-03](../../../laboratory/WO-03-axon-and-spinal-root-evidence.md) only after source and preparation qualification under [WO-00](../../../laboratory/WO-00-source-and-genomic-qc.md), [WO-01](../../../laboratory/WO-01-soil-development-and-maturation.md) and independent [WO-02 care](../../../laboratory/WO-02-daily-care-and-chemistry.md).

| Step | What the actual investigator does | Required proof/decision |
|---|---|---|
| A16-01 | Register source/target region, neuronal phenotype and actual preparation/lot | real specimen, source assay and 3D sampling limits |
| A16-02 | Select qualified optical/electrophysiology method, timebase and coordinate transformation | instrument calibration, region coverage, source/target identity |
| A16-03 | Document soma, dendrite and axon candidates **as processes of neurons** with separate cell/track/projection identifiers | original images, tracing method and spatial uncertainty |
| A16-04 | Classify anatomical contact separately from coactivity/correlated channels | STRUCTURAL vs ASSOCIATIONAL status and matched spatial/control evidence |
| A16-05 | If claiming actual influence, require a separately approved laboratory experiment and negative routes | valid source/recipient evidence and common-drive exclusion |
| A16-06 | If claiming a learned Root, require **CE-A02** fixed-reader task and delayed independent performance | valid task-specific evidence; never infer semantic storage from axonal density |
| A16-07 | Compare repeat observations with prior qualified region, method and care history | flag geometry/medium/instrument changes as NONCOMPARABLE, or record scoped observed differences |
| A16-08 | Sign evidence classification and failure state | source-specific reviewed graph edge or UNRESOLVED/REJECTED |

**If an expected axon track is missing today:** check optical field/depth, source/region registration, segmentation, qualified calibration, true cell source and changes to care before concluding a physical process disappeared. A software tool cannot respond by prescribing axon growth, adding growth factors or modifying cells.

**Delivered package:** real source and sample ledger, image/electrical raw hashes, cell/axon identity, controls, independent care, reviewer approval and typed [RootEvidenceEdge](../../../schemas/animon/root_evidence_edge.schema.json). All physical observation/manipulation methods must come from local approved SOPs.

