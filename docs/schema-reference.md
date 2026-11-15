# Complete JSON schema dictionary

> **ChristEye Anaiero · 15 November 2026 · We Develop Software LLC**  
> **Status:** Research design · unvalidated biological proposal · WDS Public Research Reference License.  
> [Documentation Home](index.md) · [Full document library](full-document-library.md) · [Phase P01 original](../phases/P01/README.md)

Browse the original JSON contract files and their required-field definitions. These are the actual system design schemas supplied in the v6 release, not illustrative one-page summaries.

**Validation status:** Schemas define structure; a file passing JSON Schema is not a validated biological device or a correct learned capability.

## [activation_job.schema.json](../schemas/v6/activation_job.schema.json)

**Record:** ActivationJob  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `schema_version` | constant = 1.0.0 | yes |  |
| `job_id` | string | yes |  · min length: 1 |
| `pack_id` | string | yes |  · min length: 1 |
| `target_module` | string | yes |  · pattern: `^CE-(P\\|D\\|R)\d{2}$` |
| `state` | enum · DISCOVER, VERIFY, STAGE, DRY_RUN, SHADOW, CALIBRATE, ASSESS, RELEASE, MONITOR, BLOCKED, FAILED | yes |  |
| `required_checks` | array | yes |  |
| `completed_checks` | array | yes |  |
| `release_allowed` | boolean | yes |  |
| `reason` | string | no |  |

## [anaiero_crystal_v2.schema.json](../schemas/v6/anaiero_crystal_v2.schema.json)

**Record:** Anaiero Digital Crystal v2  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `schema_version` | constant = 2.0.0 | yes |  |
| `pack_id` | string | yes |  · min length: 1 |
| `version` | string | yes |  · min length: 1 |
| `kind` | constant = digital-experience-replay | yes |  |
| `installable` | constant = True | yes |  |
| `permission_mode` | enum · local-only, shareable | yes |  |
| `claims` | array | yes |  |
| `evidence_tier` | enum · D, H, X, F | yes |  |
| `physical_carrier` | constant = False | yes |  |
| `asset` | object | yes |  |
| `source_event_ids` | array | yes |  |
| `readers` | array | yes |  |
| `activation` | array | yes |  |
| `disclaimer` | string | yes |  · min length: 1 |

## [anaiero_growth_request.schema.json](../schemas/v6/anaiero_growth_request.schema.json)

**Record:** Anaiero growth request  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `request_id` | string | yes |  · pattern: `^[A-Za-z0-9._-]{3,90}$` |
| `target_module` | string | yes |  · min length: 1 |
| `missing_capability` | string | yes |  · min length: 1 |
| `alternatives_considered` | array | yes |  |
| `protected_roots` | array | yes |  |
| `expected_gain` | number / null | no |  |
| `evidence_refs` | array | yes |  |
| `operator_review_required` | constant = True | yes |  |

## [anaiero_lotus_decision.schema.json](../schemas/v6/anaiero_lotus_decision.schema.json)

**Record:** Anaiero Lotus decision and limit receipt  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `decision_id` | string | yes |  · pattern: `^[A-Za-z0-9._-]{3,90}$` |
| `action` | string | yes |  · min length: 1 |
| `status` | enum · STAGE_DIGITAL, HOLD, VETO, UNRESOLVED | yes |  |
| `evidence` | string | yes |  · min length: 1 |
| `limit_decision` | enum · permitted-digital, veto, not-reviewed | yes |  |
| `protected_root_delta` | number / null | no |  |
| `physical_action_executed` | constant = False | yes |  |

## [anaiero_root_evidence.schema.json](../schemas/v6/anaiero_root_evidence.schema.json)

**Record:** Anaiero Root evidence record  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `root_id` | string | yes |  · pattern: `^[A-Za-z0-9._-]{3,90}$` |
| `competency` | string | yes |  · min length: 1 |
| `task_version` | string | yes |  · min length: 1 |
| `evidence_tier` | enum · F, X, H, D, A | yes |  |
| `measured_retention` | number / null | no |  |
| `delay_label` | string | yes |  · min length: 1 |
| `source_refs` | array | yes |  |
| `claim_scope` | string | yes |  · min length: 1 |
| `biological_engram_proven` | constant = False | yes |  |

