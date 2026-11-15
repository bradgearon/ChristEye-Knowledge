# The 24 original Anaiero module data contracts

> **ChristEye Anaiero · 15 November 2026 · We Develop Software LLC**  
> **Status:** Research design · unvalidated biological proposal · WDS Public Research Reference License.  
> [Documentation Home](index.md) · [Full document library](full-document-library.md) · [Phase P01 original](../phases/P01/README.md)

The complete data schema, process steps, and proposed behaviors for each module from the project-owned module-contract registry. See corresponding full design/companion Markdown files under [specifications/v3](../specifications/v3/README.md).

## CE-A01: Working and Episodic Memory StarChip

**Implementation track:** `digital-first` · **Primary record:** `EpisodeRecord` · [Original manifest](../registries/v6/module-manifests/CE-A01.json)

### Data record

| Field | Type/constraint |
|---|---|
| `episode_id` | `uuid` |
| `source_sha256` | `hex64` |
| `actor_scope` | `enum` |
| `project_id` | `string` |
| `observed_at` | `UTC` |
| `kind` | `enum` |
| `payload_object` | `sha256` |
| `consent_training` | `boolean` |
| `consent_export` | `boolean` |
| `causal_parent` | `uuid?` |
| `correction_of` | `uuid?` |

### Processing contract

1. Normalize an input to canonical UTF-8, attach a stable source hash and the user-selected project/permission scope. Refuse an event with no source or no applicable recording permission.
2. Append the full event and its predecessor hash to the immutable journal before asynchronous indexing; never treat an index as the system of record.
3. Extract candidate task/context/action/result spans and store offsets to the original source; isolate model-inferred fields with an explicit confidence and model version.
4. Retrieve using a lexical baseline followed by an optional semantic ranker; filter first by rights, project and cutoff time, then rank only eligible hits.
5. On correction, append a superseding event and close the previous assertion’s active interval, retaining evidence history; revocation removes it from all active indexes and Crystal eligibility.
6. Create delayed recall tests from episodes by holding out whole tasks and later timestamps; promote only evidence-backed, independently assessed outcomes.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A01.schema.json)

---

## CE-A02: Long-Term Storage and Consolidation StarChip

**Implementation track:** `digital-first` · **Primary record:** `DurableObject` · [Original manifest](../registries/v6/module-manifests/CE-A02.json)

### Data record

| Field | Type/constraint |
|---|---|
| `sha256` | `hex64` |
| `plaintext_size` | `uint64` |
| `media_type` | `MIME` |
| `tenant` | `string` |
| `retention_policy` | `enum` |
| `origin_event_id` | `uuid` |
| `created_at` | `UTC` |
| `encryption_key_id` | `string?` |
| `supersedes` | `sha256?` |
| `verified_at` | `UTC?` |

### Processing contract

1. Write blob bytes to a temporary name, fsync, compute SHA-256, atomically rename to objects/aa/digest and update the metadata transaction.
2. Index only committed objects using a durable outbox; a worker retries idempotently and writes index_generation on success.
3. Create a Merkle-style snapshot manifest listing sorted logical paths, digests, permissions and policy version; hash and sign the manifest before publishing.
4. Run restore drills on a clean instance, verify object digests, rebuild indexes and reconcile active tombstones before allowing queries.
5. Define compaction separately from deletion: superseded material can be retained for traceability while an authorized erasure request removes protected payloads according to policy.
6. Qualify all critical recovery paths through simulated crash points before describing a repository as long-term storage.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A02.schema.json)

---

## CE-A03: Healing and Integrity Recovery Network

**Implementation track:** `digital-first + monitored research` · **Primary record:** `RecoveryCase` · [Original manifest](../registries/v6/module-manifests/CE-A03.json)

### Data record

| Field | Type/constraint |
|---|---|
| `case_id` | `uuid` |
| `onset` | `UTC` |
| `symptom` | `enum` |
| `affected_routes` | `list` |
| `sensor_quality` | `object` |
| `competing_hypotheses` | `list` |
| `selected_action` | `enum` |
| `permit_receipt` | `id?` |
| `before_after_test` | `id?` |
| `status` | `enum` |

### Processing contract

