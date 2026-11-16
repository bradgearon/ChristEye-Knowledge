---
title: "LAB-R1-04 — Abnormal readings, investigation, care changes and incident review"
document_id: "LAB-R1-04"
revision: "0.1"
date: "2026-11-16"
status: "Controlled observation/deviation process; does not authorize physical chemical interventions"
owner: "We Develop Software LLC"
---

# LAB-R1-04 — What to do when a value, cell or culture changes

A realistic lab document needs **a defined response to abnormal data**, not merely a list of things worth monitoring. This decision framework is intended to be populated with a *real source-qualified* BrainXell/Axion protocol and laboratory safety/quality plan.

## 1. Entry and event capture

For every unexpected measurement, morphological change, missed service, contamination concern, instrument fault or failed task:

1. **Identify the physical unit** (facility, plate, well, cell lot, experiment), real operator, timestamp and source SOP; record which subsystem generated the alarm.
2. **Preserve the original evidence** (unaltered image, assay, instrument trace, raw measurement, service receipt and calibration). Avoid editing yesterday's record to make a trend disappear.
3. **Compare only compatible states:** same well/preparation, sample method, units, location and qualified equipment. Note any medium change, time shift, instrument reconfiguration, source/maturation or observer change between readings.
4. **Check data validity:** a missing, stale, uncalibrated or out-of-coverage instrument makes the state UNKNOWN or INVALID; it is not a reliable physiological low or high.
5. **Classify impact:** essential care threat, suspected contamination/source integrity, measured environmental deviation, instrument fault, readout-quality fault, growth/phenotype difference, or learning-only issue. These categories have different authority and response paths.
6. **Set a decision:** qualified observation only, DEVIATION_REVIEW, HOLD_NEW_EXPERIMENT, or immediate **facility safety/care escalation**. The system itself cannot choose a chemical, dose, genomic procedure, stimulus or cell-growth response.
7. **Assign an accountable person and completion condition**; record what the actual authorized operator decided and its controlled work order.
8. **If the lab executes an approved corrective service**, link its signed source SOP/version, reagent lot, actual physical receipt, pre/post observations, safety outcome and whether a new baseline is required.
9. **Close only after review**: disposition, affected data/experiments, new qualification state, negative findings and signoff. Never silently reclassify REJECTED data as observed.

## 2. Decision matrix with concrete examples

| Event | Scientist/technician verification | Immediate interpretation | Next authorized action and re-entry gate |
|---|---|---|---|
| Apparent contamination or unexpected material | microscope/source history, independently qualified culture hygiene tests, operator contamination SOP | **POSSIBLE_CONTAMINATION**, invalid new biological experiments | isolate/escalate under facility biosafety SOP; no experimental intervention until source restored and signed |
| Medium yellow earlier than usual | previous qualified medium color/time, actual volume, lot, incubator alarms and pH/chemistry if measured | **CARE_DEVIATION** rather than proof of deficient learning | alert culture lead; if vendor/lab permits an early service, the qualified operator performs and records it; rebaseline after actual service |
| One well has unexpectedly lower visible volume | check physical level/evaporation/edge location, last verified service, source and contamination context | **VOLUME_OR_SERVICE_DEVIATION** | authorized care lead checks under source-specific SOP; protect independent care, do not assume volume lost = cellular uptake |
| pH, osmolality, glucose or lactate outside qualified band | check units, sample compartment, calibration, quantitation limit, recent medium exchange | **OUT_OF_BAND** or **INVALID_ASSAY** | notify human care/chemistry lead, hold new learning; approved quality/service assessment separately |
| Axonal/neurite morphology changed | matched imaging, region and cell source, time, segmentation, multiple-marker qualification | **STRUCTURAL_OBSERVATION_ONLY** | update registered morphology and request scientific review; do not declare new neural Root |
| Sudden spike-count increase/decrease | channel quality, electrode/reference artifact, environmental changes, decoder/gain, source and care | **READOUT_DEVIATION** | record G3 HOLD/REBASELINE; never infer gained/lost knowledge from firing rate |
| Learning task wrong | actual delivered cue, permitted source rights, ground-truth blinding, codebook and readout status | **TASK_FAILURE**, not nutrient deficiency | review digital validity and controls; no food/peptide change |
| Old Root regression after new task | protected old task, reader, exposure and care source | **INTERFERENCE_CANDIDATE** | hold new training, preserve data, scientific decision before proceeding |
| Proposed peptide or growth request | actual molecule/form/receiver/qualified preparation vs requested training benefit | **RESEARCH_REQUEST_ONLY** | open separate change proposal, PI/cell/safety review and approved facility SOP; no autonomous physical action |
| Source line identity/genomic anomaly | supplier/bank identity data, matched actual culture and authorized genomic test | **SOURCE_QUALIFICATION_HOLD** | qualified culture/genomics team evaluates/release; do not automatically edit cell identity/clone |
| Host/web/Worker failure | physical care path unaffected? watchdog, independent owner, experiment evidence | **DIGITAL_FAULT** | hold experiments/education; independently qualified care and environmental support continue |