## [anaiero_support_receipt.schema.json](../schemas/v6/anaiero_support_receipt.schema.json)

**Record:** Anaiero lab-issued support receipt  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `module_id` | string | yes |  · pattern: `^[A-Za-z0-9._-]{3,90}$` |
| `service_event_id` | string | yes |  · pattern: `^[A-Za-z0-9._-]{3,90}$` |
| `approved_procedure_ref` | string | yes |  · min length: 1 |
| `performed_by` | string | yes |  · min length: 1 |
| `observations_ref` | string | yes |  · min length: 1 |
| `status` | enum · planned, completed, deviated, escalated | yes |  |
| `no_biological_setpoints_generated_by_learner` | constant = True | yes |  |

## [anaiero_usage_event.schema.json](../schemas/v6/anaiero_usage_event.schema.json)

**Record:** Anaiero UsageEvent v2  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `event_id` | string | yes |  · pattern: `^[A-Za-z0-9._-]{3,90}$` |
| `project_id` | string | yes |  · pattern: `^[A-Za-z0-9._-]{3,90}$` |
| `kind` | enum · task, correction, demonstration, reference, result, preference, revocation | yes |  |
| `content` | string | yes |  · min length: 1 |
| `source_id` | string | yes |  · min length: 1 |
| `scope` | enum · local, shareable, restricted | yes |  |
| `use_for_training` | boolean | yes |  |
| `allow_export` | boolean | yes |  |
| `timestamp` | string | yes |  |
| `parent_id` | string / null | no |  |

## [audit_entry.v1.schema.json](../schemas/p02/audit_entry.v1.schema.json)

**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `previous` | string | yes |  · pattern: `^[0-9a-f]{64}$` |
| `payload` | object | yes |  |
| `entry_hash` | string | yes |  · pattern: `^[0-9a-f]{64}$` |

## [clock_anchor.v1.schema.json](../schemas/p02/clock_anchor.v1.schema.json)

**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `sample` | integer | yes |  · min: 0 |
| `t_ns` | integer | yes |  · min: 0 |

## [crystal_pack.schema.json](../schemas/v6/crystal_pack.schema.json)

**Record:** CrystalPackManifest  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `schema_version` | constant = 1.0.0 | yes |  |
| `pack_id` | string | yes |  · pattern: `^[a-z][a-z0-9.-]{4,100}$` |
| `name` | string | yes |  · min length: 1 |
| `version` | string | yes |  · pattern: `^\d+\.\d+\.\d+([-.][A-Za-z0-9.-]+)?$` |
| `kind` | enum · library, skill, model_adapter, style_policy, tool, composite, physical_candidate, molecular_candidate | yes |  |
| `status` | enum · digital-demo, research-only, qualified-digital | yes |  |
| `installable` | boolean | yes |  |
| `target_modules` | array | yes |  |
| `capabilities` | array | yes |  |
| `assets` | array | yes |  |
| `dependencies` | array | no |  |
| `activation` | object | yes |  |
| `rights` | object | yes |  |
| `evidence` | object | yes |  |
| `safety` | object | yes |  |
| `design_only` | boolean | no |  |

## [evidence_claim.schema.json](../schemas/v6/evidence_claim.schema.json)

**Record:** EvidenceClaim  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `schema_version` | constant = 1.0.0 | yes |  |
| `claim_id` | string | yes |  · min length: 1 |
| `claim` | string | yes |  · min length: 1 |
| `evidence_class` | enum · F, X, H, D | yes |  |
| `status` | enum · proposed, tested, supported, rejected, unresolved | yes |  |
| `evidence_ids` | array | yes |  |
| `counterexample` | string | yes |  · min length: 1 |
| `measurable_endpoint` | string | no |  |
| `confounders` | array | no |  |