1. Validate timestamps, freshness and recorded delivery before inferring a network deficit.
2. Generate named hypotheses with predicted observations for each, rather than labeling every poor response as forgotten knowledge.
3. Try reversible software checks in increasing order of scope: replay a valid message in simulation, switch an approved digital route, rebuild an index from durable objects, restore a known configuration.
4. Freeze task scoring before each comparison; report both target recovery and protected old-task performance.
5. On biological warning, request qualified review and keep routine approved life support separate from task pause; block unsupported changes.
6. Write a complete RecoveryCase with selected explanation, alternative explanations and unresolved status if evidence cannot distinguish them.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A03.schema.json)

---

## CE-A04: Growth and Capacity Planning Network

**Implementation track:** `digital-first + monitored research` · **Primary record:** `GrowthProposal` · [Original manifest](../registries/v6/module-manifests/CE-A04.json)

### Data record

| Field | Type/constraint |
|---|---|
| `proposal_id` | `uuid` |
| `diagnosed_gap` | `string` |
| `comparator_results` | `list` |
| `resource_budget` | `object` |
| `candidate_module` | `string` |
| `expected_gain_vector` | `object` |
| `uncertainty` | `object` |
| `protected_roots` | `list` |
| `permit` | `enum` |
| `observed_gain` | `object?` |

### Processing contract

1. Collect prospective saturation indicators: queue delay, memory pressure, task failure by type, effective data coverage and skill-transfer score.
2. Ask the diagnostic module whether the gap is routing, access, data quality, insufficiency of examples or genuinely missing computational capacity.
3. Model candidate actions: no change, index refresh, curriculum revision, software specialization, new digital module, research review.
4. Estimate expected quality gain and 95% uncertainty using held-out tasks; reject plans whose resource cost or protected-task loss exceeds approved bounds.
5. Deploy a new digital StarChip in shadow mode with the same observations, score it against old behavior and only then admit it to active routing.
6. For living research, request independent source, maturation, interface, maintenance and experimental qualification; never instantiate a cell population through software.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A04.schema.json)

---

## CE-A05: Refreshing, Renewal and Maintenance Network

**Implementation track:** `digital-first + monitored research` · **Primary record:** `RefreshPlan` · [Original manifest](../registries/v6/module-manifests/CE-A05.json)

### Data record

| Field | Type/constraint |
|---|---|
| `plan_id` | `uuid` |
| `target_id` | `string` |
| `last_validated` | `UTC` |
| `age` | `seconds` |
| `stale_dependencies` | `list` |
| `action` | `enum` |
| `snapshot_before` | `sha256` |
| `outcome_receipt` | `uuid` |
| `followup_due` | `UTC` |

### Processing contract

1. Attach validity intervals and explicit source status to each fact, route and capability certificate.
2. Schedule periodic queries and retrieval tests that do not leak answers from the future to the learner.
3. If a source is stale, reacquire only under authorized ingestion policy and retain old source for audit.
4. If a skill regresses, first retest using its established cue and unchanged decoder; compare with storage, access and environment explanations.
5. Refresh the digital cache or replay a Crystal in staging, then run old and new task tests before promotion.
6. Issue a research-review request for any observed physiological decline instead of assuming that more training, food or stimulation is a repair.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A05.schema.json)

---

## CE-A06: Learning Stabilization and Consolidation Network

**Implementation track:** `digital-first + research hypothesis` · **Primary record:** `RetentionStudy` · [Original manifest](../registries/v6/module-manifests/CE-A06.json)

### Data record

| Field | Type/constraint |
|---|---|
| `study_id` | `uuid` |
| `capability_id` | `string` |
| `participant_id` | `string` |
| `training_epoch` | `string` |
| `probe_schedule` | `list` |
| `holdout_digest` | `sha256` |
| `reader_digest` | `sha256` |
| `immediate` | `float` |
| `delayed` | `dict` |
| `controls` | `list` |
| `status` | `enum` |

### Processing contract