**Escalation authority:** a biological culture operator and facility supervisor direct actual care/safety responses, while digital Lotus only records flags and permitted digital actions. If a source/medium lot is uncertain, the correct disposition is HOLD, not an "estimate" of safe dose.

## 3. Qualified limit register, not fictitious healthy ranges

Every analyte/instrument has a separate **parameter lock record** with source/protocol, unit, sampling location, method, reference, lower/upper qualified control band, allowed drift criterion, the independent study owner and approval date.

- **Not qualified:** ranges unset. The observation may be logged, but it cannot receive a PASS or authorize a biological experiment.
- **Within signed band:** no alert from that parameter; any other unresolved care/assay/permission issue can still hold the study.
- **Outside signed band:** status DEVIATION_REVIEW. A default automatic correction would be unsafe.
- **Missing/stale/changed method:** NOT_MEASURED or NOT_COMPARABLE, not a falsely normal value.
- **After physical change:** preserve both old and new series and explain whether they are comparable; only a qualified source/model-specific review may set new limits.

**Do not copy the exact 2D Axion supplier bands into MaxOne, organoid Soil, a spinal Root tissue model or a physical Crystal study.** Any future tissue growth/modulation must be a separate research campaign with a protocol and qualified source.

## 4. Corrective-change request (CAR) packet

A completed CAR must contain:

- CAR ID, initiating evidence and source/well/care context.
- **Proposed action class:** observation repeat, equipment recalibration, qualified maintenance service, source rejection, digital decoder reversion, or separate biological research proposal.
- Actual vendor/lab SOP ID and version, approved operator and whether the change is physically performed.
- Current/previous valid measurements, comparability and alternate explanation; data retention/re-analysis impact.
- Impact on cell identity, axon morphology, morphology and old learned tasks, as appropriate.
- Original qualified care continuity and independent limit signoff.
- Postcondition source receipt and requalification/hold status.
- Scientific PI, culture lead and safety review signatures for any relevant biological change.
- Status DRAFT, HELD, APPROVED_FOR_SEPARATE_SOP or REJECTED. **No auto-execution state.**

The [biology change proposal schema](../schemas/biology/change_proposal.schema.json) is a machine data contract; the form in [deviation_template.csv](forms/deviation_template.csv) is the technician log. Neither can command a pump, genomic edit, cell source change or stimulation.

## 5. Example: from Day N to Day N+1

Yesterday's valid observation had one medium lot, one instrument configuration and a normal-appearing culture. Today the medium's color differs, volume is visibly lower, or instrument noise rises. Record each as a separate datum with its own source and comparability. Do not declare all three reflect the same cell metabolic mechanism.

**Actions:** (i) hold any new teaching trials, (ii) preserve both days, (iii) check care receipt and source lot, (iv) notify the qualified care/instrument leads, (v) perform only the approved local maintenance/diagnostic action if authorized, (vi) record before/after receipt and rebaseline status, (vii) resume only after signed release.

The response is a controlled engineering/lab process even if the cause remains unknown.

## 6. Review completion

The incident is not closed unless a named reviewer accepts the disposition, source data remain unaltered, missed/abnormal care is accounted for, affected experiment claims are updated and the appropriate G1–G5 gates are re-evaluated. A missing datum and a negative finding must never be silently discarded.

**Related:** [source release](01_SOURCE_AND_MATERIAL_RELEASE.md), [daily work](02_DAILY_OPERATION_AND_QC.md), [MEA and learning](03_MEA_AND_EVALUATION.md), [SustiBus/ChemiBus](../specifications/v3.2/design/CE-A17_Animon_Care_Design.md), [Axons and Root evidence](../specifications/v3.2/design/CE-A16_Animon_Spinal_Root_Design.md).

© 2026 We Develop Software LLC · WDS Public Research Reference License. Patent document excluded.