## [instrument_capability.v1.schema.json](../schemas/p02/instrument_capability.v1.schema.json)

**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `id` | string | yes |  |
| `mode` | enum · SIMULATED, READ_ONLY_PLACEHOLDER | yes |  |
| `recording_units` | string | yes |  |
| `physical_output_enabled` | constant = False | yes |  |
| `vendor_config_ref` | string / null | no |  |

## [module_records/CE-A01.schema.json](../schemas/v6/module_records/CE-A01.schema.json)

**Record:** EpisodeRecord  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `episode_id` | string | yes | uuid |
| `source_sha256` | string | yes | hex64 |
| `actor_scope` | string | yes | enum |
| `project_id` | string | yes | string |
| `observed_at` | string | yes | UTC |
| `kind` | string | yes | enum |
| `payload_object` | string | yes | sha256 |
| `consent_training` | boolean | yes | boolean |
| `consent_export` | boolean | yes | boolean |
| `causal_parent` | string | no | uuid? |
| `correction_of` | string | no | uuid? |

## [module_records/CE-A02.schema.json](../schemas/v6/module_records/CE-A02.schema.json)

**Record:** DurableObject  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `sha256` | string | yes | hex64 |
| `plaintext_size` | integer | yes | uint64 |
| `media_type` | string | yes | MIME |
| `tenant` | string | yes | string |
| `retention_policy` | string | yes | enum |
| `origin_event_id` | string | yes | uuid |
| `created_at` | string | yes | UTC |
| `encryption_key_id` | string | no | string? |
| `supersedes` | string | no | sha256? |
| `verified_at` | string | no | UTC? |

## [module_records/CE-A03.schema.json](../schemas/v6/module_records/CE-A03.schema.json)

**Record:** RecoveryCase  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `case_id` | string | yes | uuid |
| `onset` | string | yes | UTC |
| `symptom` | string | yes | enum |
| `affected_routes` | array | yes | list |
| `sensor_quality` | object | yes | object |
| `competing_hypotheses` | array | yes | list |
| `selected_action` | string | yes | enum |
| `permit_receipt` | string | no | id? |
| `before_after_test` | string | no | id? |
| `status` | string | yes | enum |

## [module_records/CE-A04.schema.json](../schemas/v6/module_records/CE-A04.schema.json)

**Record:** GrowthProposal  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `proposal_id` | string | yes | uuid |
| `diagnosed_gap` | string | yes | string |
| `comparator_results` | array | yes | list |
| `resource_budget` | object | yes | object |
| `candidate_module` | string | yes | string |
| `expected_gain_vector` | object | yes | object |
| `uncertainty` | object | yes | object |
| `protected_roots` | array | yes | list |
| `permit` | string | yes | enum |
| `observed_gain` | object | no | object? |

## [module_records/CE-A05.schema.json](../schemas/v6/module_records/CE-A05.schema.json)

**Record:** RefreshPlan  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `plan_id` | string | yes | uuid |
| `target_id` | string | yes | string |
| `last_validated` | string | yes | UTC |
| `age` | string | yes | seconds |
| `stale_dependencies` | array | yes | list |
| `action` | string | yes | enum |
| `snapshot_before` | string | yes | sha256 |
| `outcome_receipt` | string | yes | uuid |
| `followup_due` | string | yes | UTC |

## [module_records/CE-A06.schema.json](../schemas/v6/module_records/CE-A06.schema.json)

**Record:** RetentionStudy  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `study_id` | string | yes | uuid |
| `capability_id` | string | yes | string |
| `participant_id` | string | yes | string |
| `training_epoch` | string | yes | string |
| `probe_schedule` | array | yes | list |
| `holdout_digest` | string | yes | sha256 |
| `reader_digest` | string | yes | sha256 |
| `immediate` | number | yes | float |
| `delayed` | object | yes | dict |
| `controls` | array | yes | list |
| `status` | string | yes | enum |

## [module_records/CE-A07.schema.json](../schemas/v6/module_records/CE-A07.schema.json)

