---
title: "LAB-R1-03 — MEA acquisition, baseline control and learning-evidence work order"
document_id: "LAB-R1-03"
revision: "0.1"
date: "2026-11-16"
status: "Source-specific instrument and study record; no novel electrical protocol authorized"
owner: "We Develop Software LLC"
---

# LAB-R1-03 — Work to establish readout before claiming Animon learning

**Reference branch:** Axion Maestro Edge, compatible BrainXell motor-neuron/astrocyte 24-well co-culture. **Do not use the MaxOne channel geometry/settings in this experiment.** P01's [electrical companion](../phases/P01/docs/03_Electrical_WindBus_and_Training_Companion.md) contrasts those instruments precisely because their signal paths and seeding geometries are different.

The aim is **first to prove reliable observation**, and only later, in a separately approved study, to test defined adaptation and delayed performance. No new electrical stimulation sequence or live learned-Word code is issued by this packet.

## Before the first MEA recording — G2/G3 work order

1. **Confirm cell/source release:** link campaign, plate, well, neuron/astrocyte lot and qualified care status; record current DIV, observed morphology and whether any service or disturbance has occurred since the last measurement. No valid biological source or health context → HOLD.
2. **Qualify physical device:** record Maestro Edge serial, firmware/software and electrode plate ID, installed SDK revision, qualified acquisition configuration, channel/electrode map, environmental control and calibration/service receipt.
3. **Reference and controls:** acquire a qualified device/reference baseline per the OEM SOP, record sham/artifact characterization and negative control route, and archive source/raw files before any new stimulation/teaching. If the device cannot distinguish signal from known artifact, stop G3 inference.
4. **Predefined readout fields:** raw data URI + digest; plate/well, channel, sample/timebase, temperature/gas record, actual recording duration, artifact mask, noise and signal-quality metrics defined by the lab. Exclude invalid channels according to a pre-signed rule, never because they hurt the expected outcome.
5. **Protect comparability:** freeze analysis pipeline revision and prior valid comparison record. If the codebook, electrode routing, gain, filter, medium composition or sampling procedure changed, mark REBASELINE_REQUIRED and investigate before inferring physical neural change.
6. **Preserve independent care:** the operator confirms the next maintenance duty; MEA/teaching never displaces a required culture service, and a missing healthy readout never disables Milk.

## Supplier-specific recording reference, clearly separated from lab authorization

The named Axion/BrainXell v10.0 reference recommends recording around conditions suitable for the culture, an additional **10–15 minute equilibration** in a preconditioned Maestro Edge, and a **10-minute reference recording**. It also notes that recording may be performed **before a scheduled medium change on the same day** to avoid avoidable comparisons across changed media. The supplier described activity becoming apparent by about DIV12 and approaching a plateau around DIV21 *in that model*.