1. Define a competency and held-out task family before training; seal labels and reader/decoder version.
2. Record immediate acquisition, then delayed performance after teaching feedback has been disabled.
3. Record probe exposure itself, because retrieval can alter learning; where feasible use separate cohorts or matched schedules.
4. Analyze absolute delayed macro recall and paired deltas by biological preparation or digital training run, not electrode or trial pseudo-replicates.
5. Compare spaced versus massed curriculum only when total exposure and task-information supply are known and matched.
6. Propose promotion only when the lower confidence bound clears a preregistered target and old protected skills meet noninferiority criteria.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A06.schema.json)

---

## CE-A07: Learning Enhancement and Curriculum Network

**Implementation track:** `digital-first` · **Primary record:** `CurriculumUnit` · [Original manifest](../registries/v6/module-manifests/CE-A07.json)

### Data record

| Field | Type/constraint |
|---|---|
| `unit_id` | `string` |
| `domain` | `string` |
| `prerequisites` | `list` |
| `content_digest` | `sha256` |
| `target_skill` | `string` |
| `format` | `enum` |
| `learner_reader` | `string` |
| `assessment_digest` | `sha256` |
| `mastery_rule` | `object` |
| `copyright_scope` | `string` |

### Processing contract

1. Build a DAG of competencies with stable IDs and no prerequisite cycles.
2. For each skill create training examples, analogies, counterexamples, transfer cases and a withheld test set; never embed answer keys in learner-visible text.
3. At each turn choose among ready lessons using a declared utility of uncertainty reduction, novelty, estimated improvement and cost.
4. Use an explicit lesson-to-receiver adapter: LLM prompt/RAG/trainable model for digital modules; candidate stimulus encoding is a research-only interface.
5. After a learning block, test mastery and old-skill regression separately; reject progression if delays or held-out results fail.
6. Version the curriculum and keep the source license and produced representations so CrystalPack export is rights-auditable.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A07.schema.json)

---

## CE-A08: Health, Anti-Disease Surveillance and Biosafety Network

**Implementation track:** `monitored research` · **Primary record:** `HealthAlert` · [Original manifest](../registries/v6/module-manifests/CE-A08.json)

### Data record

| Field | Type/constraint |
|---|---|
| `alert_id` | `uuid` |
| `module_id` | `string` |
| `sensor_id` | `string` |
| `observed_at` | `UTC` |
| `validity` | `enum` |
| `threshold_ref` | `string` |
| `deviation` | `object` |
| `confidence` | `float?` |
| `category` | `enum` |
| `required_review` | `string` |
| `receipt` | `id?` |

### Processing contract

1. For each sensor record calibration, location, frequency, freshness and operating bounds supplied by the qualified lab.
2. Filter malformed/stale readings before trend detection and propagate UNKNOWN rather than normal.
3. Use rule-based deterministic alarms as the reference; compare any trained observer to that same information set.
4. Correlate alerts with source/batch, maintenance and instrumentation changes without inferring pathogen presence from neural activity.
5. Keep independent baseline support active when task work pauses; unauthorized medium, peptide or genetic changes are never emitted.
6. Record all alert acknowledgements, reviewed dispositions, false alarms and overdue service.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A08.schema.json)

---

## CE-A09: Main Living Network: Soil, Spirit, Heart and Tree

**Implementation track:** `hybrid research architecture` · **Primary record:** `BiologicalTrial` · [Original manifest](../registries/v6/module-manifests/CE-A09.json)

### Data record

| Field | Type/constraint |
|---|---|
| `trial_id` | `uuid` |
| `preparation_id` | `string` |
| `task_codebook_version` | `string` |
| `requested_event` | `id` |
| `delivered_event` | `id?` |
| `raw_record_ref` | `string` |
| `mask_digest` | `sha256` |
| `decoder_digest` | `sha256` |
| `response` | `enum` |
| `validity` | `enum` |
| `evaluator_label` | `enum?` |
| `feedback_receipt` | `id?` |

### Processing contract

1. Commission a cell-free host where the full task loop, clock, artifact masks and abort paths are testable without tissue.
2. Version each preparation by source, geometry (planar or 3D), cell identities, environment and instrumentation; do not generalize across versions.
3. Qualify codebook delivery and recorded response with the lab, then freeze readout for learning comparisons.
4. Task oracle: C0+AB→X, C0+BA→Y, C1+AB→Y, C1+BA→X, identical AA/BB→HOLD; missing input→UNRESOLVED instrument status.
5. Deliver feedback only after the response window in a qualified training arm; withhold feedback and sealed targets during evaluation.
6. Report valid class macro recall, invalid fraction, delayed tests and differences to controlled software-only baselines as separate claims.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A09.schema.json)

