---
title: "LAB-R1-02 — Daily operation, medium service, source QC and previous-day comparison"
document_id: "LAB-R1-02"
revision: "0.1"
date: "2026-11-16"
status: "Technician batch-record design — physical execution depends on selected current vendor/lab SOP"
owner: "We Develop Software LLC"
---

# LAB-R1-02 — What the technician does and records each day

**Scope:** first 2D **BrainXell spinal motor-neuron + astrocyte / Axion Maestro Edge 24-well MEA** reference culture. Every numbered day below is relative to the actual **Day 0 plating**. This is not a validated recipe for 3D Soil, a spinal Root organoid or a genetically modified Animon.

**Physical operations are governed by an attached approved vendor/institution SOP, not by a guessed Animon formula.** The older [Axion/BrainXell v10.0 reference](https://www.axionbiosystems.com/resources/culture-protocol/neuronastrocyte-24-well-mea-co-culture-protocol-v100) describes a Day 0 / Day 1 / Day 4 / Day 7+ maintenance sequence, whereas [BrainXell currently lists a v10.1 version](https://brainxell.com/protocols/). The culture lead must record which full revision the selected facility signed and reconcile any changed wording, cell composition, surface or volumes before issuing an actual Work Order.

**Forms:** [daily observation](forms/daily_observation_template.csv), [source/lot receipt](forms/source_receipt_template.csv), [physical service](forms/culture_service_template.csv), [deviation](forms/deviation_template.csv). [Deviation response logic](04_DEVIATIONS_AND_CHANGE_CONTROL.md). An unqualified monitor band must stay UNQUALIFIED; no default range from a different cell line may be used.

## A. Every daily shift: an actual inspection process

At the **scheduled daily observation** time for each well:

1. Open today's controlled row with **actual timestamp**, technician, incubation/plate/well ID, preparation/source lot, day-in-vitro (DIV), culture service SOP revision, experiment group and last valid comparison observation ID. Verify the physical labels match the database before handling.
2. Inspect the current **source/care status** before interpreting signals. Confirm signed last physical maintenance receipt, planned next maintenance due time, known material lot, incubator monitoring validity and any alarms since the last observation. A scheduled service is *not* evidence a service occurred.
3. Document visually the current well morphology with **registered microscope image ID** or NOT_CAPTURED; inspect attachment/coverage, unexpected debris, detachment, cell/neurite appearance and apparent contamination. Record observed abnormalities, not inferred viability or disease.
4. Record the actual current **medium condition**: visible color and evidence of volume change/evaporation, actual measured volume if the lab has a validated method, separate feed/sample/waste route identity, medium lot and last service type.
5. Read environmental monitoring for temperature, gas/CO₂ (if monitored), pH, osmolality, glucose, lactate and other source-qualified assays **only if measured with a validated method, location and calibration**. Record value, units, location, instrument, clock, assay validity and raw reference. Otherwise enter **NOT_MEASURED**, never normal or zero.
6. Compare each numerical field with **the previous valid observation from the same well, assay, units and relevant service condition**. Calculate delta, relative change if denominator permits, and label a discontinuity (new medium lot, sampling, incubator move, electrode session, assay revision). Use only the scientist-approved band and trend rule from the controlled parameter register.
7. Review **alarms/deviations** before authorizing any new training or assay. If a critical source, care or instrument measurement is missing/out of qualified range, mark **HOLD_NEW_EXPERIMENT / HUMAN_REVIEW** while the independent culture service continues per its SOP.
8. Confirm the **next scheduled maintenance** is due/not due according to the approved supplier/lab protocol; if due, request the authorized culture operator to perform the corresponding vendor/SOP service and produce a **physical pre/post receipt**, including actual volumes and material lots. Do not derive chemical adjustments from a task score.
9. Close the row with signed operator and reviewer status. Preserve negative observations and all original image/assay files; never replace yesterday's values with smoothed or "healthy" values.

**One row per well per observation period**; any additional chemistry or MEA session has its own linked record. A plate average does not hide a failing well. Each actual culture/biological preparation remains identifiable independently from the wells nested within it.

## B. Day-indexed vendor reference and technician checklist

This table identifies **what the lab must prepare and verify**. Vendor v10.0 values are an *older supplier-specific example*, not approved instructions; freeze and execute the full current source SOP selected at G0/G1.

| Stage | Vendor-source example and planned operation | Before/after observations and required record | Do not proceed when |
|---|---|---|---|
| **Pre-Day 0** | Source release; qualified equipment/plate coating; reagent versions; full approved medium recipe and trained staff | COA and source records, plate map, equipment calibration, SOP revision, sterile/environmental readiness | Any G0/G1 source or service approval missing |
| **Day 0 — plating** | Supplier-controlled thawing, viable count, compatible neuron/astrocyte placement on the MEA and approved initial support. P01 example 80k neurons+20k astrocytes/well; supplier supports a range | actual viable neuron and astrocyte count; lot, plate/well/cell source, initial image, medium/surface lot, physical operator and time | Actual cell count/viability or handling departure fails the chosen lab/vendor release gate |
| **Day 1 — medium transition** | v10.0 specifies Day 1 Medium replacement, with a **460 µL exchange** preserving **40 µL** in the illustrative well; verify v10.1 and the lab SOP before doing it | before/after qualified volume and appearance, exact formulation/lot, elapsed time from plating, performed-versus-planned service, authorized operator | source/SOP or culture volume uncertain, abnormal attachment/contamination, invalid service path |
| **Day 2–3 — observe** | Daily physical observation and independent approved maintenance if actually due; not automatic novel additives | current vs previous-day image/volume/health/exchange; environmental alarms; new specimen record only if qualified | off-band / suspected contamination / missing care receipt |
| **Day 4 — source-defined addition** | v10.0 describes a **500 µL Day 4 Medium addition** to reach **1 mL**/well; do not borrow for another geometry | current and previous viable observations; qualified culture/media lot, actual volume receipt and whether service changes comparability of subsequent chemistry | current version not reviewed or service/adherent state invalid |
| **Day 5–6 — observe** | Daily monitoring; verify care schedule, imaging and note anomalous appearance; do not infer maturation from day number | previous-day deltas, external environmental monitoring, controlled alarms | G2 source/viability/support not qualified |
| **Day 7 onward — ongoing maintenance** | v10.0 refers to **half-medium changes** on a qualified 2–3-times-weekly schedule, commonly indicated Day 10/13/16 etc. **The current approved version controls actual dates/amounts.** | each due service log with actual before/after volume, formulation lot, receipt, deviation, sensor and task-session separation | maintenance overdue/abnormal volume or wrong lot; escalate via independent care lead |
| **From around DIV12 — activity observation** | v10.0 states neural activity may become visible by roughly DIV12; do **not** treat it as a universal pass threshold | recorded real source/channel and 10-minute manufacturer-reference acquisition where valid; controls, artifact and active electrode count | instrument/source reference, clock and baseline not qualified |
| **Around DIV21 and beyond — trend characterization** | Supplier example reports neural activity approaching a plateau around DIV21 under its specific preparation | real stable multi-session baseline rather than one snapshot; pre-specified G2/G3 quality metrics and batch variability | source drifts, contamination, reader changes or unverified care explain signal |
| **Any later research stage** | Defined training/delayed evaluation ONLY after G3, PI study release and independent limit permission | actual task condition, external controls, sealed evaluator and protected earlier Root results | no valid measured baseline, no evidence of delivered cue or ethics/PI release |

**Important:** BrainXell's current protocol index lists v10.1 and P01 references Axion v10.0. The above historical quantities should help the purchasing/lab team **reconcile** the vendor procedure, not replace the complete current procedure. Do not mix an Axion 2D medium schedule, a MaxOne plate and a 3D Soil experiment.

## C. Every measurement needs an acceptance band with provenance

For a real numeric observation x_today and the prior **comparable valid** observation x_prev:

- **Absolute change:** Δx = x_today − x_prev, with instrument unit and reference IDs.
- **Percent change:** Δ% = 100×(x_today−x_prev)/abs(x_prev) only if x_prev is finite and nonzero.
- **Qualified band comparison:** x_today must be compared with a signed source/model/instrument-specific range [L,U], and, when declared, a verified allowable absolute day change Δ_max.
- **Service boundary:** compare before-service and after-service readings explicitly; do not treat changed medium/volume/reagent or instrument calibration as the same unbroken baseline.
- **Scientific effect:** a change in calcium/electrode signal with no valid task/reader is a physiology observation, not new Understanding.

**No lab-qualified range, no pass.** A number can be observed and its delta computed, but cannot be declared "normal", "safe", or "ready for teaching" without a named signed control plan. Do not substitute typical human blood or unrelated organoid reference ranges.

| Variable | What the qualified team defines | If abnormal or missing |
|---|---|---|
| Incubator temperature, gases / environmental state | actual installed sensor calibration, source SOP target and alert window | notify care operator; **hold new experimental inputs**, independent maintenance/incident response continues |
| Well volume/evaporation | approved geometry-dependent volume band, servicing interval and measurement method | care lead checks service path; if visibly abnormal, inspect under facility SOP and record deviation |
| Medium appearance and contamination signs | source morphology/medium color characterization and lab biosafety rules | suspected contamination → isolate/notify and follow facility contamination SOP; no ad hoc treatment |
| Morphology/neurite and viability metrics | actual source phenotype, blinded image method, reference and method uncertainty | investigate sample handling, source drift and health before interpreting learning |
| pH, osmolality and analytes (if measured) | actual sample location, assay calibration, source-specific interpretive range | report NOT_MEASURED/OUT_OF_BAND and protect independent care; no automatic reagent/peptide addition |
| Electrode noise, active channels and burst/readout quality | OEM/instrument qualification, artifact/sham baseline and study-specific G3 limits | invalidate that session's task interpretation; do not label silent tissue dead |
| Learned task/readout scores | independent evaluator, frozen codebook, valid controls, study analysis plan | NO_ROOT_PROMOTION/UNRESOLVED, not "feed more" or "grow more" |

## D. Example abnormal-event decision process

**Case:** yesterday's medium was visually unchanged; today's medium is unexpectedly yellow before the approved next service.  
Record both appearances, time since verified last service, current volume, source/lot, incubator alarm and any validated pH/analyte measurement. **Flag PREMATURE_MEDIA_CHANGE**, hold new experimental/training actions and contact the authorized culture lead. The vendor v10.0 discusses early service under this condition, but **the actual operator must use their qualified source/SOP**, record what they did and mark the service as a discontinuity. Never add a custom component or "molecular Word" to compensate.

**Case:** electrode activity is substantially different while an instrument upgrade or medium change occurred.  
Do not compare a raw score to the previous session as if all conditions were identical. Record the changed instrument/assay, declare READOUT_CHANGE, re-establish qualified baseline and reject any inference about erased memory until independently tested.

**Case:** a cell or axon appears to grow.  
Use qualified imaging to register its geometry and source uncertainty. It is **not** a CellBirthEvent unless a division was observed and is not a functional Root without later task evidence.

## E. Required daily signoff fields

Daily record cannot close without plate/well/date/operator, source/medium lot, last qualified receipt, observation statuses, baseline comparison or reason unavailable, control-limit revision, next care duty, deviations, experimental HOLD/REVIEW decision and reviewer status. An unobserved item is **NOT_MEASURED** rather than a fabricated 0.

Daily quality automation is specified in [LAB-R1-04 exception protocol](04_DEVIATIONS_AND_CHANGE_CONTROL.md), with a simple read-only trend analyzer in [check_daily_records.py](scripts/check_daily_records.py). That script never controls culture or authorizes experimental exposure.

Next: [MEA/baseline and learning decisions](03_MEA_AND_EVALUATION.md). Source values and work-order authority ultimately belong to the signed current manufacturer/local laboratory protocol.

© 2026 We Develop Software LLC · WDS Public Research Reference License. No physical patent document published.
