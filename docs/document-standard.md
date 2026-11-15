# Design and companion document standard

> **ChristEye Anaiero · 15 November 2026 · We Develop Software LLC**  
> **Status:** Research design · unvalidated biological proposal · WDS Public Research Reference License.  
> [Documentation Home](index.md) · [Full document library](full-document-library.md) · [Phase P01 original](../phases/P01/README.md)

The Phase 1 source-defined authoring contract. All future designs must report explicit measured data, source identity, governing authority, and unresolved unknowns; a module title or conceptual analogy is insufficient.

## 20 required specification sections

| § | Section | Mandatory technical material |
|---|---|---|
| 01 | **Document authority** | ID, version, evidence F/X/H/D/A, authors, references, approved build revision, open unknowns. |
| 02 | **Functional contract** | User need, measurable outputs, non-goals, digital/live/simulated mode. |
| 03 | **System boundaries** | Source, receiver, owner, permitted routes, safety interlock, separation/replay plan. |
| 04 | **Bill of materials** | Vendor, part, compatible device, alternatives, sourcing, lot traceability, quantity and unit. |
| 05 | **Cell census** | Tissue identity, input viable count, subtype fractions, area/volume, final observed census, replication unit. |
| 06 | **Molecular inventory** | Chemical identity, process role, stage, compartment, receptor, status, lot, assay and SOP reference. |
| 07 | **Anatomical/structural inventory** | Organoid/planar geometry, ECM, neurites, synapses, glial contacts and measured connectivity. |
| 08 | **Developmental stages** | Production source, maturation, acceptance, training, retention, renewal; gate between stages. |
| 09 | **Nourishment & exchange** | Separate support route, supply identity, consumption/waste, service schedule, alarms and sensor freshness. |
| 10 | **Electronic interfaces** | Electrodes, sampling and stimulus profiles, clocks, ground, readout quality, vendor permit and shutdown. |
| 11 | **Information protocol** | Codebook, WindEvent, training target, decoder state, state/label isolation, information budget. |
| 12 | **Learning program** | Baseline; teaching; probes; rest/intervening activity; delayed evaluation; curriculum progression. |
| 13 | **Data and software** | JSON schemas, calibration versions, signature and hash, provenance, reproducibility. |
| 14 | **Diagnostics** | Structural, electrophysiological, optical, health and task performance measures; unknown states. |
| 15 | **Controls and statistics** | Independent preparations, negative controls, power/precision, preregistration, sealed holdout. |
| 16 | **Failure and hazards** | Potential faults, observable signatures, response authority, no silent auto-rescue. |
| 17 | **Resource budgets** | Cell supply, plates/chips, storage bandwidth, imaging, operator time, fluid service, energy. |
| 18 | **Verification and release** | Bench/cell-free tests, living gates, evidence class, acceptance decisions, nonclaims. |
| 19 | **Traceability** | Cited primary data, supporting ChristEye documents, deviations and superseded claims. |
| 20 | **Change control** | Exact changed component, null dependencies, regression plan, independent approval and rollback limitations. |

## Record metadata (machine validated)

`document_id`, `module_id`, `revision`, `applicable_preparation`, `status`, `source_refs`, `controlled_unknowns`, `owner`, `gate`. Every field must be present and testable. Declare source namespaces: external primary, supplier, prior Anaiero specification, measured experiment.

## Companion document: evidence rule

A companion is not a summary; it identifies which statements are published findings (F), cross-study syntheses (X), hypotheses (H), engineering choices (D) or analogies (A). For each claim, give the measured preparation, alternative mechanism, control test, allowed conclusion and explicit failure condition. Include negative results.

## Example traceable requirement

```yaml
document_id: CE-AN-P01-01
module_id: C
revision: P01.0
applicable_preparation: Axion planar vendor reference
status: supplier_reference_only
source_refs: [R01, R13]
controlled_unknowns: [U01, U02, U04, U05, U06]
owner: qualified_culture_lead
gate: G1
```

The complete source document is [Program phases and document standard](../phases/P01/docs/00_Program_Phases_and_Document_Standard.md).