---

## CE-A10: Experience-to-Crystal Compiler

**Implementation track:** `digital-first` · **Primary record:** `CrystalManifest` · [Original manifest](../registries/v6/module-manifests/CE-A10.json)

### Data record

| Field | Type/constraint |
|---|---|
| `pack_id` | `string` |
| `semver` | `string` |
| `kind` | `enum` |
| `producer` | `string` |
| `build_digest` | `sha256` |
| `artifacts` | `list[digest]` |
| `lessons` | `list` |
| `memory_roots` | `list` |
| `license` | `object` |
| `reader_contract` | `string` |
| `tests` | `object` |
| `evidence_tier` | `enum` |
| `installable` | `boolean` |

### Processing contract

1. Resolve a declarative build plan naming project, event interval, allowed rights, source roots and target reader.
2. Apply revocation and export filters before any text extraction or indexing; produce a redaction report and no hidden copies in assets.
3. Deduplicate by content hash, preserve exact citations and include failed/corrected episodes when permitted.
4. Build lessons and replay events with signed causal ordering; store evaluation prompts separately with their digest.
5. Run deterministic schema validation, declared security and license checks and prospective behavior tests in staging.
6. Produce reproducible pack bytes and immutable build receipt including tool versions, input hashes and capabilities proven—not claimed.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A10.schema.json)

---

## CE-A11: Connective Controllers, WindBus and Bridge Endpoints

**Implementation track:** `digital-first + measured biological adapter` · **Primary record:** `WindEnvelope` · [Original manifest](../registries/v6/module-manifests/CE-A11.json)

### Data record

| Field | Type/constraint |
|---|---|
| `event_id` | `uuid` |
| `trace_id` | `uuid` |
| `source_id` | `string` |
| `recipient_id` | `string` |
| `schema_id` | `string` |
| `version` | `semver` |
| `kind` | `enum` |
| `observed_at` | `UTC` |
| `issued_at` | `UTC` |
| `expires_at` | `UTC` |
| `payload_ref` | `sha256` |
| `source_scope` | `enum` |
| `signature` | `bytes?` |

### Processing contract

1. Register publisher/subscriber capabilities with declared schema versions and authorized message kinds.
2. Validate payload structure, size, freshness, consent, receiver compatibility and recursion depth before enqueue.
3. Persist a per-route sequence number and event ID; acknowledge processed messages only after downstream durable commit.
4. Use at-least-once delivery with idempotency keys rather than promising unimplementable exactly-once biological effects.
5. Measure acquisition→encoding→queue→recipient delivery latencies and log requested versus actual physical delivery separately.
6. For each bridge compare isolated, bridged, shuffled, replay and software-teacher conditions; clear queues and cached outputs during disconnect.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A11.schema.json)

---

## CE-A12: Lotus Main Coordinator and Developmental Operating System

**Implementation track:** `digital-first` · **Primary record:** `LotusDecision` · [Original manifest](../registries/v6/module-manifests/CE-A12.json)

### Data record

| Field | Type/constraint |
|---|---|
| `decision_id` | `uuid` |
| `state_snapshot` | `sha256` |
| `candidates` | `list` |
| `expected_effects` | `vector` |
| `protected_margin` | `vector` |
| `uncertainty` | `object` |
| `ranking_policy` | `string` |
| `veto_receipt` | `string` |
| `result` | `enum` |
| `review_due` | `UTC?` |

### Processing contract

1. Assemble a state vector from fresh, permissioned observations and explicit missingness.
2. Enumerate allowable actions: NO_CHANGE, DEFER_TASK, REQUEST_REVIEW, approved digital alternate route, replay in staging, snapshot, or curriculum evaluation.
3. Eliminate actions that violate hard constraints or protected Root noninferiority margins before ranking.
4. Use a declared scalar utility for eligible digital actions with uncertainty; if confidence intervals overlap zero choose UNRESOLVED or gather additional evidence.
5. Record proposal, alternatives, decision inputs, independent veto and executed receipt as separate events.
6. Re-evaluate actions on sealed outcomes; change the planner only via versioned offline deployment, never recursive self-approval.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A12.schema.json)

