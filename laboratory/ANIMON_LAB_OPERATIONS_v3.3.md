---
title: "ANIMON-LAB/3.3 — Laboratory execution program and daily operations"
date: "2026-11-16"
owner: "We Develop Software LLC"
status: "Operational work orders; physical steps depend on locally qualified SOPs"
---

# Animon Laboratory Operations — v3.3

**The purpose of this release is real scientist-facing work, not another conceptual glossary.** Every activity below has an owner, input, physical/digital evidence, decision, response to failure, and handoff. The [existing v3.2 designs](../specifications/v3.2/README.md) remain the scientific architecture. This work-order set is the *execution and QC layer*.

**Three work classes:** DOCUMENT/DIGITAL (can be executed with current software and source records), PHYSICAL LAB/SOP (only after a chosen institutional procedure and qualified personnel are authorized), and RESEARCH HYPOTHESIS (no actual procedure known or validated). DNA extraction, cell culture/differentiation, changing nutrient/peptide conditions, tissue sampling and electrical interventions belong to the second or third category. This document does not substitute guessed settings for an actual protocol.

## 0. Responsibility map

| Role | Specific task / signoff |
|---|---|
| Scientific PI | chooses biological source and model, primary question, controls, actual independent unit, ethical review and permitted SOPs |
| Cell-foundry lead | accepts authenticated source, cell lineage/genotype/QC, cell/culture history and lot change |
| Culture/care lead | owns approved physiological care, monitors the actual preparation and escalates disturbances regardless of task results |
| Molecular/omics lead | specifies DNA/RNA/protein/chemistry specimen, assay, custody, QC and interpretation |
| Neuroscience/imaging lead | signs region, cell type, axon/dendrite, channel mapping, source-to-target validity |
| Embedded safety lead | checks sensor/instrument clocks, digital/electronic isolation, independent care continuity and denied actuation |
| Data QA | checks source hashes, units, provenance, completeness, previous-day comparisons, invalidation and final archive |
| Statistician/evaluator | confirms control/batch hierarchy, readout metrics, protected skills, exclusions and outcome uncertainty |

If the required role or authorized procedure is missing, the physical work item is **HOLD**. Essential care already assigned to a laboratory must continue independently of a software hold.

## 0A. Phase-specific physical execution order from the original P01 source

**Do not mistake the early planar reference for a 3D Soil organoid.** The [P01 composition and counts](../phases/P01/docs/01_Physical_Module_Composition_and_Counts.md) defines **R0 as all-digital; R1 as a candidate 2D planar C learner**, using a supplier-specific illustrative 24-well MEA co-culture of **80,000 neurons + 20,000 matched astrocytes per well**; R2 optional observer/recipient wells; R3 a support-policy candidate; and **P06 as the later 3D Soil branch** whose cell count is NOT SET. The precise 2D count is not an authorized Animon cell-plating instruction and cannot be applied to 3D.

Read the [source-derived phase and material bill-of-materials](ANIMON_PHASE_AND_MATERIAL_BOM.md) before procurement. A real lab must qualify the selected vendor/cell pair, actual model geometry, source line and medium/care SOP.

## 1. Site-work order prerequisites and progression

| Gate | Work order | Required record before next stage |
|---|---|---|
| G00 | [WO-00](WO-00-source-and-genomic-qc.md) | chosen qualified institution, PI, ethics/material authority, source reference, approved methods, actual instrument owners |
| G01 | WO-00 | authenticated banked cell line/lot, phenotype/genetic certificates where appropriate, passage history, consent and contamination/QC outcomes |
| G02 | [WO-01](WO-01-soil-development-and-maturation.md) | real source-qualified neural preparation and its approved tissue-generation SOP; batch and specimen identifiers |
| G03 | [WO-02](WO-02-daily-care-and-chemistry.md) | independently maintained source-specific care plan, authenticated service receipts, baseline assay and environment |
| G04 | WO-01 | measured region/cell morphology, glial/neuronal identity, 3D observation coverage, maturity qualification |
| G05 | [WO-03](WO-03-axon-and-spinal-root-evidence.md) | separated anatomical/functional axon and source/recipient evidence; artifacts and uncertainty |
| G06 | [WO-04](WO-04-learning-and-delayed-retention.md) | approved input/reader, controls, actual exposure, immediate/delayed tests, old skill and care comparisons |
| G07 | all | evidence package with signed interpretation: scoped finding, negative, HOLD or unresolved |

