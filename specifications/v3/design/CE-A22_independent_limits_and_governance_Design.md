---
title: "CE-A22 independent limits and governance Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "current"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `docs/design/CE-A22_independent_limits_and_governance_Design.docx`  
> **SHA-256 (original DOCX):** `a1d3a211ae82d81c42ba5ace7f1278ec7583b9b528a62404d1b01138028eb52c`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**CE-A22 / DESIGN / 14 NOVEMBER 2026**

Independent Limit Controller, Governance and Evidence

Implementation contract, data models, controller behavior and acceptance

| **Track** | **Readiness**                          | **Source precedence**                                        |
|-----------|----------------------------------------|--------------------------------------------------------------|
| DESIGN    | digital-first + mandatory lab controls | Research editions R01–R03 govern conflicting older equations |

# 1. Intended result and scope

Provide non-negotiable protection, audit and claim integrity across every module regardless of a proposed Lotus benefit.

An independently owned Limit L gates all physical or high-risk actions. The digital Lotus requests; L evaluates current calibration, policy, action type and operator authorization. The model never edits its own hard limits. Evidence E holds sealed truth, claims and source links under a separate principal. Biological Heart remains the response-forming organization, not L.

# 2. System boundary, responsibilities and exclusions

- Keep independent safety controller L separate from biological Heart

- Enforce signed policy, allowed actions, missingness and freshness requirements

- Prevent research-only packs from being executed or materialized through ordinary installer

- Maintain F/X/H/D/A evidence provenance and immutable decisions

Inputs: ActionRequest; route identity; resource status; evidence status; permission token. Outputs: Permit or veto; reason; audit trail; escalation receipt; claim record.

Dependency interfaces: A12 Lotus, A11 WindBus, A17 care, A08 health, A14 consumer. This module never owns a biological operating limit or a sealed evaluator label unless explicitly identified by a laboratory-approved component.

# 3. Canonical data record and persistence

| **Field**      | **Type and invariant** |
|----------------|------------------------|
| receipt_id     | uuid                   |
| action_id      | uuid                   |
| actor          | string                 |
| module         | string                 |
| action_type    | string                 |
| approval_scope | string                 |
| policy_digest  | sha256                 |
| inputs_valid   | boolean                |
| state          | enum                   |
| reason_code    | string                 |
| authorized_by  | string?                |
| timestamp      | UTC                    |

Canonical event rule: include schema version, producer, source hash, timestamp, permission scope, correlation ID and immutable lineage. Write primary records before making downstream decisions; an index, dashboard or generated summary is not source truth. Unknown and missing values remain distinguishable.

# 4. Processing and state transitions

Declared state sequence: CHECK_IDENTITY → CHECK_POLICY → CHECK_FRESHNESS → CHECK_BOUND → PERMIT_OR_VETO → LOG → ESCALATE. Treat each stage as an idempotent transition with a recorded request and receipt; on failure, preserve the last qualified state and the reason.

1\. Enumerate allowed actions and risk tiers with default DENY for unknown instructions or undefined physical parameters.

2\. Verify actor identity and authorization, current lab release, fresh sensors and valid signed policy version.

3\. Evaluate safety and protected Root constraints before performing any utility ranking.

4\. Return an immutable permit/veto receipt; a permit is specific to requested action, target and conditions and cannot be replayed.

5\. For software changes, stage and test, preserve rollback snapshot and independently check effective permissions.

6\. For living research, allow only device-vendor-supported verified interlocks under site procedure; simulation code contains no live stimulation or pump driver.

# 5. Reference algorithm and executable design

Fail closed for unqualified physical control; allow normal approved care independently of task-level pause; reject any model-generated override of independently owned limits.

def process_a22(request, registry, ledger, evaluator, limits):

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

A language model asks to change fluid composition after a recall error. L rejects the unsupported action; it may issue REQUEST_LAB_REVIEW, which is not a chemical delivery event.

# 7. Implementation work and integration order

Phase 1: Implement permission-check library. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 2: Add research-pack no-execute tests. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 3: Build evidence ledger with change audit. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 4: integrate with WindBus, Lotus decisions, provenance and failure receipt; simulate adverse input and restart before any runtime promotion. Phase 5: publish versioned capability and performance certificate, explicitly digital-only or qualified-laboratory research.

# 8. Acceptance tests and failure response

| **Required test or failure** | **Required observable evidence**                                            |
|------------------------------|-----------------------------------------------------------------------------|
| 1                            | Research-only molecular and physical Crystal packages are never installable |
| 2                            | No utility score overrides veto                                             |
| 3                            | Safety and capability claims have separate release status                   |
| R1                           | Model self-edit might bypass oversight                                      |
| R2                           | Unregistered paths can defeat limits                                        |
| R3                           | Biological rollback is not guaranteed                                       |

Report these metrics separately (do not replace with a composite score): Policy violations blocked, unauthorized actions, coverage of stop rules, provenance completeness, false permit rate.

# 9. Scientific and policy boundary

Human-derived material, privacy/consent, institutional oversight and site-approved operating procedures are required before living studies. No medical claims.

Physical interventions, culture production, medium formulation, stimulation envelopes, peptide treatments and experimental material transfer are not executed by the digital reference runtime. Those depend on the separate institutional approval, source identification, qualified equipment and safety envelopes documented in research source R02 and predecessor R04.

# 10. Sources and status

Original design trace: CE-DEV-001 §22, §27; CE-CRYSTAL-SYS-001 §Architecture and R04/R10/R12.

Source set: R01–R03 are the latest supplied synthesis and mathematical audit; R04 is the physical device predecessor. Current measurements: none generated by this document. Software examples and planned acceptance thresholds are proposals until executed and reported.