---

## CE-A13: Crystal Replication, Versioning and Cloning

**Implementation track:** `digital-first + research-only physical branch` · **Primary record:** `CloneReceipt` · [Original manifest](../registries/v6/module-manifests/CE-A13.json)

### Data record

| Field | Type/constraint |
|---|---|
| `parent_pack` | `sha256` |
| `child_pack` | `sha256` |
| `version` | `string` |
| `copy_mode` | `enum` |
| `authorizer` | `string` |
| `rights_audit` | `sha256` |
| `reader` | `string` |
| `clone_tests` | `object` |
| `isolated_test` | `object` |
| `created_at` | `UTC` |

### Processing contract

1. Verify all source assets and license/consent rights before copying.
2. Perform digest-addressed copies, then compare child and parent root manifests and their independent signatures.
3. If a new reader or software version is specified, record a compatibility migration and do not call it byte-identical.
4. Install into a new sandbox whose cache, training data and credentials are isolated from the source.
5. Evaluate output equivalence and skill equivalence separately on sealed tasks; report expected nondeterminism and outcome distributions.
6. For biological recipient B, require independent-only evaluation after all teacher and shared digital channels are verifiably removed.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A13.schema.json)

---

## CE-A14: Crystal Consumption, Experience Replay and Remanifestation

**Implementation track:** `digital-first + biological study concept` · **Primary record:** `ConsumptionJob` · [Original manifest](../registries/v6/module-manifests/CE-A14.json)

### Data record

| Field | Type/constraint |
|---|---|
| `job_id` | `uuid` |
| `pack_digest` | `sha256` |
| `target_reader` | `string` |
| `mode` | `enum` |
| `state` | `enum` |
| `replay_cursor` | `uint64` |
| `consent_snapshot` | `sha256` |
| `assessment_set` | `sha256` |
| `rollback_checkpoint` | `sha256` |
| `activation_receipt` | `id?` |

### Processing contract

1. Discover pack and reader; verify signed manifest, complete assets, rights, dependencies and model/hardware compatibility.
2. Stage with no access to production secrets or physical actuators; snapshot target digital state.
3. Choose a declared consumption mode and create a replay schedule with deterministic order and content-level de-duplication.
4. Route Words and correction examples via the target’s supported digital interface; log every replay cue, source, timestamp and output.
5. Stop the teacher, clear cached answer-bearing context, freeze the reader and evaluate unseen tasks plus protected skills.
6. Promote only when criteria pass; otherwise restore software checkpoint and retain failed attempt as evidence; biological state is never described as rolled back.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A14.schema.json)

---

## CE-A15: Organoid Soil Development, Geometry and Morphology

**Implementation track:** `qualified-lab research only` · **Primary record:** `PreparationVersion` · [Original manifest](../registries/v6/module-manifests/CE-A15.json)

### Data record

| Field | Type/constraint |
|---|---|
| `prep_id` | `string` |
| `source_lineage` | `ref` |
| `approval_refs` | `list` |
| `organism` | `enum` |
| `region` | `string` |
| `geometry` | `enum` |
| `batch` | `string` |
| `received_at` | `UTC` |
| `imaging_map_ref` | `string` |
| `functional_release` | `object` |
| `quality_review` | `enum` |

### Processing contract

1. Create an institution-approved source and consent record, named biological lead and preparation version; block receiving if required documents are absent.
2. Capture chamber geometry, optically accessible regions, electrode map, clock synchronization, source identity and sample-level provenance.
3. Track viable composition and longitudinal morphology as measurements from qualified assays, not as instructions for inducing cell types.
4. Check structural/interface stability with the preparation-specific lab criteria, then verify experimentally distinguishable inputs and responses.
5. Keep source-expansion/differentiation service separate from mature learning; physical additions or joining are new research versions.
6. Publish only measured observation coverage, released limits, instrument compatibility and unresolved parameters; never infer cognition from size, symmetry or marker intensity.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A15.schema.json)