Age, tissue size, protein intensity or high firing rate alone **never** advances a gate.

## 2. Mandatory work-order header

Every work order and daily sheet has fields:

~~~text
work_order_id / step_id / preparation_id / source_lot_id
animon_spec_id / experimental_batch_id / site and instrument IDs
execution_class / operator and qualified role / review owner
current approved SOP ID, revision and authorized action
precondition references / verified previous action / source sample IDs
event UTC clock, local lab clock and instrument timebase
raw data URI + SHA-256; derived result URI and analysis version
measurement units, spatial compartment, calibration and missingness
comparison source: previous valid day, accepted baseline, peer preparation
primary outcome, limits and review band sourced from actual approved SOP
status: NOT_STARTED, VALID, REVIEW_TREND, HOLD_NEW_EXPERIMENT,
        INVALID_MEASUREMENT, NONCOMPARABLE, UNRESOLVED, REJECTED
deviation number / who reviewed / next allowable step
~~~

**Synthetic examples never become real preparation IDs.** A specimen or source without real custody cannot be advanced to observed status.

## 3. Every-day lab workflow — shift start

1. Identify actual prep/source/lot and read prior shift's signed handoff. Retrieve pending incidents, missed care receipts, assay exceptions, restrictions and booked sampling times. Missing identity = HOLD new research.
2. Check source-qualified maintenance SOP and authorized operator. Confirm that *real physiological support remains independently owned* even if Animon/ Lotus software is offline.
3. Confirm current instrument calibration, date/time synchronization, sensor validity and method version. Noncalibrated or unsourced readings cannot establish physiology.
4. Obtain the day's measurements **per the approved preparation-specific SOP** and log values with units, sample location, source method, sample time and actual reader (not an inferred value).
5. Link any real care or sample service event to signed physical receipts, including qualified formulation/lot and change history. Scheduled does not mean performed.
6. Compare each valid and comparable observation with: (i) site-approved absolute limit, (ii) previous comparable valid day/shift, (iii) accepted baseline or time-matched control, and (iv) whether care/method/source changed.
7. A critical SOP deviation, missing real service receipt or source/QC failure produces HOLD_NEW_EXPERIMENT and immediate lab lead escalation under its independent SOP. **Do not auto-feed, alter medium/peptide, start stimulation, or discontinue essential care.**
8. Only after care/source/instrument identity is verified may an already-approved biological study procedure proceed. Record the actual step performed under its own local method, or explicitly NOT_PERFORMED.

## 4. During acquisition and end-of-day handoff

9. Each actual DNA/RNA/protein/imaging/MEA/chemistry acquisition gets sample or instrument ID, method and operator, spatial origin, UTC instrument clock, calibration, source and artifact handling, raw immutable file hash and original storage.
10. If a medium lot, assay, optical transform, culture stage, source line, drug/peptide exposure or reader version changes, mark prior/today results NONCOMPARABLE unless a qualified bridging study exists.
11. Compute and retain delta from previous valid comparable value and deviation from stable, stage-appropriate qualified baseline. Do not silently impute missing days.
12. A human checks out-of-range, missingness, confounding, artifact, contamination or old-skill decline. Retain raw readings even when invalid; separate data validity from biological state.
13. Write a shift-handoff record: accepted studies and care operations, HOLD reasons, next authorized observations, responsible names, signed review and expiry of calibration/baseline approvals.

## 5. Previous-day comparison — exact software rules

For same-preparation, same-stage, same-method and same-unit comparable values:

\[
\Delta_{24h} = x_{today} - x_{previous}, \qquad
\Delta_{\%}=100\frac{x_{today}-x_{previous}}{|x_{previous}|}.
\]

If previous value is null/zero, ratio is undefined. If method, medium lot, sample region, instrument calibration or underlying culture state changes without bridging validation, output **NONCOMPARABLE**, not a misleading percent change.

