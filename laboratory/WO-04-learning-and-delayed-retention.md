---
title: "WO-04 — Animon teaching observation, independent task testing and retention"
date: "2026-11-16"
status: "Study design and evidence protocol; live biological stimulation requires independent lab SOP"
owner: "Neuroscience PI, independent evaluator, statistician and care lead"
---

# WO-04 — Prove or reject a retained functional Root

**What this work order actually tests:** whether a specific, qualified preparation exhibits a **history-dependent task relationship** that remains usable after a defined interval, while source, care, input, response and frozen decoder remain distinguishable from external software effects. It does *not* use a guessed neural stimulation signal or training schedule, and a stored digital event is not biological learning.

[CE-A02 engineering](../specifications/v3.2/design/CE-A02_Animon_Long_Term_Retention_Design.md) · [CE-A02 scientific companion](../specifications/v3.2/companion/CE-A02_Animon_Long_Term_Retention_Scientific_Companion.md) · [BIO-06](../processes/BIO-06-teaching.md).

## 1. Study lock: before any teaching or test exposure

| Step | Actual PI or evaluator task | Must be frozen or signed before proceeding |
|---|---|---|
| 04-01 | Define one bounded task and its meaningful classes, cue/response contract and independent evaluator | task ID, expected chance/constant-prediction baselines, primary endpoint |
| 04-02 | Confirm WO-00 genuine source qualification, WO-01 physical maturity/identity, WO-02 independent care | prep/source/lot, true sample/model state, current qualified care and approved SOP |
| 04-03 | Validate recording and intended input delivery instruments and true source/recipient channels | OEM and local source SOP, qualified I/O receipt, clock/calibration and artifact mapping |
| 04-04 | Freeze test input map/decoder and split training, holdout and delayed evaluation without shared answer-bearing material | codebook hash, reader digest, immutable split/source rights and access ACL |
| 04-05 | Select actual biological unit and batch/line hierarchy, comparison groups, scientific outcomes and analysis model | signed design, group allocation rationale, primary result and uncertainty plan |
| 04-06 | Define baseline, teaching phase, immediate assessment, qualified delay and later assessment **under approved model-specific conditions** | predeclared time windows and valid comparators, not arbitrary elapsed days |
| 04-07 | Establish controls for maturation, repeated exposure, noncontingent feedback, host adaptation, instrument artifacts and prior Roots | source-matched comparators and independent care-state tracking |

The actual laboratory must approve all **physical cue/stimulation arrangements** and parameters, if any, in its facility protocol. No adaptive model or writing document may choose the live exposure schedule or waveform.

## 2. Pre-acquisition work: a day-before and trial-start checklist

**At the actual previous approved observation day:**
1. Retrieve source/lot, care and genuine instrument status. Ensure all prior abnormal physical measurements and source exceptions are resolved by qualified staff or place the study on HOLD.
2. Record a valid *predeclared baseline* using the approved instrument/method. This is task performance, not merely mean firing rate. Record response class distribution, valid/invalid data count, source and reader digest.
3. Check previous performance and qualified care/maturation reference. If the decoder, signal source or physical prep changed since baseline, record NONCOMPARABLE and require a reviewed baseline bridge or reset of the study plan.
4. Freeze the actual task, evaluator, source splits and comparator identities in a signed record; no post-hoc target selection.

**At each approved study session:**
5. Before training, confirm actual session belongs to the preapproved preparation and task; care and source are valid and no safety HOLD is active.
6. For every actual event, retain a true delivered input receipt (not just planned instruction), timestamp, source/recipient channel, source/evaluator version, assigned outcome, response, validity mask and observer role. Never fill a missing physical response using generated Star dialogue.
7. Segregate real task feedback and answer labels from the sealed evaluator. Essential physical care never depends on correct responses.
8. During immediate evaluation, use the locked reader and controlled test split. Save per-class confusion matrix, chance baseline and actual invalid-trial counts.
9. End the session with an immutable record of input/feedback availability and its shutdown; future delayed probes must disclose any intervening teaching or support change.

## 3. Later retention assessment

At the **predeclared later evaluation point** (which could be after an approved rest/observation interval, not automatically the next calendar day):

1. Verify *same source/prep, valid care, current instrument/clock and unchanged or scientifically bridged reader*.
2. Confirm answer-bearing training/teacher/replay caches are absent from the actual test route. Report any required support inputs separately.
3. Deliver only the **authorized test conditions** and record their actual physical delivery/source. If delivery cannot be verified, the trial is INVALID, not a failed learner.
4. Run the fixed independent evaluator, store each class and uncertainty, and test source- and stage-matched controls.
5. Repeat earlier protected tasks only if those tasks and the care/instrument conditions were appropriately predeclared.
6. Compare immediate versus delayed score with the proper metric and uncertainty, and record whether earlier result may have been access/decoder-related.
7. Link any candidate spinal Root hypothesis to *separately qualified* WO-03 anatomical/functional evidence; do not localize a memory to spinal tissue from task success alone.