---

## CE-A16: Roots Registry, Engram Hypothesis Tracking and Growth Maps

**Implementation track:** `digital-first + measured research` · **Primary record:** `RootRecord` · [Original manifest](../registries/v6/module-manifests/CE-A16.json)

### Data record

| Field | Type/constraint |
|---|---|
| `root_id` | `string` |
| `task_relation` | `string` |
| `learner_id` | `string` |
| `context` | `list` |
| `acquisition_run` | `id` |
| `reader_digest` | `sha256` |
| `delay_scores` | `dict` |
| `protected` | `boolean` |
| `evidence_links` | `list` |
| `graph_edges` | `list` |
| `access_routes` | `list` |
| `status` | `enum` |

### Processing contract

1. Create a Root candidate only from an acquisition run with a locked task, source IDs and valid response.
2. Store absolute delayed test outcomes and variability; mark unavailable/undefined ratios instead of pretending a score is zero.
3. Attach observed channels and optional image regions as ASSOCIATED_WITH edges, not causal or storage links.
4. Generate two alternative cues and separate retrieval tests to discriminate access-path failure from loss of performance; unresolved is allowed.
5. Run reversible digital edge masking; report task changes across a prespecified protected skill battery, retaining deleted edge provenance.
6. Promote VERIFIED_ROOT only after declared delay and independent confirmation; freeze root status during downstream compilation.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A16.schema.json)

---

## CE-A17: Daily Milk, Water and Support-Dispensing Operations

**Implementation track:** `qualified-lab support + digital scheduler` · **Primary record:** `CareServiceReceipt` · [Original manifest](../registries/v6/module-manifests/CE-A17.json)

### Data record

| Field | Type/constraint |
|---|---|
| `service_id` | `uuid` |
| `prep_id` | `string` |
| `approved_SOP_ref` | `string` |
| `requested_at` | `UTC` |
| `due_window` | `object` |
| `responsible_operator` | `string` |
| `supply_lot` | `string` |
| `measurements_before` | `dict` |
| `authorized` | `boolean` |
| `execution_reference` | `string?` |
| `measurements_after` | `dict` |
| `deviation` | `string?` |

### Processing contract

1. Import site-approved service intervals and task-independent support requirements; validation rejects missing units, out-of-date SOP and incompatible module identity.
2. Derive due windows and conflicts across modules without inventing volumes, compositions, concentrations or physical flow.
3. Request operator authorization and inventory allocation; log approved supplies and lot identifiers.
4. Accept an externally verified delivered-service receipt, preserving intended/delivered timestamps and sampling path.
5. Observe freshness and deviations; escalate unresolved faults to the responsible laboratory team.
6. Continue approved baseline care during task pause and keep all care events out of training reward computation.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A17.schema.json)

---

## CE-A18: Bread, Meat, Water and Metabolic Provision Network

**Implementation track:** `dual-lane: digital curricula + lab-governed biological care` · **Primary record:** `ProvisionEvent` · [Original manifest](../registries/v6/module-manifests/CE-A18.json)

### Data record

| Field | Type/constraint |
|---|---|
| `event_id` | `uuid` |
| `lane` | `enum[digital,physical_record]` |
| `category` | `enum` |
| `content_or_SOP_ref` | `string` |
| `recipient_id` | `string` |
| `reason` | `string` |
| `rights_or_approval` | `string` |
| `delivered_receipt` | `string?` |
| `evaluation_id` | `string?` |

### Processing contract

1. Determine the requested lane and refuse any conversion from a digital lesson to a chemical actuation event.
2. For a digital provision, resolve prerequisite Roots, source licenses, target reader and sealed assessment.
3. Route educational content through WindBus with explicit intended recipient and provide feedback only after task outcome closure.
4. Record changes to digital competency and root retention; do not use memory score as a nutritional dose proxy.
5. For physical-care metadata, accept only a lab-issued approved formulation/version reference and a qualified service receipt.
6. On apparent support effect, compare instrument validity and general excitability before claiming selective teaching or learned knowledge.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A18.schema.json)

---

## CE-A19: Central Data Router, Host and Internet Integration

