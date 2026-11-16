---
title: "WO-02 — Daily Animon care, ChemiBus, SustiBus and scientific drift"
date: "2026-11-16"
status: "Specific monitoring/QC order; physical care requires facility SOP"
owner: "Independent culture-care, molecular science, and lab QA leads"
---

# WO-02 — Daily care and chemistry: shift work order

**Purpose:** assign actual daily accountability for Animon physiological support, separate independent care from task feedback, and ensure every chemical claim is traceable to a real specimen and assay. The source-approved laboratory SOP is the only authority for physical culture maintenance, medium changes, growth factors, peptides, or emergency care.

[Lab execution standard](ANIMON_LAB_OPERATIONS_v3.3.md) · [CE-A17](../specifications/v3.2/design/CE-A17_Animon_Care_Design.md) · [BIO-04](../processes/BIO-04-sustibus-chemibus.md) · [BIO-10](../processes/BIO-10-care-health.md).

### Reference from the original P01 care plan

[P01 Daily Care Quality Gates](../phases/P01/docs/04_Daily_Care_Quality_Gates_and_Assays.md) already distinguishes **daily inspection** from **supplier-qualified service intervals**, and names source-specific observations of medium identity/volume/color, environment, optical status, chemical assays and actual service receipts. Use those source-linked instrument/medium classes when preparing the lab-specific table below; do **not** reuse the Axion/BrainXell planar maintenance schedule for a 3D Soil or a different MEA source.

## 1. Before observing a new preparation

| Item | Document the actual source | Release condition |
|---|---|---|
| Qualified living model | source, age/stage, actual prep and SOP version | cell/care owner confirms source and SOP |
| Maintenance reference | lab-approved formulation, medium lot, environment plan, staff and service timing | no guessed recipe or software-selected intervals |
| Instrument | valid calibration, unit, real sample position, clock and range | uncalibrated reading cannot support health inference |
| Acceptance limits | stage-specific absolute critical and warning bounds from approved SOP | missing bounds = UNKNOWN, not a new safety threshold |
| Baseline | stable source/stage/method-matched reference or authorized control | unreviewed daily history is not automatically normal |
| Safety | qualified independent service fallback and emergency escalation contact | task failures cannot interrupt essential care |

Possible observations under the actual SOP include source-specific environmental conditions, pH/osmolality, actual service receipts, and calibrated metabolic analytes such as glucose/lactate where truly sampled. These are *categories*, not a protocol-specific list of nutrient doses.

## 2. Daily sequence and explicit failure action

| Step | Trained scientist's task | Output field(s) | If condition fails |
|---|---|---|---|
| CARE-01 | retrieve real source lot, prep and yesterday's reviewed deviation/shift report | prep_id, source_lot_id, current restrictions | missing source → HOLD_NEW_EXPERIMENT |
| CARE-02 | confirm actual qualified care service, operator and physical receipt | care_plan_ref, SOP revision, actual service receipt, current medium lot | planned service without receipt → escalate care review |
| CARE-03 | check all data sources have valid calibration/units/time origin | sensor ID, method, sample position, calibration | INVALID_MEASUREMENT; no causal interpretation |
| CARE-04 | obtain approved physical environment/health observations | value, unit, instrument, time, actual condition | beyond qualified SOP critical bounds → care-lead emergency SOP and suspend new study |
| CARE-05 | collect only **authorized scheduled** chemistry specimens under local SOP | sample ID, analyte/form, location, assay, calibration, raw hash | no sample/invalid assay → NOT_MEASURED |
| CARE-06 | compare all valid comparable readings with yesterday and qualified baseline | delta_abs, delta_pct, baseline and changed conditions | REVIEW_TREND / NONCOMPARABLE / UNRESOLVED |
| CARE-07 | ask whether new learning studies remain authorized given health/care | human reviewer, study hold, old task context | hold new teaching, never withhold care |
| CARE-08 | hand off actual actions, incidents and next approved observations | signed shift record, open deviations, next owner | unresolved action remains open |

**SustiBus records actual support; it is not a pump controller.** **ChemiBus records chemistry; it is not a peptide dispense instruction.** **Honey is evaluation feedback; it has no permission to stop Milk.**

