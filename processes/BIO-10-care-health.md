# BIO-10 · Daily care, nutritional requirements, abnormal state and health surveillance

**Authority:** cell-production lead and qualified OEM/incubator procedures. **Software:** read-only monitoring, service plan, review notification and audit.

## Care is independent of teaching success

A learner producing a "wrong" answer must **never** lose necessary Milk or have its medium altered automatically. The same care requirement applies during training, rest, instrument outages, software upgrades and task failure. If a module cannot be safely maintained when Lotus is off, its care architecture is not qualified.

## What a living preparation requires at a high level

Qualified complete culture medium and environmental support ordinarily provide aqueous transport, osmotic/ionic balance, buffer/gas exchange, carbon/energy sources such as glucose/pyruvate, amino acid and lipid/vitamin constituents and defined supplement components. The correct formulation and schedule depend on actual lineage, differentiation stage, organoid geometry, surface/exchange mechanism, supplier instructions and laboratory validation. A peptide supplement is not a guaranteed learning enhancer.

| Observation | Reporter and interpretation |
|---|---|
| Service due/complete | signed SOP version, actual physical receipt, responsible qualified human |
| Medium/batch | composition identity, supplier lot and expiry, release status |
| Environmental | temperature/gas/pH/osmolality where qualified and located; uptime and sensor age |
| Metabolic | selected source/recipient glucose and lactate with time/volume/recovery accounting |
| Biological identity | regional cell markers and growth/maturity evidence |
| Network function | independent electrode/readout/axon validity and task tests |
| Contamination/disease | identity/sterility/quarantine outcomes under approved biosafety policy |
| Fault | stale sensor, lost telemetry, service mismatch, qualified OEM care failure, tissue-risk alert |

## Anti-disease scope

"Anti-disease" begins as **surveillance and containment notification**, not a self-programming immune or treatment organoid. No pathogen challenge, disease agent propagation or self-directed molecular treatment is part of this design. A positive alert means qualified human assessment and separately approved laboratory disposition.

## Proposed scheduling states

~~~text
SERVICE_PLAN_REGISTERED
    -> MATERIALS_AND_SOP_CHECK
    -> DUE_FOR_OPERATOR
    -> PHYSICAL_SERVICE_RECEIPT
    -> INDEPENDENT_QUALIFICATION
    -> MONITOR_AND_ARCHIVE
  any invalidity -> HOLD_TASKS + HUMAN_REVIEW
~~~

The project software may remind, correlate, record, and inspect. It **cannot** issue a pump command, select a supplement or restart stimulation on host reconnection. An adverse environmental state should not be conflated with "forgetting" or a Root death.

## Food and peptide exception card

A change to the complete medium, nutrient concentration, growth signal, receptor ligand, peptide form or any structural intervention always requires: specific qualified preparation; ordinary molecular form and target; source-paper applicability; analytical measurement; independent care adequacy; competing explanations; human PI/cell lead signoff; predeclared physical/functional harms; actual qualified service receipt; and a stop or rescue plan. Missing a source, dose authority or viable control means **NOT APPROVED**. No generic daily formula, stimulation count or sample volume is invented.

[ISSCR standards](https://www.isscr.org/resources/guidelines-for-stem-cell-research) · [BIO-04 ChemiBus/SustiBus](BIO-04-sustibus-chemibus.md) · [BIO-05 measuring](BIO-05-measurements.md).