**Implementation track:** `digital-first` · **Primary record:** `IngressAsset` · [Original manifest](../registries/v6/module-manifests/CE-A19.json)

### Data record

| Field | Type/constraint |
|---|---|
| `asset_id` | `sha256` |
| `source_uri` | `string` |
| `original_digest` | `sha256` |
| `received_at` | `UTC` |
| `provenance` | `object` |
| `license` | `object` |
| `user_scope` | `string` |
| `content_type` | `MIME` |
| `parse_version` | `string` |
| `trust` | `enum` |
| `decision` | `enum` |

### Processing contract

1. Allow user-initiated imports or explicit scoped connectors only; enforce domain allowlist, size budgets, MIME sniffing and no untrusted redirects.
2. Save original source bytes and digest in quarantine before parsing; sandbox document conversion without executing embedded macros/scripts.
3. Attach origin, timestamp, rights, intended recipients, consent and sensitivity to every normalized record.
4. Publish a content reference on a versioned typed bus, never an unsanitized command payload.
5. Build a selective extraction layer for project-specific knowledge and allow the user to approve any exportable Crystal source.
6. When a source changes or consent is withdrawn, retract active indexes and trigger dependent packs for requalification.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A19.schema.json)

---

## CE-A20: Filesystem StarChip, Versioned Knowledge and Scheduled Crystallization

**Implementation track:** `digital-first` · **Primary record:** `FilesystemSnapshot` · [Original manifest](../registries/v6/module-manifests/CE-A20.json)

### Data record

| Field | Type/constraint |
|---|---|
| `snapshot_id` | `sha256` |
| `parent_snapshot` | `string?` |
| `root_manifest` | `sha256` |
| `project_scope` | `string` |
| `policy_digest` | `sha256` |
| `file_count` | `uint64` |
| `total_bytes` | `uint64` |
| `created_at` | `UTC` |
| `crystallization_job` | `string?` |
| `verified` | `boolean` |

### Processing contract

1. Normalize relative paths, reject traversal and reserved names, enforce per-project namespace and content quotas.
2. Write immutable chunk/blob objects by digest and update directory snapshots transactionally.
3. Maintain tags and vector/lexical search indexes as derived caches with rebuild logs; never mutate original objects to satisfy a semantic guess.
4. Use a snapshot policy triggered by time, meaningful content changes or explicit request; deduplicate hashes and capture a deterministic manifest.
5. Construct a Crystal build request from consented objects only and include permission-provenance root and dependency list.
6. Verify export against source, restore a snapshot independently and test that revocations invalidate derived indexes and future pack builds.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A20.schema.json)

---

## CE-A21: Supportive and Affective Regulation, Honey and Needs Network

**Implementation track:** `digital-first + hypothesis for tissue states` · **Primary record:** `SupportInteraction` · [Original manifest](../registries/v6/module-manifests/CE-A21.json)

### Data record

| Field | Type/constraint |
|---|---|
| `interaction_id` | `uuid` |
| `need_category` | `enum` |
| `explicitly_reported_by_user` | `boolean` |
| `user_preference_ref` | `string?` |
| `policy_version` | `string` |
| `proposed_response` | `string` |
| `consent_scope` | `string` |
| `outcome_rating` | `float?` |
| `distress_inference` | `enum[none,unverified]` |

### Processing contract

1. Receive an explicit need or safe task signal; do not infer a mental-health diagnosis from observed activity.
2. Select supportive responses from user-configured options, e.g., explanation pacing, acknowledgment, interruption reduction or task decomposition.
3. Apply a consent gate before storing a preference or using it for model adaptation.
4. Measure usefulness through volunteered feedback and task completion, not unverifiable claims that tissue feels rewarded.
5. Keep ordinary supportive language separate from training labels and biological support processes.
6. Refresh or forget a preference on user instruction, and preserve auditable changes to active policy.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A21.schema.json)

---

## CE-A22: Independent Limit Controller, Governance and Evidence

**Implementation track:** `digital-first + mandatory lab controls` · **Primary record:** `PermitReceipt` · [Original manifest](../registries/v6/module-manifests/CE-A22.json)

### Data record

