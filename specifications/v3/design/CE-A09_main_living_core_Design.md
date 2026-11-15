---
title: "CE-A09 main living core Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "current"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `docs/design/CE-A09_main_living_core_Design.docx`  
> **SHA-256 (original DOCX):** `acd1698b6c07a3157e2ea8eb6ef3718df363b75f16de4966b04d864cf1fdce3b`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**CE-A09 / DESIGN / 14 NOVEMBER 2026**

Main Living Network: Soil, Spirit, Heart and Tree

Implementation contract, data models, controller behavior and acceptance

| **Track** | **Readiness**                | **Source precedence**                                        |
|-----------|------------------------------|--------------------------------------------------------------|
| DESIGN    | hybrid research architecture | Research editions R01–R03 govern conflicting older equations |

# 1. Intended result and scope

Define the core computing substrate as a qualified living Soil with identifiable Spirit participants, response-forming Heart, linked Mesh and demonstrable capabilities.

Physical separation: a living Core C with its own qualified support, Host H with acquisition and codebook, independent Evaluator E holding labels, independent Limit L enforcing qualified instrument permissions, trained Observer O and optional Support S connected through electronic bridge. Physical Soil denotes the living preparation as a whole; Spirit roles refer to measured participant networks. A biological Heart is not the software safety controller.

# 2. System boundary, responsibilities and exclusions

- Separate tissue boundaries from host software boundaries

- Support task-specific input/response codebooks and independent evaluation

- Report neuronal, astrocytic, inhibitory, structural and telemetry components as typed participants

- Join observers and other modules through reversible electronic interfaces first

Inputs: Qualified stimulus profile identifiers; task context; sensor observations; care status. Outputs: Measured neural response; decoded Wind; trial receipt; Root evidence.

Dependency interfaces: A11 WindBus, A12 Lotus, A24 observer, A17 support, A22 limit controller. This module never owns a biological operating limit or a sealed evaluator label unless explicitly identified by a laboratory-approved component.

# 3. Canonical data record and persistence

| **Field**             | **Type and invariant** |
|-----------------------|------------------------|
| trial_id              | uuid                   |
| preparation_id        | string                 |
| task_codebook_version | string                 |
| requested_event       | id                     |
| delivered_event       | id?                    |
| raw_record_ref        | string                 |
| mask_digest           | sha256                 |
| decoder_digest        | sha256                 |
| response              | enum                   |
| validity              | enum                   |
| evaluator_label       | enum?                  |
| feedback_receipt      | id?                    |

Canonical event rule: include schema version, producer, source hash, timestamp, permission scope, correlation ID and immutable lineage. Write primary records before making downstream decisions; an index, dashboard or generated summary is not source truth. Unknown and missing values remain distinguishable.

# 4. Processing and state transitions

Declared state sequence: SOURCE_RELEASE → CHARACTERIZE → INTERFACE_READY → TRAIN → REST → EVALUATE → REVIEW. Treat each stage as an idempotent transition with a recorded request and receipt; on failure, preserve the last qualified state and the reason.

1\. Commission a cell-free host where the full task loop, clock, artifact masks and abort paths are testable without tissue.

2\. Version each preparation by source, geometry (planar or 3D), cell identities, environment and instrumentation; do not generalize across versions.

3\. Qualify codebook delivery and recorded response with the lab, then freeze readout for learning comparisons.

4\. Task oracle: C0+AB→X, C0+BA→Y, C1+AB→Y, C1+BA→X, identical AA/BB→HOLD; missing input→UNRESOLVED instrument status.

5\. Deliver feedback only after the response window in a qualified training arm; withhold feedback and sealed targets during evaluation.

6\. Report valid class macro recall, invalid fraction, delayed tests and differences to controlled software-only baselines as separate claims.

# 5. Reference algorithm and executable design

Host H presents only authorized task events and records delivered events; neural activity remains a physical observation; decoder outputs a class or UNKNOWN; evaluator E scores after response closure and controls feedback disclosure.

def process_a09(request, registry, ledger, evaluator, limits):

require_schema_and_authority(request)

state = load_versioned_state(request.target_id)

require_fresh_sources_and_reader(state)

candidates = propose_scoped_steps(request, state)

decision = limits.filter(candidates, state)

if decision.is_unresolved: return record("UNRESOLVED")

receipt = stage_without_unqualified_physical_action(decision)

result = evaluator.assess_on_sealed_data(receipt)

return ledger.append(lineage(request, receipt, result))

The reference pseudocode is a controller contract, not a claim that an implementation already exists for every stage. Specific interfaces and validation belong to the module records below and to the executable digital sample.

# 6. Concrete usage trace

If a missing context input produces UNRESOLVED, this is not a learned HOLD response. A silent channel invalidates the trial rather than earning task credit.

# 7. Implementation work and integration order

Phase 1: Create software simulator matching device contracts. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 2: Build read-only MEA adapter interface. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 3: Define independent evidence gates before living research. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 4: integrate with WindBus, Lotus decisions, provenance and failure receipt; simulate adverse input and restart before any runtime promotion. Phase 5: publish versioned capability and performance certificate, explicitly digital-only or qualified-laboratory research.

# 8. Acceptance tests and failure response

| **Required test or failure** | **Required observable evidence**                                      |
|------------------------------|-----------------------------------------------------------------------|
| 1                            | Valid input/output responses beyond artifacts                         |
| 2                            | Learning beyond matched noncontingent controls                        |
| 3                            | A claimed biology-driven result survives frozen digital readout tests |
| R1                           | Organoid is not a validated full mind                                 |
| R2                           | Spinal cells and cerebral organoids are different preparations        |
| R3                           | Morphology does not establish task understanding                      |

Report these metrics separately (do not replace with a composite score): Macro recall, availability, channel validity, network drift, delayed retention, route contribution, calibration.

# 9. Scientific and policy boundary

Organoid preparation choice, species, regional identity, cell composition, supplier instructions and institutional approval are controlled external dependencies, not generated prescriptions.

Physical interventions, culture production, medium formulation, stimulation envelopes, peptide treatments and experimental material transfer are not executed by the digital reference runtime. Those depend on the separate institutional approval, source identification, qualified equipment and safety envelopes documented in research source R02 and predecessor R04.

# 10. Sources and status

Original design trace: CE-DEV-001 §§4–5, 13–18, 23–24; CE-CRYSTAL-SYS-001 §Architecture and EX01/EX04.

Source set: R01–R03 are the latest supplied synthesis and mathematical audit; R04 is the physical device predecessor. Current measurements: none generated by this document. Software examples and planned acceptance thresholds are proposals until executed and reported.

