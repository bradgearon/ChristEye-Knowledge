---
title: "CE-A01 episodic and working memory Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "current"
rights_holder: "We Develop Software LLC"
---

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