A history-based *screening* signal may use:

\[
z_{robust} = \frac{|x_{today}-median(x_{qualified baseline})|}
{1.4826\, MAD(x_{qualified baseline})}.
\]

This is only a **software anomaly screen** when sufficient stable baseline and measurement comparability exist. It is not a universal biological danger limit; stage drift and zero MAD invalidate it. Critical low/high and warning bands **must be transcribed from the real source- and stage-qualified SOP**, not invented by the assistant.

| Condition | Data system decision | Qualified human action |
|---|---|---|
| SOP critical bound exceeded | HOLD_NEW_EXPERIMENT | lab care lead applies its approved contingency and reviews whether study evidence is valid |
| warning band exceeded (without critical breach) | REVIEW_TREND | verify sample/method/history and decide whether follow-up or invalidation is warranted |
| missing source, care receipt or valid calibration | INVALID_MEASUREMENT or HOLD | check physical care independently; repair records under QA control |
| source/culture/assay method changed | NONCOMPARABLE | perform a scientifically justified bridge or begin a new baseline |
| too little comparable history, absent limits or absent previous reading | UNRESOLVED | collect/qualify data and do not infer physiology |
| valid comparable measurement within actual SOP and review criteria | NORMAL_RECORD | archive and proceed with only already-authorized work |

A reference [non-actuating daily QC tool](../qa/daily_qc.py) is used for deterministic classification and never controls lab equipment.

## 6. Genetic source plan and specific material categories

**Proposed starting material class for scientific review:** authenticated, research-consented **banked human iPSC line(s)** or another institution-approved neural source, with independent genetically distinct source lines as appropriate to the study. This is *not* authorization to solicit genetic samples from individuals or derive cells without a qualified institution.

The [WO-00 procurement and material qualification order](WO-00-source-and-genomic-qc.md) requires:
- Supplier/bank name, accession, line/lot identity, tissue-of-origin class, passage/population-doubling history, transport and transfer/consent rights.
- Qualified line identity and genomic integrity reports (e.g. **STR or SNP profiling**, model-relevant genotype, appropriate **karyotype or copy-number assessment**), with exact certificate/version and any re-test triggers.
- Contamination screening and culture identity evidence, including an appropriate **mycoplasma result**, pluripotency/source phenotype and assay controls where applicable.
- Independently specified specimen streams: genomic **DNA identity**, **RNA expression/lineage**, **protein/spatial phenotype** and **extracellular chemistry**. Each receives a sample ID, actual region/time, source/assay, raw file and handling chain. **The real laboratory** defines extraction and tissue-handling methods and necessary reagent/physical parameters.
- An explicit decision whether any genetic cell ID is needed. Default is *external observed* IDs and lineage reference. Genetically inserting unique daughter identifiers is a separate unvalidated engineering proposal, not assumed in G01.

**Source release rule:** if the actual source, permissions, line authentication, contamination and required genome stability checks are unqualified, no biological preparation enters the learning program.

## 7. Change control and actual stop/handoff

Each material, assay, reader, source, phenotype or physical research change must record change ID, current method/version, scientific need, expected endpoints, effects on existing Roots/care, independent human approvals, actual service receipt and later comparison validity. A low test score is **not** a feed/peptide/growth authorization.

The scientist's complete handoff is [WO-00](WO-00-source-and-genomic-qc.md) → [WO-01](WO-01-soil-development-and-maturation.md) → [WO-02](WO-02-daily-care-and-chemistry.md) → [WO-03](WO-03-axon-and-spinal-root-evidence.md) → [WO-04](WO-04-learning-and-delayed-retention.md).

**Source standards:** [ISSCR stem-cell model systems](https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems), [ISSCR reporting](https://www.isscr.org/basic-research-standards/reporting), [2025 Nature neural-organoid framework](https://www.nature.com/articles/s41586-024-08487-6). These standards support rigorous source and model validation. No source provides an Anaiero-specific validated culture protocol.

© 2026 We Develop Software LLC. WDS Research Reference License. Confidential physical patent excluded.
