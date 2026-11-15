---
title: "ChristEye Anaiero — CE-A01 Episodic And Working Memory engineering design"
document_id: "CE-A01-DESIGN"
revision: "3.1"
date: "2026-11-15"
owner: "We Develop Software LLC"
status: "Complete candidate engineering specification; living parameters require qualification"
---

# CE-A01 — Episodic And Working Memory

**Current design (revision 3.1)** · [Scientific companion](../companion/CE-A01_episodic_and_working_memory_Companion.md) · [Controlled processes](../../../processes/README.md) · [Evidence register](../../../research/evidence/anaiero-biology-2026-11.md)

> **Purpose:** candidate spinal Root and/or Soil ensemble, digital event-buffer first. The existing v3 digital architecture is retained in the source-history appendix below. This revision specifies cellular roles, axons, learning, support, joining and growth/evidence decisions; it is not a claim that those biological mechanisms have been demonstrated.

## 1. Product behavior and acceptance

**Intended outcome:** declare episode encoding → confirm reception → train contextual sequence → delayed cue recall → transfer.

**Criterion for accepting the result:** frozen-reader delayed improvement with source provenance and preserved earlier functions. Report measured performance, uncertainty, source preparation, comparator, reader version and observation window; a biological claim requires its own qualified experiment. Nothing in this release provides clinical or live-culture authorization.

## 2. Functional decomposition and owners

| Function | Responsible subsystem | Authority and record |
|---|---|---|
| Requested use and subject matter | StarChip CE-A01 and curriculum package | digitally scoped use request, task and source rights |
| Coordination | Lotus, central router and WindBus | permitted educational scheduling, typed receipts |
| Source cell production (when applicable) | Crest and cell-foundry scientist | separately qualified source-lot release; never an automatic model action |
| Living substrate | Soil / proposed spinal Root region | preparation version, independent care, measured maturity |
| Anatomical and electrical joining | axon and OEM instrumentation team | mapped physical structure, actual I/O receipts, timing and control |
| Physiological exchange | SustiBus | read-only care status and signed human service receipts |
| Molecular measurements | ChemiBus | assay/sample identity, calibration and named chemical form |
| Scientific judgment | independent evaluator and human scientific director | preregistered tests and separate claim approval |
| Protection | independently qualified Limit controller | physical veto and safe support continuation |

This document does **not** allocate one organoid to every software StarChip. It allocates *records, learning and observation contracts*; any physical preparation is justified separately. Use [BIO-01](../../../processes/BIO-01-crest-lineage.md) for progenitors and [BIO-03](../../../processes/BIO-03-soil-organoid.md) for the complete organoid Soil.

## 3. Biological composition and specific physical meaning

**Proposed living or digital substrate:** candidate spinal Root and/or Soil ensemble, digital event-buffer first.

Record qualified source line, tissue family (planar versus 3D), regional phenotype, neuronal/glial population coverage, candidate precursor versus post-mitotic cell status, cell-lot IDs and instrument visibility. Axon, dendrite, spine and synapse are **parts of neuron assemblies**, never separate cells that a Crest simply duplicates. Existing planar supplier counts are reference conditions, **not** a 3D module prescription: [P01 cell counts](../../../phases/P01/docs/01_Physical_Module_Composition_and_Counts.md).

**Cell-level lineage contract:** source_lot_id → clone_lineage_id (shared ancestral cohort) → observed cell_instance_id / cell_track_id (with observation-confidence metadata). A claimed division requires an actual CellBirthEvent; a genetic barcode is not assumed uniquely assigned to each baby cell. See [BIO-01](../../../processes/BIO-01-crest-lineage.md) and [SYS-12](../../../processes/SYS-12-lineage-ids.md).

## 4. Axon, spinal Root and Soil connection plan

axonal input order, recipient dendrites, and glial support characterized independently.

**Root tissue** in Anaiero is the *proposed spinal-derived material subsystem*. **Root function** is an independently demonstrated retained relation. Link a RootFunctionalTrace to a candidate SpinalRootSubstrate only when source, morphology and functional evidence support the association. A positive electrode correlation does **not** establish a direct axon or synapse. Four Mesh edge classes stay separate: STRUCTURAL_CONTACT, STATISTICAL_ASSOCIATION, INTERVENTION_SUPPORTED_INFLUENCE and TASK_RELATION. [BIO-02](../../../processes/BIO-02-axon-spinal-root.md).