**Record:** CurriculumUnit  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `unit_id` | string | yes | string |
| `domain` | string | yes | string |
| `prerequisites` | array | yes | list |
| `content_digest` | string | yes | sha256 |
| `target_skill` | string | yes | string |
| `format` | string | yes | enum |
| `learner_reader` | string | yes | string |
| `assessment_digest` | string | yes | sha256 |
| `mastery_rule` | object | yes | object |
| `copyright_scope` | string | yes | string |

## [module_records/CE-A08.schema.json](../schemas/v6/module_records/CE-A08.schema.json)

**Record:** HealthAlert  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `alert_id` | string | yes | uuid |
| `module_id` | string | yes | string |
| `sensor_id` | string | yes | string |
| `observed_at` | string | yes | UTC |
| `validity` | string | yes | enum |
| `threshold_ref` | string | yes | string |
| `deviation` | object | yes | object |
| `confidence` | number | no | float? |
| `category` | string | yes | enum |
| `required_review` | string | yes | string |
| `receipt` | string | no | id? |

## [module_records/CE-A09.schema.json](../schemas/v6/module_records/CE-A09.schema.json)

**Record:** BiologicalTrial  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `trial_id` | string | yes | uuid |
| `preparation_id` | string | yes | string |
| `task_codebook_version` | string | yes | string |
| `requested_event` | string | yes | id |
| `delivered_event` | string | no | id? |
| `raw_record_ref` | string | yes | string |
| `mask_digest` | string | yes | sha256 |
| `decoder_digest` | string | yes | sha256 |
| `response` | string | yes | enum |
| `validity` | string | yes | enum |
| `evaluator_label` | string | no | enum? |
| `feedback_receipt` | string | no | id? |

## [module_records/CE-A10.schema.json](../schemas/v6/module_records/CE-A10.schema.json)

**Record:** CrystalManifest  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `pack_id` | string | yes | string |
| `semver` | string | yes | string |
| `kind` | string | yes | enum |
| `producer` | string | yes | string |
| `build_digest` | string | yes | sha256 |
| `artifacts` | array | yes | list[digest] |
| `lessons` | array | yes | list |
| `memory_roots` | array | yes | list |
| `license` | object | yes | object |
| `reader_contract` | string | yes | string |
| `tests` | object | yes | object |
| `evidence_tier` | string | yes | enum |
| `installable` | boolean | yes | boolean |

## [module_records/CE-A11.schema.json](../schemas/v6/module_records/CE-A11.schema.json)

**Record:** WindEnvelope  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `event_id` | string | yes | uuid |
| `trace_id` | string | yes | uuid |
| `source_id` | string | yes | string |
| `recipient_id` | string | yes | string |
| `schema_id` | string | yes | string |
| `version` | string | yes | semver |
| `kind` | string | yes | enum |
| `observed_at` | string | yes | UTC |
| `issued_at` | string | yes | UTC |
| `expires_at` | string | yes | UTC |
| `payload_ref` | string | yes | sha256 |
| `source_scope` | string | yes | enum |
| `signature` | string | no | bytes? |

## [module_records/CE-A12.schema.json](../schemas/v6/module_records/CE-A12.schema.json)

**Record:** LotusDecision  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `decision_id` | string | yes | uuid |
| `state_snapshot` | string | yes | sha256 |
| `candidates` | array | yes | list |
| `expected_effects` | string | yes | vector |
| `protected_margin` | string | yes | vector |
| `uncertainty` | object | yes | object |
| `ranking_policy` | string | yes | string |
| `veto_receipt` | string | yes | string |
| `result` | string | yes | enum |
| `review_due` | string | no | UTC? |

## [module_records/CE-A13.schema.json](../schemas/v6/module_records/CE-A13.schema.json)

**Record:** CloneReceipt  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `parent_pack` | string | yes | sha256 |
| `child_pack` | string | yes | sha256 |
| `version` | string | yes | string |
| `copy_mode` | string | yes | enum |
| `authorizer` | string | yes | string |
| `rights_audit` | string | yes | sha256 |
| `reader` | string | yes | string |
| `clone_tests` | object | yes | object |
| `isolated_test` | object | yes | object |
| `created_at` | string | yes | UTC |

