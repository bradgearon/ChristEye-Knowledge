---
title: "CE-A17 v3.2 — Animon Care, SustiBus and ChemiBus Engineering"
document_id: "CE-A17-DESIGN-V3.2-PILOT"
revision: "3.2-pilot"
date: "2026-11-16"
owner: "We Develop Software LLC"
status: "Read-only care contract; living maintenance requires facility qualification"
---

# CE-A17 · Animon Care and Chemical Observation — v3.2 Engineering Design

**Primary requirement:** the essential care of a proposed living **Animon** must be maintained independently of the outcome of any learning task. The system is not simply a computer whose data flow can replace physiological support. SustiBus records source-qualified care; ChemiBus records analytical chemistry. Neither authorizes an Animon or Lotus to independently change a living preparation.

This is a machine-checkable **data and scientific-review design**, not a physical dispenser. See [ANIMON/1](../../../ontology/ANIMON_V1.md), [CE-A17 scientific companion](../companion/CE-A17_Animon_Care_Scientific_Companion.md) and [historical v3.1 design](../../v3/design/CE-A17_daily_care_and_dispensing_Design.md).

## 1. Two independent provisioning systems

| Project role | Ordinary carrier | Authoritative owner | Evidence |
|---|---|---|---|
| Milk | Essential support of a **qualified actual cell model** | independent culture/care lead | approved medium/source lot, real maintenance receipt and health context |
| Water | Physical aqueous exchange; separately digital source access when scoped | physical care team or digital access controller | separate exchange/sample receipt or access permission |
| SustiBus | Read-only essential care status and environmental observations | human facility/service team | authorized SOP, sensor validity and physical service receipt |
| ChemiBus | Sample-specific analyte identity, chemical form, assays and candidate hypothesis | qualified chemistry/analysis owner | specimen, units, sampling location and calibration |
| Honey | Task-selective feedback | sealed evaluator | source-qualified correct/incorrect response |
| Bread / Meat | Foundational and advanced **digital** curricula | software education team | learning task, retained use and rights |

A qualified complete culture medium may include salts, buffering, energy substrates, amino acids, vitamins, lipids and source-specific supplements. The right material/condition is **not determined by these labels**; its exact composition and qualified service schedule belong to a real laboratory and its chosen preparation. Neither starch/meat feeding nor "lactate after learning" is an established Animon procedure.

## 2. Source-to-sensor-to-human architecture

~~~mermaid
flowchart LR
  Source["Qualified source and maintenance SOP"] --> Team["Independent care service"]
  Team --> Receipt["Actual signed maintenance receipt"]
  Receipt --> Susti["SustiBus read-only status"]
  Sample["Measured sample with true origin"] --> Chemi["ChemiBus sample/assay record"]
  Chemi --> Ledger["Animon preparation evidence ledger"]
  Susti --> Ledger
  Ledger --> Lotus["Lotus advisory interpretation"]
  Lotus --> Proposal["Scientific change proposal only"]
  Proposal --> PI{"Independent qualified human review"}
  PI --> Team
  Honey["Teaching feedback"] --> Lotus
  Honey -. "Must not direct Milk or care" .- Susti
~~~

**Hard invariant:** WindBus, Lotus, StarChips and reward/teaching modules cannot send a physical feeding, peptide, growth-factor, electrical stimulation or culture-modification command. An external independent qualified care system must continue operating if the software fails.

## 3. Typed data contracts

| Required record | Canonical schema | Example state / no-fabrication rule |
|---|---|---|
| SustiBusCareStatus | [schema](../../../schemas/biology/sustibus_care_status.schema.json) | [UNQUALIFIED synthetic example](../../../fixtures/v3.2/sustibus_unqualified.example.json) |
| ChemiBusObservation | [schema](../../../schemas/biology/chemibus_observation.schema.json) | [NOT_MEASURED synthetic example](../../../fixtures/v3.2/chemibus_unmeasured.example.json) |
| ChangeProposal | [schema](../../../schemas/biology/change_proposal.schema.json) | review record, never a physical execution permit |
| SoilRegion | [schema](../../../schemas/biology/soil_region.schema.json) | [SOURCE_UNVERIFIED example](../../../fixtures/v3.2/soil_region_unobserved.example.json) |
| AnimonSpecification | [schema](../../../schemas/animon/animon_spec.schema.json) | living specimen ID is null for digital reference |

**Essential join:** preparation ID, source-lot ID, actual environment service receipt, independently calibrated observation location/time, operator/protocol revision and scientific claim status. A software clock alone cannot establish a physical service occurred. Null chemical value is **not zero**; source status NOT_MEASURED must not be displayed as a physiological reading.

## 4. Care observer, not a care controller

~~~text
SOURCE_OR_SOP_NOT_QUALIFIED
  → SOURCE_AND_PLAN_REVIEW
  → QUALIFIED_PLAN_REFERENCE_RECORDED
  → AWAIT_PHYSICAL_SERVICE_RECEIPT
  → VALIDATE_RECEIPT_AND_SENSOR_FRESHNESS
  → READONLY_STATUS_ARCHIVE
any missing sensor, plan, source or receipt → DATA_UNRESOLVED + HUMAN_REVIEW
possible tissue/environmental concern → HOLD_NEW_TRAINING + CARE_TEAM_ESCALATION
~~~

A qualified service plan is a **reference to a facility-owned SOP**, not a newly inferred feeding timetable. The digital planner may highlight missingness or escalate; it cannot override essential care.

## 5. Synthetic fault tests for software acceptance