Required anatomical outputs: axon_projection_id, source_region_id, proposed target_region_id, imaging transform, depth/coverage, candidate terminal marker, functional assay, latency uncertainty, alternative common-input explanation, and the temporal range in which the mapping is valid.

## 5. Molecules, proteins and structural candidates

**Module-specific ordinary candidates:** AMPA/NMDA/GABA/glycine; ARC, CaMKII, EAAT as readouts.

Do not treat these as all ingredients to be fed to cells. The candidate chemical role could be **source identity**, **receiving receptor**, **support**, **axonal/synaptic structure**, **modulation**, **assay readout** or **research-only hypothesis**. Each selected target needs target-specific identity, compartment, chemical form (if appropriate), receiver and scientific scope. Receptor expression or presence alone does not establish a working molecular Word or semantic encoding.

Refer to the original [41-entry material/structure inventory](../../../phases/P01/docs/02_Molecules_Structures_and_Development_Companion.md), [formal audit](../../../research/full-markdown/R03_03_Formal_Models_and_Reconciliation_1_.md) and the [2026 literature register](../../../research/evidence/anaiero-biology-2026-11.md).

## 6. Defined process and release stage sequence

**Local candidate education sequence:** declare episode encoding → confirm reception → train contextual sequence → delayed cue recall → transfer.

| Gate | Required evidence / transition | Action when unknown |
|---|---|---|
| G0 — source or digital identity | actual module ID, content rights, source lot if living, versioned codebook | HOLD |
| G1 — readout readiness | source measurement, artifact exclusion, valid units/clock, frozen reader | OBSERVE_ONLY |
| G2 — baseline and prerequisite | initial function and protected older tasks with comparators | NO_TRAINING |
| G3 — acquisition | actual cue/delivery receipt, training feedback separate from essential support | RECORD_UNRESOLVED |
| G4 — delayed Roots | feedback off, specified later task probe, old skill noninferiority | NO_ROOT_PROMOTION |
| G5 — mechanism/physical link | separately justified axon/molecular observation and controls | HYPOTHESIS_ONLY |
| G6 — expansion or replay | permissioned digital action, or independently approved lab study | HUMAN_REVIEW |
| G7 — scientific release | verified test artifact, claim scope, reviewer and rollback/retire plan | HOLD |

Detailed process definitions: - [BIO-02: BIO 02 axon spinal root](../../../processes/BIO-02-axon-spinal-root.md)
- [BIO-05: BIO 05 measurements](../../../processes/BIO-05-measurements.md)
- [BIO-06: BIO 06 teaching](../../../processes/BIO-06-teaching.md)
- [SYS-11: SYS 11 module training](../../../processes/SYS-11-module-training.md).

## 7. Data model, typed buses and storage

**Default data records:** ModuleRecord, SourceLot (if applicable), CellObservation, AxonProjection, RootAssessment, ChemicalObservation, CareReceipt, WindEvent, AuditEntry, ClaimRecord and GrowthChangeProposal.

**Module-specific primary fields:** | **Field**        | **Type and invariant** |
|------------------|------------------------|
| episode_id       | uuid                   |
| source_sha256    | hex64                  |
| actor_scope      | enum                   |
| project_id       | string                 |
| observed_at      | UTC                    |
| kind             | enum                   |
| payload_object   | sha256                 |
| consent_training | boolean                |
| consent_export   | boolean                |
| causal_parent    | uuid?                  |
| correction_of    | uuid?                  |

Canonical event rule: include schema version, producer, source hash, timestamp, permission scope, correlation ID and immutable lineage. Write primary records before making downstream decisions; an index, dashboard or generated summary is not source truth. Unknown and missing values remain distinguishable.
.

| Bus | This module sends / receives | Forbidden shortcut |
|---|---|---|
| WindBus | signed/typed digital task, observation, advice, replay and performance messages | sending task labels into a sealed test or physical actuation |
| ChemiBus | selected analyte/proteoform *measurements*, assay provenance and candidate mechanism review | encoding a software instruction as biochemical dose |
| SustiBus | qualified care status and human service receipts, sampled environment and support adequacy | conditioning essential nourishment on task score |
| BioBridge / axon map | OEM signal receipt, instrument source/recipient, anatomy evidence and route certificate | claiming a spike correlation proves axon connection |

**Wire invariant:** no "feed", "stimulate", "grow", "divide" or "introduce peptide" action on the digital bus. Only an independently qualified physical device and signed laboratory SOP can authorize live actions.

## 8. Training of associated neuron/Spirit systems