## [module_records/CE-A14.schema.json](../schemas/v6/module_records/CE-A14.schema.json)

**Record:** ConsumptionJob  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `job_id` | string | yes | uuid |
| `pack_digest` | string | yes | sha256 |
| `target_reader` | string | yes | string |
| `mode` | string | yes | enum |
| `state` | string | yes | enum |
| `replay_cursor` | integer | yes | uint64 |
| `consent_snapshot` | string | yes | sha256 |
| `assessment_set` | string | yes | sha256 |
| `rollback_checkpoint` | string | yes | sha256 |
| `activation_receipt` | string | no | id? |

## [module_records/CE-A15.schema.json](../schemas/v6/module_records/CE-A15.schema.json)

**Record:** PreparationVersion  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `prep_id` | string | yes | string |
| `source_lineage` | string | yes | ref |
| `approval_refs` | array | yes | list |
| `organism` | string | yes | enum |
| `region` | string | yes | string |
| `geometry` | string | yes | enum |
| `batch` | string | yes | string |
| `received_at` | string | yes | UTC |
| `imaging_map_ref` | string | yes | string |
| `functional_release` | object | yes | object |
| `quality_review` | string | yes | enum |

## [module_records/CE-A16.schema.json](../schemas/v6/module_records/CE-A16.schema.json)

**Record:** RootRecord  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `root_id` | string | yes | string |
| `task_relation` | string | yes | string |
| `learner_id` | string | yes | string |
| `context` | array | yes | list |
| `acquisition_run` | string | yes | id |
| `reader_digest` | string | yes | sha256 |
| `delay_scores` | object | yes | dict |
| `protected` | boolean | yes | boolean |
| `evidence_links` | array | yes | list |
| `graph_edges` | array | yes | list |
| `access_routes` | array | yes | list |
| `status` | string | yes | enum |

## [module_records/CE-A17.schema.json](../schemas/v6/module_records/CE-A17.schema.json)

**Record:** CareServiceReceipt  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `service_id` | string | yes | uuid |
| `prep_id` | string | yes | string |
| `approved_SOP_ref` | string | yes | string |
| `requested_at` | string | yes | UTC |
| `due_window` | object | yes | object |
| `responsible_operator` | string | yes | string |
| `supply_lot` | string | yes | string |
| `measurements_before` | object | yes | dict |
| `authorized` | boolean | yes | boolean |
| `execution_reference` | string | no | string? |
| `measurements_after` | object | yes | dict |
| `deviation` | string | no | string? |

## [module_records/CE-A18.schema.json](../schemas/v6/module_records/CE-A18.schema.json)

**Record:** ProvisionEvent  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `event_id` | string | yes | uuid |
| `lane` | string | yes | enum[digital,physical_record] |
| `category` | string | yes | enum |
| `content_or_SOP_ref` | string | yes | string |
| `recipient_id` | string | yes | string |
| `reason` | string | yes | string |
| `rights_or_approval` | string | yes | string |
| `delivered_receipt` | string | no | string? |
| `evaluation_id` | string | no | string? |

## [module_records/CE-A19.schema.json](../schemas/v6/module_records/CE-A19.schema.json)

**Record:** IngressAsset  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `asset_id` | string | yes | sha256 |
| `source_uri` | string | yes | string |
| `original_digest` | string | yes | sha256 |
| `received_at` | string | yes | UTC |
| `provenance` | object | yes | object |
| `license` | object | yes | object |
| `user_scope` | string | yes | string |
| `content_type` | string | yes | MIME |
| `parse_version` | string | yes | string |
| `trust` | string | yes | enum |
| `decision` | string | yes | enum |

## [module_records/CE-A20.schema.json](../schemas/v6/module_records/CE-A20.schema.json)