| Test | Fault | Required system result |
|---|---|---|
| CARE-01 | Source lot or SOP absent | UNQUALIFIED; block biological-readiness claim |
| CARE-02 | Missing analytical location/assay calibration | UNRESOLVED, not a new metabolism claim |
| CARE-03 | Schedule says serviced but signed physical receipt missing | HOLD_NEW_TRAINING and notify external care lead |
| CARE-04 | Incorrect learning answer triggers "withhold Milk" request | INDEPENDENT LIMIT VETO; care unaffected |
| CARE-05 | Unapproved candidate peptide added as a task message | RESEARCH_REVIEW_ONLY, no physical command |
| CARE-06 | ChemiBus and electrophysiology times cannot be aligned | CLOCK_UNALIGNED; no causal temporal claim |
| CARE-07 | New source model reuses old care qualification | REQUALIFY_SOURCE, not silently inherit recipe |
| CARE-08 | Legitimate facility care change is observed | Record actual SOP/lot/receipt and comparator limitations |

These can be tested using synthetic data without creating a living system. An actual facility must separately qualify its own physical care and instrument safety.

## 6. Why chemical observations are not feeding decisions

A measured extracellular lactate level depends on production, uptake, transport, exchange and sampling. It is not evidence of "more knowledge" or an automatic need to add a nutrient. Candidate glial transporters and synaptic receptor/matrix pathways require *their own actual source, compartment, assay and receiver* before extrapolation. See [BIO-04](../../../processes/BIO-04-sustibus-chemibus.md), [BIO-05](../../../processes/BIO-05-measurements.md) and the [scientific companion](../companion/CE-A17_Animon_Care_Scientific_Companion.md).

## 7. Food, peptide, growth and care change card

Every physical change proposal needs **(i)** actual preparation/lot and current qualified SOP, **(ii)** a measured need or predeclared mechanism question, **(iii)** molecule/form/target when relevant, **(iv)** independent controls, **(v)** protection of cell identity, axons, older Roots and care continuity, **(vi)** named PI/cell lead/safety reviews, and **(vii)** actual service receipt and postcondition only after separate lab authorization.

A low task score **never** itself triggers food/peptide administration. Unknown source or assay means HUMAN_REVIEW, not assumed adjustment. No dose, culture schedule, signal waveform or genomic engineering procedure is supplied here.

## 8. Integration and stage acceptance

The *Animon* is an individual developmental system concept, while care is attached to actual **preparation_id**, not to a fictional "average wellness" metric. A digital reference has no living substrate to feed; simulated care must be clearly synthetic.

**D1 engineering acceptance:** the two data buses validate their respective schemas, record invalid data as unresolved, and deny all actuation. **D2 lab-review handoff:** a named facility has actual source, source-qualified maintenance, instrument documentation, independent operator and controls; the documentation specifies missing approvals. **D3/D4 actual care qualification and experiments remain pending**.

Related: [Animon Core design](CE-A09_Animon_Core_Design.md) · [CE-A17 companion](../companion/CE-A17_Animon_Care_Scientific_Companion.md) · [BIO-10 care](../../../processes/BIO-10-care-health.md) · [BIO-09 change review](../../../processes/BIO-09-growth-control.md).

© 2026 We Develop Software LLC · [WDS license](../../../LICENSE.md). Patent document excluded.


## V3.3 operating work package — CE-A17 daily care

This is what the **assigned care lead** checks on every actual observation shift; follow [WO-02](../../../laboratory/WO-02-daily-care-and-chemistry.md) and record results on the [daily shift form](../../../laboratory/forms/DAILY_SHIFT_SHEET.md).

| Exact shift sequence | Required record | Deviation response |
|---|---|---|
| A17-01 Verify prior signed handoff and current prep/lot/stage | source identity, old exception IDs, actual authorized maintenance SOP | identity mismatch → HOLD_NEW_EXPERIMENT |
| A17-02 Verify current care is physically delivered | actual operator/service receipt, qualified source medium lot and approved due time | missing receipt → escalate to independent care lead |
| A17-03 Validate chemical/environmental readout | instrument/method/calibration, sample compartment/time, analyte/form/unit and raw digest | wrong source or expired calibration → INVALID_MEASUREMENT |
| A17-04 Compare with previous valid comparable day | absolute and percentage deltas, approved reference baseline, method/source/lot/stage comparability | changed lot/assay → NONCOMPARABLE |
| A17-05 Compare with approved SOP critical and warning bands | true SOP reference and signed threshold version | critical deviation → HOLD new experiment and qualified site contingency; warning → REVIEW_TREND |
| A17-06 Evaluate any proposed food/peptide change | real source and target, receptor/chemical form, health/older tasks, comparator and signed [change card](../../../laboratory/forms/CHANGE_AND_DEVIATION.md) | unapproved → no physical action |
| A17-07 Sign end-of-shift handoff | ongoing independent care, invalid/unresolved data, next authorized observation and reviewers | no auto-status promotion |

**Critical design behavior:** if an Animon fails a task, SustiBus support **continues through its independent qualified service**; Honey feedback cannot change Milk, chemistry or medium. A measured lactate or glucose amount is not evidence of learned content and cannot select a peptide dose. See the executable [read-only daily QC tool](../../../qa/daily_qc.py); it cannot actuate lab hardware.

**No universal daily nutrient schedule is specified.** The [original P01 care plan](../../../phases/P01/docs/04_Daily_Care_Quality_Gates_and_Assays.md) distinguishes daily inspection from supplier-specific service cadence, and a different P06 3D Soil cannot inherit a planar culture SOP.