**Bench action:** after the lab signs the current compatible vendor/MEA SOP, the operator executes *that version*, not a guessed signal profile. Record actual settings and durations as performed, and do not treat DIV12/DIV21 as a universal maturity or memory threshold. [Older co-culture reference](https://www.axionbiosystems.com/resources/culture-protocol/neuronastrocyte-24-well-mea-co-culture-protocol-v100) · [current manufacturer protocol index](https://brainxell.com/protocols/).

## Daily/each-session readout and comparison record

| Data element | Required entry | Same-well previous-session comparison | Exception |
|---|---|---|---|
| Source and well | preparation, neuron/astrocyte lots, plate ID, well group and DIV | same real biological source, not just similar SKU | SOURCE_MISMATCH → stop interpretation |
| Care context | verified physical receipt, actual service/reagent lots and environment state | changed medium/formulation means annotated discontinuity | CARE_UNQUALIFIED → suspend new learning |
| Instrument/time | OEM configuration, clock, calibration, raw path+checksum and duration | same or formally compatible acquisition revision | READOUT_CHANGED → no retention claim |
| Electrode reference | reference noise/artifact metrics, channel mask, active readout coverage | qualified previous calibration/reference | OUT_OF_RANGE/MISSING → requalification |
| Spontaneous physiology | valid session-level firing/burst/synchrony metrics and units, if supported by measured data | same definitions, comparable window and channel subset | data quantity alone ≠ comprehension |
| Task exposure | ordered condition, actual delivered cue receipt, randomized/blinded assignment, feedback source | same codebook/permission and valid comparison design | NO_DELIVERY_PROOF → invalidate task result |
| Performance | per-class X/Y/HOLD results, full valid/invalid confusion record | frozen decoder and declared task split | changed reader/leaked labels → UNRESOLVED |
| Delayed probe | actual rest/interval and absent teaching route, sealed task labels, protected old tasks | compare immediate and delayed using predeclared analysis | no delayed session → NOT_TESTED |
| Molecular/axon hypothesis | sample/structure/region assay ref as separately observed | match source identity, time/compartment and actual coverage | missing source/assay → no mechanism claim |

The **previous-day comparison** is a *quality/interpretation tool*. Neuroscience signals vary; a significant numerical change alone is not a valid diagnosis, and a shared electrical drive can look like a network connection.

## Four-condition initial study layout (P01 proposal)

P01 offers one 24-well reference as **four conditions with six wells each**. The study director must decide a *scientifically justified* arrangement and register plate randomization, blinding and the exposure/feedback information budget. An example analytical framework is:

- **A / contingent:** intended feedback linked to the predeclared task under an institution-approved instrumentation study.
- **B / yoked noncontingent:** matched permitted exposure, not carrying the same answer information to the learner.
- **C / time-matched no-feedback:** controls for elapsed development, repeated handling and ordinary network maturation.
- **D / sham/readout:** separates source/reader behavior and delivery/artifact from any inference of learning.

**Implementation dependency:** this is an experimental-design proposal, not a command to stimulate live neurons. Actual stimulation parameters, controls, electrical exposure, safety envelope and SOP must be selected and independently approved by the qualified electrophysiology team. Six wells on one plate cannot substitute for multiple independent preparations or multiple cell-source batches. A digital exact-rule oracle is evaluated separately.

## Session lifecycle — what closes G3 vs G4 vs G5

**G3: acquisition qualified (before teaching)**

1. Qualified source/care and no unresolved contamination/health event.
2. Real instrument/firmware/channel map, calibrated time and artifact controls.
3. Recorded valid spontaneous or appropriate baseline response with a clear input/readout contract, not merely visually pleasing traces.
4. Frozen analysis codebook, valid/invalid criteria and authorized operator signature.
5. **Result:** G3_READY_FOR_STUDY_REVIEW or G3_HOLD with specific reason.

**G4: bounded acquisition question (PI-reviewed future experiment)**

1. Predeclared task and primary metric, comparison, sampling hierarchy and exposure/feedback-control allocation.
2. Only **qualified physically delivered** cue receipts enter any biological interpretation.
3. A true independent evaluator receives labels after the response, without the input/readout path receiving answers during the holdout.
4. Matched controls and unchanged care are documented. Actual task output is compared against both chance and the relevant experimental control.
5. **Result:** HISTORY_DEPENDENT_BEHAVIOR_CANDIDATE, NO_ACQUISITION_DETECTED or UNRESOLVED. Do not say the Animon was taught "a word" solely because an electrode fired.

**G5: retention question (separate study review)**

1. Capture a valid immediate/near-term post-training task assessment.
2. Define and record the later interval in the approved study plan, including how training feedback and teacher source are actually removed.
3. Reapply the **same frozen readout and codebook** at the declared delayed observation. Record every valid and invalid trial and old-skill assessment.
4. Determine if the delayed difference remains beyond the declared matched control, and report source/viability/recording changes and confidence intervals over real independent preparations.
5. **Result:** RETENTION_IN_TESTED_CONDITION, NO_RETENTION_DETECTED, ACCESS_CONFOUNDED or UNRESOLVED. No inference about spinal memory location without independent [CE-A16](../specifications/v3.2/design/CE-A16_Animon_Spinal_Root_Design.md) evidence.

### Score definitions and actual calculations

For each class c in X, Y, HOLD, compute per-class recall = correctly predicted trials of that class / all valid trials of that class. Macro recall is the mean over classes; **all classes must have valid support**. For a proposed delayed task compare absolute scores and class effects first. The chance-corrected ratio R = (delayed - chance)/(immediate - chance) is undefined when immediate performance is too near chance. See [real digital metric implementation](../anaiero/roots.py).

Record the raw trial IDs, condition, answer-carrying channel status and actual source hashes. Do **not** substitute sample synthetic scores or a digital MemoryBank lookup for living retention.

## Recording-day decision table

| Observable condition | Required next record / decision |
|---|---|
| No qualified source or care receipt | Hold experiment and contact cell/care lead; independent maintenance continues |
| Device/reference artifact exceeds signed qualification | Hold G3, investigate under OEM diagnostic SOP, preserve raw data |
| Tissue response changed concurrently with medium replacement | Tag service discontinuity and avoid direct old/new learning claims |
| Low spontaneous electrical activity | Mark measured condition; assess channel/cell/source validity, not automatically dead or memory-lost |
| Learning metric improves but digital decoder changed | ACCESS_OR_READER_CONFOUNDED; repeat only a separately approved comparison with frozen readout |
| Delayed task fails but alternative prior cue succeeds without new teaching | ACCESS_HYPOTHESIS for analysis; no certain anatomical localization |
| Contamination or abnormal health observation | No new biological teaching; facility biosafety/care SOP and real incident record |
| Apparent axon extension but no recipient-only effect | structural observation only; don't promote CE-A16 Root contribution |

## What this first campaign can actually accomplish

A qualified first 2D MEA reference could characterize viability, a reproducible baseline, readout stability, matched experimental controls and—in a separately approved trial—a narrow bounded learning hypothesis. It **cannot by itself** establish a 3D Soil, cloned spinal memory Root, autonomous animon cognition, growth on demand or peptide-encoded installable knowledge.

**Deliverable G3:** real [session record](forms/mea_session_template.csv), raw-source hash, independent source/care receipt, accepted/rejected channels and signoff.  
**Deliverable G4/G5:** separate signed StudyReadinessPacket and original learned/delayed data, if and only if independently approved/collected.

Next: [exception, deviations and corrective-action review](04_DEVIATIONS_AND_CHANGE_CONTROL.md).

© 2026 We Develop Software LLC · WDS Public Research Reference License. Patent document excluded.