**Record:** FilesystemSnapshot  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `snapshot_id` | string | yes | sha256 |
| `parent_snapshot` | string | no | string? |
| `root_manifest` | string | yes | sha256 |
| `project_scope` | string | yes | string |
| `policy_digest` | string | yes | sha256 |
| `file_count` | integer | yes | uint64 |
| `total_bytes` | integer | yes | uint64 |
| `created_at` | string | yes | UTC |
| `crystallization_job` | string | no | string? |
| `verified` | boolean | yes | boolean |

## [module_records/CE-A21.schema.json](../schemas/v6/module_records/CE-A21.schema.json)

**Record:** SupportInteraction  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `interaction_id` | string | yes | uuid |
| `need_category` | string | yes | enum |
| `explicitly_reported_by_user` | boolean | yes | boolean |
| `user_preference_ref` | string | no | string? |
| `policy_version` | string | yes | string |
| `proposed_response` | string | yes | string |
| `consent_scope` | string | yes | string |
| `outcome_rating` | number | no | float? |
| `distress_inference` | string | yes | enum[none,unverified] |

## [module_records/CE-A22.schema.json](../schemas/v6/module_records/CE-A22.schema.json)

**Record:** PermitReceipt  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `receipt_id` | string | yes | uuid |
| `action_id` | string | yes | uuid |
| `actor` | string | yes | string |
| `module` | string | yes | string |
| `action_type` | string | yes | string |
| `approval_scope` | string | yes | string |
| `policy_digest` | string | yes | sha256 |
| `inputs_valid` | boolean | yes | boolean |
| `state` | string | yes | enum |
| `reason_code` | string | yes | string |
| `authorized_by` | string | no | string? |
| `timestamp` | string | yes | UTC |

## [module_records/CE-A23.schema.json](../schemas/v6/module_records/CE-A23.schema.json)

**Record:** CrystalCandidate  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `candidate_id` | string | yes | string |
| `preparation_id` | string | yes | string |
| `carrier_class` | string | yes | enum |
| `physical_boundary` | string | yes | string |
| `learned_relation` | string | yes | string |
| `matched_histories` | array | yes | list |
| `receiver_contract` | string | yes | string |
| `retention_horizon` | string | yes | string |
| `observed_measurements` | array | yes | list |
| `evidence_tier` | string | yes | enum |
| `alternative_explanations` | array | yes | list |
| `portability_claim` | boolean | yes | boolean |

## [module_records/CE-A24.schema.json](../schemas/v6/module_records/CE-A24.schema.json)

**Record:** ObserverPrediction  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `prediction_id` | string | yes | uuid |
| `observer_id` | string | yes | string |
| `source_preparation` | string | yes | string |
| `feature_cutoff` | string | yes | UTC |
| `future_horizon` | string | yes | ms |
| `feature_digest` | string | yes | sha256 |
| `missingness_mask` | object | yes | object |
| `predicted_class` | string | yes | enum |
| `calibrated_probability` | number | no | float? |
| `observed_outcome_ref` | string | no | string? |
| `advice_type` | string | yes | enum |

## [root_record.schema.json](../schemas/v6/root_record.schema.json)

**Record:** RootRecord  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `schema_version` | constant = 1.0.0 | yes |  |
| `competency_id` | string | yes |  · min length: 1 |
| `module_id` | string | yes |  · pattern: `^CE-(P\\|D\\|R)\d{2}$` |
| `task_version` | string | yes |  · min length: 1 |
| `decoder_digest` | string | yes |  · min length: 1 |
| `evaluation_split` | string | yes |  · min length: 1 |
| `chance_performance` | number | yes |  · min: 0 |
| `immediate_score` | number | yes |  · min: 0 |
| `delayed_scores` | array | yes |  |
| `evidence_class` | enum · F, X, H, D | yes |  |
| `source_ids` | array | no |  |

## [route_acl.v1.schema.json](../schemas/p02/route_acl.v1.schema.json)

**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `source` | string | yes |  |
| `recipient` | string | yes |  |
| `event_types` | array | yes |  |
| `max_bytes` | integer | yes |  · min: 1 |

