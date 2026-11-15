---
title: "CE-A12 lotus coordinator Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "current"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `docs/design/CE-A12_lotus_coordinator_Design.docx`  
> **SHA-256 (original DOCX):** `1f135a5a7d802682f49cdfeb00e718958659382e8219dcf22c761df3d2b9daeb`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**CE-A12 / DESIGN / 14 NOVEMBER 2026**

Lotus Main Coordinator and Developmental Operating System

Implementation contract, data models, controller behavior and acceptance

| **Track** | **Readiness** | **Source precedence**                                        |
|-----------|---------------|--------------------------------------------------------------|
| DESIGN    | digital-first | Research editions R01–R03 govern conflicting older equations |

# 1. Intended result and scope

Orchestrate the complete learning lifecycle while keeping biological Lotus roles distinct from this software control plane.

Lotus is the host decision coordinator; biological Lotus means local measured coordination and must have a different scoped record. Lotus never changes environmental limits. It reads Needs, Roots, observer predictions and budgets, proposes an action, asks independent L for permission and logs the counterfactual options. E retains independent truth and releases training labels only after response closure.

# 2. System boundary, responsibilities and exclusions

- Coordinate observe-diagnose-plan-stage-evaluate loops across all StarChips

- Enforce dependency, resource and protection constraints through an independent limit gate

- Represent UNRESOLVED as a first-class planning outcome

- Keep a per-decision explanation, evidence references and alternative plan

Inputs: NeedRequest; current module state; protected Roots; available action catalog. Outputs: DevelopmentDecision; staged plan; authorization request; performance receipt.

Dependency interfaces: A22 limit controller, A24 observer, A04 growth, A07 curriculum, A11 WindBus. This module never owns a biological operating limit or a sealed evaluator label unless explicitly identified by a laboratory-approved component.

# 3. Canonical data record and persistence

| **Field**        | **Type and invariant** |
|------------------|------------------------|
| decision_id      | uuid                   |
| state_snapshot   | sha256                 |
| candidates       | list                   |
| expected_effects | vector                 |
| protected_margin | vector                 |
| uncertainty      | object                 |
| ranking_policy   | string                 |
| veto_receipt     | string                 |
| result           | enum                   |
| review_due       | UTC?                   |

Canonical event rule: include schema version, producer, source hash, timestamp, permission scope, correlation ID and immutable lineage. Write primary records before making downstream decisions; an index, dashboard or generated summary is not source truth. Unknown and missing values remain distinguishable.

# 4. Processing and state transitions

Declared state sequence: OBSERVE → DIAGNOSE → PROPOSE → VETO_CHECK → STAGE → EVALUATE → PROMOTE_OR_HOLD. Treat each stage as an idempotent transition with a recorded request and receipt; on failure, preserve the last qualified state and the reason.

1\. Assemble a state vector from fresh, permissioned observations and explicit missingness.

2\. Enumerate allowable actions: NO_CHANGE, DEFER_TASK, REQUEST_REVIEW, approved digital alternate route, replay in staging, snapshot, or curriculum evaluation.

3\. Eliminate actions that violate hard constraints or protected Root noninferiority margins before ranking.

4\. Use a declared scalar utility for eligible digital actions with uncertainty; if confidence intervals overlap zero choose UNRESOLVED or gather additional evidence.

5\. Record proposal, alternatives, decision inputs, independent veto and executed receipt as separate events.

6\. Re-evaluate actions on sealed outcomes; change the planner only via versioned offline deployment, never recursive self-approval.

# 5. Reference algorithm and executable design

Estimate each action benefit vector and uncertainty; use a declared scalar ranking only after hard per-competency constraints; route physical actions to a separately approved controller, never direct autonomous operation.

def process_a12(request, registry, ledger, evaluator, limits):

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

Increasing training time raises immediate accuracy but threatens another Root. Lotus holds the action despite positive weighted gain; observer requests an alternate curriculum that must pass protected regression tests.

# 7. Implementation work and integration order

Phase 1: Implement explicit state machine and decision log. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 2: Add constrained vector-score evaluator. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 3: Simulate veto, UNRESOLVED and recovery branches. Definition of done: record a passing test, an immutable artifact digest and the named owning module.

Phase 4: integrate with WindBus, Lotus decisions, provenance and failure receipt; simulate adverse input and restart before any runtime promotion. Phase 5: publish versioned capability and performance certificate, explicitly digital-only or qualified-laboratory research.

# 8. Acceptance tests and failure response

| **Required test or failure** | **Required observable evidence**                   |
|------------------------------|----------------------------------------------------|
| 1                            | Safety veto cannot be overruled by utility         |
| 2                            | Plans never run with missing parameter bounds      |
| 3                            | Every action links to independent outcome evidence |
| R1                           | Uncalibrated reward may prioritize false metrics   |
| R2                           | Recursive self-approval is forbidden               |
| R3                           | Biological Lotus is not the host scheduler         |

Report these metrics separately (do not replace with a composite score): Validated capability improvement, protected-task noninferiority, decision regret, veto compliance, unresolved fraction, cost per qualified action.

# 9. Scientific and policy boundary

The word Lotus can name local physiological coordination or host governance when scale and mechanism are explicit. No single biological coordinator has been proven.

Physical interventions, culture production, medium formulation, stimulation envelopes, peptide treatments and experimental material transfer are not executed by the digital reference runtime. Those depend on the separate institutional approval, source identification, qualified equipment and safety envelopes documented in research source R02 and predecessor R04.

# 10. Sources and status

Original design trace: CE-DEV-001 §§17,22; Living Computer Theory Set §§7,15; Formal Models §Lotus scalar correction.

Source set: R01–R03 are the latest supplied synthesis and mathematical audit; R04 is the physical device predecessor. Current measurements: none generated by this document. Software examples and planned acceptance thresholds are proposals until executed and reported.

