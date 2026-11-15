---
title: "P02-05 Reference Task Education and Frozen Reader Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02_Cell_Free_Electronics_and_WindBus/docs/design/P02-05_Reference_Task_Education_and_Frozen_Reader_Design.docx`  
> **SHA-256 (original DOCX):** `01d59d852503066762d468cf7e8153320e83475d18a316bb3fcd09632434b9b0`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Revision:** 1.0 \| **Owner:** learning/evaluation lead \| **Runtime:** learner.py, scenario.py \| **Mode:** synthetic teaching only

# 1. Why this task comes first

The P01/v0.4 reference uses two contexts and two ordered symbols to require the learner to attend to **order, context and repeated-symbol control**. It is intentionally simple enough for an exact digital oracle, whose role is to detect mistakes in the experimental apparatus. C's acquisition and retention must be demonstrated independently of how well an external rule engine could solve the same tiny truth table. The P02 simulated learner is a test stub: it can acquire eight separate associations after repeated feedback, but it has no neural substrate, semantics, spontaneous learning or inferred biological memory. \[I01\]

# 2. Frozen task oracle and conditions

| **Context**     | **Sequence** | **Expected class** | **Important contrast**                         |
|-----------------|--------------|--------------------|------------------------------------------------|
| C0              | AB           | X                  | Order AB in context C0                         |
| C0              | BA           | Y                  | Same symbols, reversed order                   |
| C1              | AB           | Y                  | Context reverses AB outcome                    |
| C1              | BA           | X                  | Context reverses BA outcome                    |
| C0/C1           | AA           | HOLD               | Repeated-symbol rejection                      |
| C0/C1           | BB           | HOLD               | Repeated-symbol rejection                      |
| Missing/invalid | Any          | UNRESOLVED         | Instrument status, not a biological abstention |

There are eight valid context-sequence pairs: X occurs twice, Y twice, HOLD four times. Always choosing HOLD yields 50% **raw accuracy** but only one-third **macro-averaged recall**. Report all eight cells, full confusion matrix, class prevalence, valid-trial availability, and reader validity. INVALID is a data-acquisition/status outcome, never credited as correct HOLD.

# 3. Teaching state and feedback sequence

The session sequence is OFFLINE → READY → BASELINE → READY → TRAINING → INTERVENING → EVALUATION. In BASELINE, E knows the reference label but releases none. In TRAINING, H submits a cue to C, C produces a frozen-reader response, E closes the response window and only then releases the declared feedback to C. The reference SimulatedSoil stores a label for an observed key and counts exposures; after two lessons on a key it returns the stored class. This is a deterministic \*functional fixture\*, not STDP, a biological Honey model, or an estimate of real learning time. INTERVENING disables teaching; EVALUATION sends new trial identifiers through the same task codebook, with readout frozen and feedback absent.

# 4. Separation of learner, reader and evaluator

TaskCue.features() contains trial identity, context and ordered symbols **without \`expected_class\`**. E calls task_oracle() privately. FixedReader maps the synthetic learner's emitted X/Y/HOLD to a status and carries a reproducible digest; it does not fit on evaluation data. TrialOutcome records split, prediction, validity, E's ground truth, reader digest, feedback released and learner state digest. Re-running the same patterns with new IDs only checks stable acquisition of those exact relationships; it is **not** a test of broad generalization or independence of a biological engram. Those stronger claims need later new-context and delayed-cohort designs. \[I01, I02\]

# 5. Practical software sequence for this release

1.  Generate all eight valid combinations with unique trial IDs; preserve a campaign-specific source and seed.

2.  Establish BASELINE with no feedback and record macro recall, always-HOLD shortcut and per-class outcomes.

3.  Use the independent L permit before each simulated training event; withhold cue delivery if telemetry fails.

4.  Train on balanced conditions; E releases only TRAINING labels after each closed response.

5.  Enter INTERVENING with no answer-bearing input; record actual internal simulation changes separately from readout changes.

6.  Freeze the candidate's reader and software versions, then evaluate on reserved trial IDs without feedback.

7.  Report absolute performance, scheduled vs valid opportunities, label leakage audit, and protected earlier tasks.

8.  Archive every negative result and fault as well as positive demonstration; do not tune after viewing sealed confirmation without creating a new version.

# 6. Evaluation metrics and evidence boundaries

MAR = (recall_X + recall_Y + recall_HOLD)/3 is the primary P02 reference score. Availability is correct-and-on-time / scheduled opportunities, so invalid, missing and late responses stay in its denominator. An optional delayed retention ratio R_U = (P_delay − P_chance) / (P_immediate − P_chance) is not reliable if immediate performance is near chance; it can exceed 1 or be negative. Report absolute scores and uncertainty in any living experiment. P02 has no true biological delayed retention: repeated simulation responses are a software-regression result only. \[I02\]

# 7. Core tests and acceptance

test_reference_oracle, test_baseline_then_train_then_evaluate, test_invalid_cue_cannot_be_hold, test_macro_requires_all_classes, test_no_evaluation_feedback and the full demo verify the core teaching contract. The initial P02 demonstration shows macro recall rising from 1/3 to 1 in a dictionary-like mock. That result has no scientific bearing on the achievable success of living organoids. The 0.80 acquisition and 24-hour/7-day retained performance values in CE-DEV-001 v0.4 are proposed **future** engineering requirements, not achieved performance. \[I01\]

# 8. P03/P04 handoff

P03 must qualify actual physical response separation, timing, and artifact recovery before this codebook can be associated with a living preparation. P04 must pre-register experimental units, acquisition vs yoked comparisons, frozen readout, and delayed tests after an independent preparation-level power analysis. Software-only, untrained, noncontingent, shuffled and relevant shortcut baselines must be treated as separate comparison conditions. There are no proposed live stimulation amplitudes, frequencies or durations in P02.

# 9. References

\[I01\] ChristEye Physical Learning Device Design v0.4 §§14–16, 23–24. \[I02\] ChristEye Formal Models and Reconciliation, living computer equation audit. \[E01\] ISSCR Guidelines, https://www.isscr.org/resources/guidelines-for-stem-cell-research.