## [star_chip.schema.json](../schemas/v6/star_chip.schema.json)

**Record:** StarChipManifest  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `schema_version` | constant = 1.0.0 | yes |  |
| `module_id` | string | yes |  · pattern: `^CE-(P\\|D\\|R)\d{2}$` |
| `name` | string | yes |  · min length: 1 |
| `version` | string | yes |  · pattern: `^\d+\.\d+\.\d+([-.][A-Za-z0-9.-]+)?$` |
| `stage` | enum · DIGITAL-V1, LAB-RESEARCH, HYPOTHESIS | yes |  |
| `capabilities` | array | yes |  |
| `ports` | object | yes |  |
| `resources` | object | yes |  |
| `evidence_state` | enum · design, experimental, qualified | no |  |
| `integration_refs` | array | yes |  |
| `boot_dependencies` | array | yes |  |

## [trial_record.v1.schema.json](../schemas/p02/trial_record.v1.schema.json)

**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `trial_id` | string | yes |  |
| `split` | enum · baseline, training, development, confirmation, delayed | yes |  |
| `prediction` | enum · X, Y, HOLD, INVALID | yes |  |
| `valid` | boolean | yes |  |
| `reference_label` | enum · X, Y, HOLD, UNRESOLVED | yes |  |
| `reader_digest` | string | yes |  · pattern: `^[0-9a-f]{64}$` |
| `feedback_released` | boolean | yes |  |
| `learner_digest_after` | string | yes |  · pattern: `^[0-9a-f]{64}$` |

## [wind_envelope.schema.json](../schemas/v6/wind_envelope.schema.json)

**Record:** WindEnvelope  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `schema_version` | constant = 1.0.0 | yes |  |
| `event_id` | string | yes |  |
| `trace_id` | string | yes |  · min length: 1 |
| `source_module` | string | yes |  · pattern: `^CE-(P\\|D\\|R)\d{2}$` |
| `target_module` | string | yes |  · pattern: `^CE-(P\\|D\\|R)\d{2}$` |
| `capability` | string | yes |  · min length: 1 |
| `issued_at` | string | yes |  |
| `validity` | enum · valid, invalid, unresolved, expired | yes |  |
| `payload` | unspecified | yes |  |
| `provenance` | array | yes |  |
| `confidence` | number | no |  · min: 0 |
| `expires_at` | string | no |  |

## [wind_envelope.v1.schema.json](../schemas/p02/wind_envelope.v1.schema.json)

**Record:** P02 WindEnvelope v1 (software semantics)  
**Schema draft:** https://json-schema.org/draft/2020-12/schema  

| Field | Type or allowed enum | Required | Meaning or constraint |
|---|---|---|---|
| `event_id` | string | yes |  · min length: 1 |
| `trace_id` | string | yes |  · min length: 1 |
| `source` | string | yes |  · min length: 1 |
| `recipient` | string | yes |  · min length: 1 |
| `event_type` | enum · TASK_CUE, TASK_RESULT, OBSERVATION, TRAINING_FEEDBACK, SUPPORT_PROPOSAL, DECISION, RECEIPT, HEARTBEAT, FAULT, SYNC_ANCHOR | yes |  |
| `sequence` | integer | yes |  · min: 0 |
| `emitted_ns` | integer | yes |  · min: 0 |
| `deadline_ns` | integer | yes |  · min: 1 |
| `payload` | object | yes |  |
| `consent_scope` | enum · local, restricted, shareable | yes |  |
| `schema_version` | constant = 1.0.0 | yes |  |
| `clock_domain` | constant = sim-monotonic | yes |  |
| `parent_id` | string / null | yes |  |
| `producer_digest` | string | yes |  |

## Integration rules

Every WindEvent must have an identified source/recipient/codebook, protected-test data remains outside learner inputs, physical outputs require separately released limits, and a digital Crystal does not claim a physical biochemical memory. [WindBus](windbus.md) · [Physical modules](phase-1.md).
