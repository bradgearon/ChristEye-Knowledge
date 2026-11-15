# P02B-02 · Board topology and instrument interfaces — Scientific and technical companion

**Purpose:** distinguish justified inference, engineering selection and future biological research for [design P02B-02](P02B-02_Board_Topology_and_Instrument_Interfaces_Design.md).

## Measurable question

Can the declared subsystem perform its specific function and provide independently interpretable data **without** relying on hidden information, unchecked physical changes or overstated biological claims?

**Candidate mechanism or limitation:** An electrode site is not a sampled neuron. An analog front end does not become a metabolite sensor without a selective transducer and reference assay.

## Alternative explanations and negative cases

ground loop; unqualified OEM port; overcurrent; unexpected host backfeed; false assumption that wireless data can carry raw MEA streams. Model nonacquisition, access impairment, instrumentation failure and physiological support changes as distinct candidate explanations when relevant.

## Evidence program

1. Verify source and unit validity with independent reference data.
2. Freeze relevant analysis/decoder configurations before held-out evaluation.
3. Compare against a simple conventional/digital-only baseline with matched information.
4. Introduce one declared change and measure the entire affected outcome vector, including resource and protected-task cost.
5. Report negative or unresolved outcomes without promoting a hypothesis to finding.

**Concrete acceptance checks:** USB disconnect; pin identity mismatch; power-on-state verification; EMI/ground and supplier approval; redundant inhibit review.

## Biological and hardware qualification

No mains, electrode stimulator or incubator-control schematic released from this exploratory topology. Nothing in this document substitutes for manufacturer approval, a controlled laboratory preparation, a tissue handling SOP, institutional oversight or independent physical safety review.

## Provenance and attribution

The issue originated in the Anaiero engineering discussion. Specific ordinary evidence and source scopes are recorded in CE-DEV-001 v0.4 sections 5–6, 26–29; MaxOne OEM and development-board documentation. The six [scientific text editions](../research.md) preserve historical research synthesis with its own caveats. Simulation results are not living-system measurements.

## Handoff

Before design release, name responsible human owner, test environment, device/source version, signed acceptance target, excluded capabilities, unresolved unknowns and next review date. Preserve all configuration digests and rejected candidate versions.

*WDS research property · © 2026 We Develop Software LLC · [License](../../LICENSE.md)*
