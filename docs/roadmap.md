# Living roadmap and scientific release gates

The project's phases are evidence-gated, not date-gated. An insertion or revision must preserve the older phase identifiers and decision records.

## Native software development

The native Windows/Linux research runtime has integrated sessions, durable
evidence, isolated digital experiments, supervised scientific providers and
authenticated evidence replication. Its current engineering priorities are:

[CE-SW-38](software/CE-SW-38_compiled_runtime_and_lifecycle.md) implements the
first four architecture batches: compiled SDK/capabilities, canonical indexed
evidence, real VLM and supported SW5/SIM3 execution/change paths, and lifecycle/
retention/quarantine. Payload reclamation and finite quotas are implemented;
historical journal-prefix pruning remains a separate qualified extension.

The next priorities are:

1. **M5:** continuous native evidence mesh delivery using the new commit/index/
   lifecycle services, with bounded fairness, replay and rebootstrap.
2. **M6:** native package/API compatibility, failure recovery, measured workload
   profiles and sustained resource/deployment qualification.

See the [native implementation checkpoint](software/native_implementation_status.md)
for completed capabilities and the
[readiness assessment and development plan](software/native_development_plan.md)
for ratings, evidence, dependencies and completion gates.
[CE-SW-37](software/CE-SW-37_modular_runtime_and_evidence_lifecycle.md) defines
the target architecture and six delivery batches. Scientific input/model and
external-reference qualification remains a separate workstream. Native
software progress does not itself satisfy the scientific or physical phase
gates below.

## Scientific and physical phases

| Phase | Purpose | Current status | Exit evidence |
|---|---|---|---|
| P00 | ontology, scientific claims, responsibilities | Documented | versioned source register |
| P01 | source composition, measurement contracts | Documented | controlled reference cell/material list; unresolved inputs explicit |
| P02 | cell-free WindBus, teaching/evaluator loop | Software prototype | reproducible synthetic tests and fault injection |
| P02B | scientific direction, supervisory electronics | Software prototype | read-only signed telemetry, fault handling, scientific review |
| P02C | physical development-board commissioning | Not yet demonstrated | real board electrical characterization and OEM review |
| P03 | qualified planar neuronal/glial preparation | Research-gated | independently approved source and interface release |
| P04 | acquisition, retained Roots and assessment | Research-gated | delayed, independently scored skill beyond controls |
| P05 | observer, support and recipient networks | Research-gated | joined contribution, isolation and interference tests |
| P06 | 3D Soil/regional specialization and expansion | Research-gated | geometry-specific living qualification |
| P07 | physical Crystal and molecular transfer | Hypothesis | identified carrier, reader and task-specific causal evidence |
| P08 | scaling, service lifecycle and deployment | Future | reproducible end-to-end service release |

**Do not merge “source identity,” “functional maturity,” “learning,” “retention,” “joining,” or “transfer” into one success score.** Each requires a separate claim record and test.

See [scientific roles](science.md) and [acceptance principles](testing.md).