| Field | Type/constraint |
|---|---|
| `receipt_id` | `uuid` |
| `action_id` | `uuid` |
| `actor` | `string` |
| `module` | `string` |
| `action_type` | `string` |
| `approval_scope` | `string` |
| `policy_digest` | `sha256` |
| `inputs_valid` | `boolean` |
| `state` | `enum` |
| `reason_code` | `string` |
| `authorized_by` | `string?` |
| `timestamp` | `UTC` |

### Processing contract

1. Enumerate allowed actions and risk tiers with default DENY for unknown instructions or undefined physical parameters.
2. Verify actor identity and authorization, current lab release, fresh sensors and valid signed policy version.
3. Evaluate safety and protected Root constraints before performing any utility ranking.
4. Return an immutable permit/veto receipt; a permit is specific to requested action, target and conditions and cannot be replayed.
5. For software changes, stage and test, preserve rollback snapshot and independently check effective permissions.
6. For living research, allow only device-vendor-supported verified interlocks under site procedure; simulation code contains no live stimulation or pump driver.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A22.schema.json)

---

## CE-A23: Physical Crystal Candidates, Molecular Words and Peptides

**Implementation track:** `research-only` · **Primary record:** `CrystalCandidate` · [Original manifest](../registries/v6/module-manifests/CE-A23.json)

### Data record

| Field | Type/constraint |
|---|---|
| `candidate_id` | `string` |
| `preparation_id` | `string` |
| `carrier_class` | `enum` |
| `physical_boundary` | `string` |
| `learned_relation` | `string` |
| `matched_histories` | `list` |
| `receiver_contract` | `string` |
| `retention_horizon` | `string` |
| `observed_measurements` | `list` |
| `evidence_tier` | `enum` |
| `alternative_explanations` | `list` |
| `portability_claim` | `boolean` |

### Processing contract

1. Specify the claimed learned discrimination and frozen reader before measuring carrier properties.
2. Catalog verified molecular identity/compartment and receptor context separately from observed network behavior; candidate classes include proBDNF processing, TrkB/SorCS2 interactions, astrocyte state, PSD scaffolds and ECM changes.
3. Record a physical boundary, quantifiable packing measure, context and maintenance requirements for each candidate.
4. Compare trained histories with matched untrained, activity-matched and alternative-context controls; delayed selective performance is required for content claims.
5. Use licensed laboratory measurements to test association; causality and portability are separate, stronger steps requiring qualified studies.
6. Block pack installation or food dispensing from any CrystalCandidate claim until a verified biophysical input/reader contract and institutional approval exist.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A23.schema.json)

---

## CE-A24: Observer Mesh and Monitoring Communities

**Implementation track:** `digital-first + measured research` · **Primary record:** `ObserverPrediction` · [Original manifest](../registries/v6/module-manifests/CE-A24.json)

### Data record

| Field | Type/constraint |
|---|---|
| `prediction_id` | `uuid` |
| `observer_id` | `string` |
| `source_preparation` | `string` |
| `feature_cutoff` | `UTC` |
| `future_horizon` | `ms` |
| `feature_digest` | `sha256` |
| `missingness_mask` | `object` |
| `predicted_class` | `enum` |
| `calibrated_probability` | `float?` |
| `observed_outcome_ref` | `string?` |
| `advice_type` | `enum` |

### Processing contract

1. Define observation units and immutable sensor cutoff time; remove response outcome and subsequent correction data from O2 features.
2. Train O1 on nominal, route loss, support review and UNKNOWN with synthetic fault labels and independent episode splits.
3. Train O2 against prospective future failures with time-forward holdout and calibrated uncertainty; count invalid as invalid, not correct abstentions.
4. Compare observer results to fixed rules and equally informed digital models, then confirm across batches/preparations.
5. Stage advice in shadow mode; introduce approved routing or task deferral only after measured predictive value, with L veto untouched.
6. Estimate marginal intervention benefit against fixed schedules and action-matched controls, reporting false-alert burden and protected tasks.

[Full design library](../specifications/v3/README.md) · [Schema](../schemas/v6/module_records/CE-A24.schema.json)

---

**Companion evidence note:** These are specification claims and software plans, not completed biological experiments.