The software initiates a test only after the qualified experiment is registered. The actual biological protocol is authored by an authorized laboratory; Anaiero records task and cue semantics, *actual* delivered physical events, artifact/missingness, task response under a frozen reader and independent feedback. A living Spirit is a scoped neuron/glia assembly; its functional role must be measured, not assigned by its name. [BIO-06](../../../processes/BIO-06-teaching.md).

Controls: time-matched maturation, matched stimulation without answer information, digital-only reader and predictor, shared-context exposure, and delayed score without live teacher paths. Reserve a separate biological experiment for astrocytic, receptor or axonal contribution.

## 9. Long-term retention, access versus loss

Track **digital content integrity**, **cue/reader access**, **neural output validity**, and **retained task-specific use** as four different outcomes. Failing to recall could be lost source bytes, decoder mismatch, physical-care change, inactive route or a lost learned relation. A stored digital memory is not an engram; an observed task discrimination is not a portable Crystal. Report UNRESOLVED if alternatives remain. [BIO-05](../../../processes/BIO-05-measurements.md).

## 10. Physiological Milk, Water and digital Bread/Meat/Honey

Physical: use only a named institution-qualified complete culture medium and controlled environmental service. Report ingredients **by approved formulation/lot**, source container, exchange timing, true sample location, any glucose/lactate observation, drift and viability; do not invent doses or a feeder schedule. Digital: Bread supplies prerequisites, Meat supplies advanced task sets, Honey carries answer-contingent learning feedback. The physical Sea and digital access Water have separate state records.

**This module's support-specific inference limit:** decoder/index learning masquerading as new biological memory. See [BIO-04](../../../processes/BIO-04-sustibus-chemibus.md) and [BIO-10](../../../processes/BIO-10-care-health.md).

## 11. Growth and renewal decision rule

**Specific decision:** no new tissue without demonstrated persistent deficit after software repair.

First evaluate whether the problem is data, codebook, retrieval, interface, health or genuinely demonstrated capacity. Differentiate new cell birth, axon extension, new synaptic association, glial support change and digital hardware growth. Require human review before any living change. Do not equate more cells/more firing/more molecular expression with more Understanding. [BIO-09](../../../processes/BIO-09-growth-control.md).

## 12. Mandatory food/peptide/growth alteration card

| Field | Required value for each proposal |
|---|---|
| Change ID / target | immutable proposal, module CE-A01, preparation, region and current source/SOP revision |
| Trigger | specific deficiency/hypothesis + independent measured source, not a learning score alone |
| Candidate | actual qualified nutrient/formulation revision OR specific studied chemical form/receptor; mark if not validated |
| Objective | one declared primary health, target engagement, morphology or Root functional endpoint |
| Prior data | assay source, spatial location, time series, calibration, axon morphology, prior task and viability |
| Protected outcomes | independent care, earlier Roots, axon routes, identity/lineage, neighboring populations, off-target effects |
| Comparator | matched qualified control, expected alternative mechanisms, preregistered analysis |
| Authority | named PI, cell-foundry lead, biosafety/safety and approved SOP; software cannot self-approve |
| Postcondition | actual care/intervention receipt, adverse monitoring and delayed functional re-evaluation |
| Status | DRAFT / LAB_REVIEW / NOT_APPROVED / APPROVED_IN_SOP / UNRESOLVED |

**No generic peptide recipe, genomic insertion protocol or automated medium modification appears in this software implementation.** Until ordinary molecule, receiver, effect and safety are demonstrated, this card remains a research proposal.

## 13. Monitoring and observable engineering evidence

Link calibrated chemistry and metabolism (including glucose/lactate as *state*, not knowledge content), axon/dendrite imaging, candidate RNA/protein assays, network electrophysiology and task outcomes through preparation and timing anchors. Sampling/assay does not imply every RNA and protein can be watched continuously.

**Specific failure to distinguish:** decoder/index learning masquerading as new biological memory. Preserve raw readings and alternative explanations instead of rewriting the intended outcome.

## 14. Fault, shutdown and recovery states

BOOT → SOURCE_VALIDATED → OBSERVATION_VALID → DIGITAL_TRAINING_OR_APPROVED_STUDY → EVALUATION → REVIEW_OR_RELEASE. On invalid sensor, missing environment qualification, label leakage or tissue-health concern: preserve essential care under independent control, suspend unqualified task input, latch scientific uncertainty and request human review. The digital prototype must never directly control care or electrode stimulation.

## 15. Implementation milestones and source handoff

