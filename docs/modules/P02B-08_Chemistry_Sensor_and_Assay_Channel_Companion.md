# P02B-08 · Chemistry sensor and assay channel — Scientific and technical companion

**Purpose:** distinguish justified inference, engineering selection and future biological research for [design P02B-08](P02B-08_Chemistry_Sensor_and_Assay_Channel_Design.md).

## Measurable question

Can the declared subsystem perform its specific function and provide independently interpretable data **without** relying on hidden information, unchecked physical changes or overstated biological claims?

**Candidate mechanism or limitation:** AD5940 is general sensing electronics, not a lactate-specific sensor. Nutrient concentration is not a direct proxy for stored Knowledge.

## Alternative explanations and negative cases

sensor fouling; reagent cross-response; sample dilution; calibration drift; electrode artifact; surface adsorption misread as uptake. Model nonacquisition, access impairment, instrumentation failure and physiological support changes as distinct candidate explanations when relevant.

## Evidence program

1. Verify source and unit validity with independent reference data.
2. Freeze relevant analysis/decoder configurations before held-out evaluation.
3. Compare against a simple conventional/digital-only baseline with matched information.
4. Introduce one declared change and measure the entire affected outcome vector, including resource and protected-task cost.
5. Report negative or unresolved outcomes without promoting a hypothesis to finding.

**Concrete acceptance checks:** blank specificity; repeatability; recovery and stability; matrix mismatch; cross-lot comparability; excluded samples report.

## Biological and hardware qualification

Assay method and sampling volume require laboratory release; no chemical control signals permitted. Nothing in this document substitutes for manufacturer approval, a controlled laboratory preparation, a tissue handling SOP, institutional oversight or independent physical safety review.

## Provenance and attribution

The issue originated in the Anaiero engineering discussion. Specific ordinary evidence and source scopes are recorded in CE-DEV-001 v0.4 chemical inventory; AD5940 OEM datasheet; biochemical assay validation practice. The six [scientific text editions](../research.md) preserve historical research synthesis with its own caveats. Simulation results are not living-system measurements.

## Handoff

Before design release, name responsible human owner, test environment, device/source version, signed acceptance target, excluded capabilities, unresolved unknowns and next review date. Preserve all configuration digests and rejected candidate versions.

*WDS research property · © 2026 We Develop Software LLC · [License](../../LICENSE.md)*