## 3. Chemistry observations — exact scientific meaning

For every analyte, require the **actual specimen**, sampling compartment (fresh/conditioned medium, tissue or other qualified matrix), time from real service, method, unit, molecular form where relevant, quantitation range, calibration/lot and raw data. A single concentration does not establish biochemical flux or knowledge acquisition.

| Candidate data | Necessary distinction |
|---|---|
| glucose/lactate concentrations | production vs uptake, exchange/dilution, growth stage, matrix and sample handling |
| oxygen/pH/osmolality | actual validated sensor or specimen context; cannot fill from a guessed environmental default |
| glial markers and transporters | ordinary cell identity/support hypotheses; not a semantic knowledge codec |
| peptides/receptors | sampled form, receiver and specific scientific hypothesis; no automatic "add peptide" rule |
| cell viability or death | measured cell/source context, assay artifacts and comparison, not inferred from task failure |

Use [ChemiBus schema](../schemas/biology/chemibus_observation.schema.json), [NOT_MEASURED fixture](../fixtures/v3.2/chemibus_unmeasured.example.json) and [SustiBus schema](../schemas/biology/sustibus_care_status.schema.json). A fixture with null observed_value is not a zero chemical concentration or a measured sample.

## 4. When values change from the previous day

1. Verify comparability: same preparation/stage, sampled compartment, valid clock/unit, instrument and assay version, source medium lot and known intervention status.
2. Compute current minus previous valid comparable observation. If prior is missing/zero, percentage change is **undefined**.
3. Also compare to a qualified baseline, *not merely one previous day*; a developing neural culture may have an expected trend.
4. Interpret bounds from **the approved SOP**, not a universal Animon value. The read-only QA script reports status but never performs an action.
5. If a critical limit or unresolved care receipt exists: HOLD_NEW_EXPERIMENT and notify the qualified care lead. Its local contingency SOP governs physical care.
6. If a noncritical warning band is crossed: REVIEW_TREND and inspect calibration, source, handling, source line, exchange, healthy reference and earlier data.
7. If method, lot, stage, instrument or compartment changed: NONCOMPARABLE until source/assay bridging evidence is approved.
8. Keep original raw data, method, signed reviewer and decision; do not delete negative or invalid observations.

## 5. Food, peptide and growth change card

A real **ChangeProposal** needs: current qualified source/SOP, real care and analyte data, target physical component, specific proposed material/form and scientific reason, relevant alternatives, health and older skill protections, comparison and source/batch plan, independent PI/cell-care/safety approval, actual source-specific physical SOP, postcondition observation and signature.

**Branching logic:**
- LOW_ROOT_SCORE alone → digital/measurement investigation, never a chemical adjustment.
- INVALID_SAMPLE → assay investigation, no medium modification.
- CRITICAL_LAB_SOP deviation → authorized care lead follows its *existing* contingency instructions; no new experimental actuation.
- ACCEPTED_NEW_HYPOTHESIS → independent facility protocol/scientific review before any physical step.
- UNAPPROVED change → no implementation; preserve independent care.

## 6. Negative and fault cases

| Fault | Required result |
|---|---|
| missing physical care receipt | flag UNKNOWN and escalate; scheduled event is not performed event |
| expired environmental sensor | measurement invalidated; actual care and safety oversight continue |
| repeated sample with different assay | record noncomparability unless documented bridge exists |
| sample from wrong source lot | HOLD scientific attribution and investigate identity |
| electrical task reward attempts to direct feeding | HARD_DENY and audit |
| apparent lactate increase after medium change | source/service-confounded; do not call new Root |
| prior validated task declines | scientific HOLD and independent health/source/reader review |
| spurious synthetic ChemiBus data | cannot be promoted to observed physiology |

**Exit:** a signed daily source-qualified care, chemistry, comparison and deviation record. If incomplete, the result is HOLD or UNRESOLVED. The actual lab owns care operations and their SOPs. Next: [WO-03 axons](WO-03-axon-and-spinal-root-evidence.md).

**References:** [ISSCR model systems](https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems); [ISSCR reporting](https://www.isscr.org/basic-research-standards/reporting).

© 2026 We Develop Software LLC. Patent excluded.