1. **M0: records and source identity** — schema-backed records, actual owners, zero fabricated biological fields.
2. **M1: digital simulation** — WindBus, Lotus, evaluator and data storage tests including corrupted/stale events.
3. **M2: instrument bench qualification** — selected OEM hardware, actual clocks, cable/ground isolation, validity and inhibit evidence.
4. **M3: preparation qualification** — institutional source/quality, qualified independent support, observed geometry and anatomical identities.
5. **M4: declared learning** — controlled acquisition and delayed Root tests with frozen reader and comparison.
6. **M5: mechanistic/growth study** — named cell/axon/chemical contribution only if a separate PI-approved protocol warrants it.
7. **M6: independent review** — evidence packet, negative results, plan revision and compatibility release.

## 16. Acceptance and negative tests

**Primary acceptance:** frozen-reader delayed improvement with source provenance and preserved earlier functions.

**Required rejection or alternative:** decoder/index learning masquerading as new biological memory. Verify explicit controls for bad sensors, shared input, unequal support, scope leakage, source-line heterogeneity, data retrieval, tissue maturation and decoder adaptation. Report per-class metrics and preparation-level uncertainty rather than only a single gain score.

## 17. Source precedence and current scientific status

[Human spinal organoid source/identity B08](https://www.nature.com/articles/s41598-026-45679-8); [functional readout review B01](https://www.nature.com/articles/s44222-026-00507-6); [corticospinal axon connectoid B02](https://pubmed.ncbi.nlm.nih.gov/42201810/); [lineage-barcoding limitations B05](https://pmc.ncbi.nlm.nih.gov/articles/PMC11996212/); [astrocyte–neuron lactate B07](https://pmc.ncbi.nlm.nih.gov/articles/PMC3073831/); [ISSCR guidelines B09](https://www.isscr.org/resources/guidelines-for-stem-cell-research). These are scoped precedent, **not validation of this specific module**.

## 18. Reference process routing

- [BIO-02: BIO 02 axon spinal root](../../../processes/BIO-02-axon-spinal-root.md)
- [BIO-05: BIO 05 measurements](../../../processes/BIO-05-measurements.md)
- [BIO-06: BIO 06 teaching](../../../processes/BIO-06-teaching.md)
- [SYS-11: SYS 11 module training](../../../processes/SYS-11-module-training.md).

The process specification defines qualification and state changes; this document defines **the module's reason for using that process**. Any approved laboratory experimental SOP remains independently controlled.

## 19. Next scientific decision

Can CE-A01 provide frozen-reader delayed improvement with source provenance and preserved earlier functions under the declared controls, with its primary ordinary substrate and instrument observations independently qualified? If no, record failure or UNRESOLVED. If yes, promote only the exact demonstrated capability, not a complete mind, memory replication or physically packaged Knowledge.

## 20. WDS ownership, change control and original digital specification

© 2026 We Develop Software LLC. [WDS Public Research Reference License](../../../LICENSE.md). The confidential physical patent document is excluded from this public research repository. This module revision replaces generic speculation with concrete data and scientific gate requirements, not a claimed laboratory SOP.

## Original v3.0 design — full preserved source

> Prior implementation edition retained for provenance and regression review. v3.1 sections above take precedence for scientific claims.

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `docs/design/CE-A01_episodic_and_working_memory_Design.docx`  
> **SHA-256 (original DOCX):** `fea7a4af4cdaa133db20c6990bf6dac1d4f4e08ba365ce0b52a7efc2c51ef0e0`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**CE-A01 / DESIGN / 14 NOVEMBER 2026**

Working and Episodic Memory StarChip

Implementation contract, data models, controller behavior and acceptance

| **Track** | **Readiness** | **Source precedence**                                        |
|-----------|---------------|--------------------------------------------------------------|
| DESIGN    | digital-first | Research editions R01–R03 govern conflicting older equations |

# 1. Intended result and scope

Learn to capture recent experiences without confusing a recorded event with a durable memory or a changed biological engram.

Local ingestion service writes immutable UsageEvents; a restricted summarizer generates candidate assertions; two independent indexes hold temporal keys and lexical/vector retrieval features; the Root Evaluator alone can promote a retained competency. Keep direct observations apart from interpretations and source-grounded answers.

# 2. System boundary, responsibilities and exclusions

- Provide bounded working context with eviction rules and source attribution

- Partition episodes by user, device, project and consent scope

- Train retrieval with time-separated examples, paraphrases and negative distractors

- Promote only validated episodic summaries to candidate Roots

Inputs: WindEvent; permissioned user action; timestamped task outcome; raw reference. Outputs: EpisodeRecord; short-context summary; retrieval candidates; promotion proposal.

Dependency interfaces: A19 ingress, A20 filesystem, A16 Root Registry, A12 Lotus, A22 limits. This module never owns a biological operating limit or a sealed evaluator label unless explicitly identified by a laboratory-approved component.

# 3. Canonical data record and persistence

| **Field**        | **Type and invariant** |
|------------------|------------------------|
| episode_id       | uuid                   |
| source_sha256    | hex64                  |
| actor_scope      | enum                   |
| project_id       | string                 |
| observed_at      | UTC                    |
| kind             | enum                   |
| payload_object   | sha256                 |
| consent_training | boolean                |
| consent_export   | boolean                |
| causal_parent    | uuid?                  |
| correction_of    | uuid?                  |

Canonical event rule: include schema version, producer, source hash, timestamp, permission scope, correlation ID and immutable lineage. Write primary records before making downstream decisions; an index, dashboard or generated summary is not source truth. Unknown and missing values remain distinguishable.

# 4. Processing and state transitions

Declared state sequence: ACQUIRE → NORMALIZE → REDACT → INDEX → RETRIEVE → VERIFY → PROMOTE_OR_EXPIRE. Treat each stage as an idempotent transition with a recorded request and receipt; on failure, preserve the last qualified state and the reason.

1\. Normalize an input to canonical UTF-8, attach a stable source hash and the user-selected project/permission scope. Refuse an event with no source or no applicable recording permission.

2\. Append the full event and its predecessor hash to the immutable journal before asynchronous indexing; never treat an index as the system of record.

3\. Extract candidate task/context/action/result spans and store offsets to the original source; isolate model-inferred fields with an explicit confidence and model version.

4\. Retrieve using a lexical baseline followed by an optional semantic ranker; filter first by rights, project and cutoff time, then rank only eligible hits.

5\. On correction, append a superseding event and close the previous assertion’s active interval, retaining evidence history; revocation removes it from all active indexes and Crystal eligibility.

6\. Create delayed recall tests from episodes by holding out whole tasks and later timestamps; promote only evidence-backed, independently assessed outcomes.

# 5. Reference algorithm and executable design

Append input to an event log; extract task/context/outcome; form semantic and temporal indexes; run held-out retrieval probes; reject leakage from future events; pass promotion proposals to the Root Registry.

def process_a01(request, registry, ledger, evaluator, limits):

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

User edits an AHD chord progression and explains why. Capture the audio/MIDI file digest, change diff, user comment and resultant listening decision as separate sources. A later answer must cite the event, not invent the reasoning.

# 7. Implementation work and integration order

Phase 1: Implement permissioned episode ledger. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 2: Create temporal and semantic retrieval baselines. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 3: Add delayed recall benchmark and evidence QA. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 4: integrate with WindBus, Lotus decisions, provenance and failure receipt; simulate adverse input and restart before any runtime promotion. Phase 5: publish versioned capability and performance certificate, explicitly digital-only or qualified-laboratory research.

# 8. Acceptance tests and failure response

| **Required test or failure** | **Required observable evidence**                                          |
|------------------------------|---------------------------------------------------------------------------|
| 1                            | A held-out question resolves to the correct episode with a source pointer |
| 2                            | Conflicting or revoked episodes do not become authoritative               |
| 3                            | No future or sealed test event can enter a training answer                |
| R1                           | Summaries can hallucinate details                                         |
| R2                           | Over-remembering exposes private context                                  |
| R3                           | High retrieval scores can reflect source leakage                          |

Report these metrics separately (do not replace with a composite score): MRR@k, recall@k, false-memory rate, evidence-link coverage, answer latency and retention by interval; report per-source and per-user slices.

# 9. Scientific and policy boundary

Neural response signals are candidate task features, not direct readouts of personal autobiographical memories. Biological short-term plasticity must be demonstrated independently.

Physical interventions, culture production, medium formulation, stimulation envelopes, peptide treatments and experimental material transfer are not executed by the digital reference runtime. Those depend on the separate institutional approval, source identification, qualified equipment and safety envelopes documented in research source R02 and predecessor R04.

# 10. Sources and status

Original design trace: CE-DEV-001 §21; CE-CRYSTAL-SYS-001 Requirements R02–R05 and EX04.

Source set: R01–R03 are the latest supplied synthesis and mathematical audit; R04 is the physical device predecessor. Current measurements: none generated by this document. Software examples and planned acceptance thresholds are proposals until executed and reported.


