# BIO-04 · Sustibus, Chemibus and five provisions

**Purpose:** separate actual physiological support, measured chemical state, digital curriculum and learning feedback. **Status:** data and governance contract; no live feeder/chemical controller.

## Typed buses

| Bus | Real carrier | Typed messages | Allowed authority |
|---|---|---|---|
| WindBus | digital event stream | task, codebook, teaching, evidence, replay | authorized host (digital) |
| **SustiBus** | care plan and instrument readings | source-qualified medium lot, independent life-support status, due service and physical receipt | care lead / independently qualified service |
| **ChemiBus** | biochemical observations | sample/analyte, molecular form, location, recovery/units/uncertainty, target hypothesis | chemistry expert; proposed interventions only |
| BioBridge | qualified OEM neural interface | physical I/O receipt, stimulus/capture map, inhibit status | independent limit controller |
| Axon observation | image/electrophysiology graph | structural track, function and confidence | independent scientific evaluator |

No ChemiBus packet is itself a peptide; no WindBus example is a culture feed order. Do not make care contingent on whether the learner solves a task.

## Food and educational provision table

| Term | Working technical role | Selected carrier | Qualification |
|---|---|---|---|
| **Milk** | life-sustaining support | qualified complete culture medium, ionic/energy/structural availability | lab SOP, independent viability and actual service receipt |
| **Water** | exchange/access | aqueous medium, transport, circulation/clearance; different digital meaning when explicitly scoped | material balance and sample position |
| **Bread** | introductory Knowledge | source-linked book/lesson primitives and simple exercises | prerequisite, retrieval and early transfer tests |
| **Meat** | integrated advanced competence | specialist problems, tool-use scenarios, multistage curricula | withheld cases, interference and durable performance |
| **Honey** | selective instructional feedback | independently scored task receipt/contingent feedback | matched noncontingent control, sealed answer hygiene |

Candidate culture-medium constituents in published references include water, salt/bicarbonate buffering, glucose, pyruvate, amino acids, lipids, vitamins and supplier supplements. They must be specified by a **particular qualified formulation**. This is not an instruction to feed starch, meat, pills, or a particular "brain food." [P01](../phases/P01/docs/02_Molecules_Structures_and_Development_Companion.md).

## ChemiBus measured data envelope

~~~json
{
  "schema_version": "chemibus.v1",
  "preparation_id": "example-preparation",
  "sample_id": "example-sample",
  "source_lot_id": "source-1",
  "analyte": "lactate",
  "chemical_form": "unresolved",
  "observed_value": null,
  "unit": "mmol/L",
  "sample_location": "not-characterized",
  "time_since_last_service": null,
  "assay_and_calibration_ref": null,
  "data_validity": "NOT_MEASURED",
  "intervention_authority": "NONE"
}
~~~

SustiBus adds service schedule/SOP, operator role, physical source/receipt, freshness and independent continued-care assessment.

## What to do after "learning transfers" occur

A task episode may correlate with glucose use, lactate release or astrocytic calcium. A single extracellular lactate reading cannot identify learning reinforcement: concentration depends on production, uptake, transport, volume and sampling. A rat memory study implicates lactate transporters in a specific model, not an Anaiero substrate setting [B07](../research/evidence/anaiero-biology-2026-11.md). Track chemical observation and task-specific delayed Roots as *parallel channels* before hypothesizing a link.

## Universal food/peptide proposal card

~~~text
proposal_id: UUID; requesting module and data reference
input evidence: analyte assay, time series, viability, culture/supplier qualification, controls
proposed target: named ordinary molecular form and receiver (or source-qualified medium change)
comparison: unchanged conditions, appropriate negative controls and monitoring
protected metrics: independent care, regional identity, axon function, delayed old skills
authority: PI + cell lead + relevant biosafety / ethics approval
outcome: PROPOSED | LAB_REVIEW | REJECTED | UNRESOLVED | APPROVED_IN_SOP
~~~

No concentration, peptide sequence or automated delivery profile is implied by terms Bread, Milk, Honey or Wind. See [BIO-09 growth governance](BIO-09-growth-control.md), [BIO-10 care](BIO-10-care-health.md).