## 4. Quantitative results and decision rules

For a three-class X/Y/HOLD task, report class-specific valid trial counts and recall:

\[
M=\frac{1}{3}\sum_{c\in\{X,Y,HOLD\}}\frac{TP_c}{n_c}.
\]

A class with no valid trials makes the macro recall undefined; a high all-HOLD rate is not Understanding. Report raw counts, invalid decisions, calibration, preparation-level replicates and uncertainty. A proposed retention ratio is:

\[
R=\frac{M_{delayed}-M_{chance}}{M_{immediate}-M_{chance}}.
\]

When immediate advantage over chance is insufficient, R is **undefined**; it may also be negative or exceed 1. A stable digital index is not equivalent to a retained neural engram.

| Result condition | Classification | Human follow-up |
|---|---|---|
| source/instrument/care invalid | HOLD_OR_INVALID | qualify source and method; preserve raw failed data |
| present only with active teacher | ASSISTED_USE | independent recipient-only test needed |
| immediate gain but delayed data not taken | ACQUISITION_CANDIDATE; RETENTION_UNTESTED | do not promote a long-term Root |
| delayed performance no better than matched baseline/control | NO_RETENTION_DETECTED_IN_TESTED_CONDITION | report negative result/horizon honestly |
| delayed selective effect with matched source/controls and locked reader | BOUNDED_RETENTION_CANDIDATE | review prep-level statistics and exclusions before published claim |
| putative axon/physical carrier with no independent evidence | NO_ANATOMICAL_LOCALIZATION | keep WO-03/Crystal hypothesis unresolved |
| old protected task worsens or source health changed | PROTECTED_TASK_REVIEW | investigate interference, access and care before any new learning |
| actual source/electronic codebook changed | NONCOMPARABLE / ACCESS_CONFOUNDED | do not call memory loss/creation |

**The PI chooses scientific significance or effect criterion before real results**, with a statistician and pilot-based power rationale. These do not come from the program name or from a generic 95% score.

## 5. Required trial and task-level records

Every observed task event should contain: preparation/source/lot, independent biological unit, event_id, task and split/label provenance, actual input/cue delivery receipt, recipient and digital reader/version hash, assigned output class, raw channel response, invalid/artifact flags, actual care state, feedback-present boolean, UTC and instrument timestamps, operator, raw asset hash, evaluator signature and stage (baseline/teaching/immediate/delayed).

Daily summaries need: actual valid/invalid counts, class-specific metrics, open anomalies, source drift, care events, prior protected task deltas, and source/reader comparability to the last valid session. The new digital data ledger should **prevent an unexecuted planned trial from being recorded as an observed learning outcome**.

## 6. Distinguish real digital capability from biological evidence

The public code currently supports source-bound lexical MemoryBank retrieval, digital CrystalPack cloning/replay, and selected RootAssessment metrics. Those processes can be thoroughly software-tested **now** with synthetic events and independently split data. They do **not** prove a living learner was trained. A full independent digital held-out capability test is a future software deliverable, distinct from laboratory evaluation.

A physical "Crystal" or encoded peptide carrying a skill requires *different* carrier, reader and recipient-only evidence; nothing in this work order assumes such a process can be performed.

## 7. Daily research handoff and incident paths

A valid session produces a signed outcome: QUALIFIED_BOUNDED_RESULT, NO_RETENTION_DETECTED, ACQUISITION_ONLY, ASSISTED_USE, ACCESS_CONFOUNDED, INVALID, HOLD or UNRESOLVED, with actual preparation, time horizon, source dataset and reviewer.

A missed care receipt, calibration drift, hidden teacher path or source mismatch blocks a claim and leads to qualified follow-up. It must not lead to automatic medium adjustments, growth induction, genetic editing or more intense stimulation.

**Scientific precedent:** [2026 organoid feedback study](https://pubmed.ncbi.nlm.nih.gov/41720084/) found bounded task improvement that did not survive its 45-minute rest comparison. This is a valuable reason to predeclare delayed tests; it is not a claim that Animon already has long-term memory.

**Exit:** only a scoped, source-linked, independently reviewed actual result (or negative result) may be promoted. Review any WO-03 physical participation separately. [Lab execution standard](ANIMON_LAB_OPERATIONS_v3.3.md) · [WO-03](WO-03-axon-and-spinal-root-evidence.md).

© 2026 We Develop Software LLC. Patent excluded.
