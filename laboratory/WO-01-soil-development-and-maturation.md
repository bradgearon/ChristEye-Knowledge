---
title: "WO-01 — Soil preparation, longitudinal phenotyping and developmental release"
date: "2026-11-16"
status: "Observational work order; physical cultivation and differentiation require institutional SOP"
owner: "Qualified organoid scientist, cell-foundry lead and imaging reviewer"
---

# WO-01 — Define the Soil and document its actual growth

**Purpose:** specify the scientist's actual operating and observation sequence for an Animon's prospective **Soil organoid**, and prevent passage through developmental gates on the basis of time or a single marker alone. A source-specific physical formation/differentiation procedure must be selected and approved **separately** in WO-00.

[Lab execution standard](ANIMON_LAB_OPERATIONS_v3.3.md) · [CE-A09 Core](../specifications/v3.2/design/CE-A09_Animon_Core_Design.md) · [BIO-03](../processes/BIO-03-soil-organoid.md).

## 0. The actual construction sequence starts with the P01 planar reference

The original [Physical Module Composition and Counts](../phases/P01/docs/01_Physical_Module_Composition_and_Counts.md) sets **R0 = zero cells, R1 = a candidate 2D planar C learner**, then optional O/B/S comparison wells, and **P06 = prospective 3D organoid Soil** with no assigned cell count. R1 references a specific supplier/platform-compatible **80,000 motor neurons and 20,000 matched astrocytes per well** as illustrative quantities—not a generalized instruction to plate. See [phase-specific materials and counts](ANIMON_PHASE_AND_MATERIAL_BOM.md).

**Execution decision:** a PI must choose which *approved source model* the actual work order describes. For R1, characterize the qualified planar preparation and MEA interface; **do not label its shape or acceptance as 3D Soil**. For P06, separately qualify organoid source, dimensionality, viability, distribution, imaging depth and independent care; **do not reuse** the planar 80k/20k counts, medium formulation or growth history by default.

## 1. Work before the first physical batch

| Sequence | Exact owner task | Mandatory record and decision |
|---|---|---|
| 01-01 Source gate | confirm WO-00 source release, line/passage, permission, identity and quality review | SOURCE_ACCEPTED or HOLD_SOURCE; no culture from an unqualified identity |
| 01-02 Select source SOP | PI assigns one real, validated method for the *chosen* human neural/Soil model, including its stages and manufacturer/lab equipment | source-protocol ID/revision, owner, source/model and its authorized QC limits |
| 01-03 Define batch hierarchy | assign study, independent source/line, production batch, preparation, organoid and specimen/assay IDs | immutable hierarchy; one prep cannot masquerade as many independent biological units |
| 01-04 Define observation coverage | define source-approved imaging/instrument modalities, intended anatomical regions, depth, timebase and artifact controls | instrument map; what is **not** observable in the 3D interior |
| 01-05 Define qualification points | predeclare stages for morphology, source identity, genomic/cell-state assays, viability, glia, neurites, synaptic candidates and I/O readiness | stage gates and chosen source-qualified comparators; **no guessed number of calendar days** |

**Methods NOT defined here:** tissue culture recipe, cell seeding/differentiation, small molecules, growth factors, added peptides, patterning agent sequence or incubation details. All require the selected facility's verified SOP before physical execution.

## 2. At actual batch initiation

1. Confirm actual source release and operator authorization. Log the current source lot/passage, preparation/batch IDs, exact protocol revision, equipment ID and first authenticated service receipt. Distinguish a *planned* operation from an actual signed physical action.
2. Record the real **initial reference state** under the source SOP: preparation and region boundaries, instrument field, qualitative morphology, any valid measurement of size/shape and authorized viability/identity observations.
3. Record whether any tissue was lost, merged, excluded or reidentified. An individual organoid specimen is not a unique neuron or a whole Animon by definition.
4. Set a missing-data state for unobserved depth/region or unmeasured phenotype: UNKNOWN is preferable to inferring all internal cells from the visible surface.
5. Freeze the first source/model/instrument baseline record for later comparison, retaining raw image/assay and provenance. Baseline is not necessarily a healthy or mature state until a qualified scientist reviews it.

## 3. Daily morphological/physiological observation

Do the following on every designated observation day **with the facility's approved methods**, without disrupting essential care:

| Check | Actual data to obtain | Previous-day comparison | Escalation |
|---|---|---|---|
| Source & specimen continuity | line/lot, prep, organoid identity, stage and any approved service since last observation | require same specimen/valid source; flag remapping or reidentification | missing identity → HOLD_NEW_STUDY |
| Qualified environment/care | actual SOP care receipt and qualified health/environment measure | compare current to previous *comparable* day and certified site limits | care deviation → independently escalate, no software-actuated feed |
| 3D shape / outline | source-specific morphology, size/area/volume **where validated**, source image hash and method | display absolute and percent change; consider expected stage trend and measurement error | unexpected change → REVIEW; not automatically induce growth |
| Visible cell/region pattern | image region, cellular layer/zone candidates, sampling depth and uncertainty | track region coverage and whether repeated observations are same area | changed geometry/missing depth → NONCOMPARABLE |
| Viability/death indicators | method-qualified noninvasive or separately authorized assay result and origin | current versus baseline/previous valid matching method and controls | source-SOP outlier or concerning evidence → care/PI review |
| Neural/glial state | only if actual stage-gated sampled identity/functional assay scheduled | compare to matched stage/batch/source, not numerical prior-day rate alone | panel invalid → NO_CELL_IDENTITY_CLAIM |
| Electrical baseline | only if approved and properly acquired; full channel/time/artifact record | contrast valid reference/control, not assume bursts mean intelligence | invalid electronics → INVALID_MEASUREMENT |

**Important:** repeated tissue handling and illumination may itself influence the preparation; the source SOP must determine method/cadence. A daily record need not entail a destructive DNA/RNA/protein assay on the same organoid.

## 4. Stage-specific sampled genomic and molecular profile

**At the actual predeclared source-approved milestones**, the qualified scientist may review:

| Scientific question | Candidate evidence stream | Scope and failure rule |
|---|---|---|
| Are expected sources/cell states present? | lineage/regional RNA marker panels or validated scRNA/snRNA profiling and spatial protein identity | multiple markers/reference atlas, source and batch; reject identity claim on weak or mixed origin |
| Do candidate neural cells have observed processes? | neuron/axon/dendrite morphology, compartment-specific protein staining, spatial registration | a neurite is not a newly divided neuron |
| Are glia represented as intended by this model? | appropriate astrocytic/glial cell-state panel and spatial observations | marker or RNA abundance alone does not measure care or learning |
| Are spindle/axon/spinal regions related? | specific source/target mapped preparations or registered 3D domains | morphology only until WO-03 independent functional evidence |
| Does the source remain stable? | qualified genetic identity and stability assay at source-defined points | discordance/instability → re-review source, not automatic correction |
| Is there actual function? | approved electrophysiology/calcium or other validated recording with controls | recorded activity does not establish a learned relation |

Ordinary candidate markers can include SOX2/PAX6 in progenitor context; neuronal TUBB3/MAP2/RBFOX3; context-dependent spinal MNX1/ISL1/CHAT; glial SOX9/ALDH1L1/GFAP; and selected neurite/synapse markers. **The exact panel is not a universal recipe**; the model-specific PI selects it based on source, region and primary scientific question.

## 5. Decision logic when structure changes

~~~text
New size/shape change?
  -> check specimen, stage, imaging geometry and comparability
  -> check authorized care service/medium lot and healthy source status
  -> compare matched stage-appropriate reference, not just yesterday
  -> scientist classifies NORMAL_VARIATION / REVIEW_TREND /
     POSSIBLE_HEALTH_ISSUE / UNOBSERVED / INVALID_SOURCE
  -> if concern, HOLD_NEW_EXPERIMENT and qualified care/PI review
  -> if accepted stage, record only qualified observation, no learning inference
~~~

A structural change that looks like "more neurons" may be glial expansion, cell size change, segmentation or tissue damage. A true cell-division claim must link independent birth observations under WO-00; no algorithm should add new genetic material or induce proliferation based on shape.

## 6. Engineering records and release evidence

- **SoilRegion**: [schema](../schemas/biology/soil_region.schema.json), region/phenotype, 3D coverage, actual image or source method, measured viability and unobserved volume.
- **CellIdentityObservation**: actual cell and track IDs, source lineage, assay/ROI and confidence.
- **AxonProjection**: only observed process mapping with source region, target candidate and validity, not implied function.
- **SustiBusCareStatus**: actual independent care plan and signed service receipt, or explicitly UNQUALIFIED.
- **ChemiBusObservation**: sample/analyte/form/unit and calibration, with NOT_MEASURED for missing/invalid assays.

**Source-to-stage release packet:** initial source authentication, current qualified SOP, actual batch/specimen IDs, trend charts, representative raw images and spatial coverage, valid cell-state/viability evidence and assigned qualified reviewers. The release **must specify what phenotype and what observation window it covers**, not "Animon maturity 100%."

## 7. Exit gate and next work

A Soil candidate becomes **READY_FOR_PROPOSED_CONNECTIVITY_REVIEW** only when authentic source and stage identity, actual tissue health/care, imaging/electrical observation validity and independent source control have been judged adequate for WO-03's specific research question. That is not "tissue can store memory".

If any critical source/care/safety prerequisite is missing → HOLD, continue approved independent maintenance and seek review. Next: [WO-02 daily care](WO-02-daily-care-and-chemistry.md) and [WO-03 axons/spinal Root](WO-03-axon-and-spinal-root-evidence.md).

**References:** [ISSCR model systems](https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems); [Neural organoid quality and characterization framework](https://www.nature.com/articles/s41586-024-08487-6); [multi-criterion cortical organoid QC study](https://pmc.ncbi.nlm.nih.gov/articles/PMC12340003/). They motivate study controls, not universal Animon acceptance thresholds.

© 2026 We Develop Software LLC. Confidential patent excluded.
