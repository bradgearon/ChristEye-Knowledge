---
title: "P02-04 Independent Limits and Lotus Controller Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02_Cell_Free_Electronics_and_WindBus/docs/design/P02-04_Independent_Limits_and_Lotus_Controller_Design.docx`  
> **SHA-256 (original DOCX):** `760166a9a31698321d5cb4f4b6fb7e5bd62fe260fb7e6fb55a31c3a6f12f9e0e`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Revision:** 1.0 \| **Owner:** safety systems lead \| **Runtime:** controller.py \| **Research mode:** no physical outputs

# 1. Roles and hard separation

Lotus coordinates when to teach, observe, defer, consider another route or request review. It does not act as the physical life-support controller, and it cannot override L. IndependentLimitController evaluates independent sensor freshness, clock quality, storage reserve, watchdog state, permitted logical action, and instrument state before allowing even a simulated task. The biological Heart (response-forming organization) and the external L are distinct role assignments. That distinction is preserved from the Physical Learning Device design and the later scientific reconciliation. \[I01, I02\]

# 2. Authority matrix

| **Actor**           | **May propose**                                                     | **May authorize**                            | **Must never do in P02**                                          |
|---------------------|---------------------------------------------------------------------|----------------------------------------------|-------------------------------------------------------------------|
| Lotus/H             | Training schedule, review/defer/approved route proposal             | Logical session state changes after L permit | Direct electrical output, alter L limit table, dispense chemistry |
| O Observer          | Predicted normal, missing input, route issue, uncertainty           | None                                         | Operate a pump, declare task truth, rewrite decoder               |
| S Support policy    | REQUEST_REVIEW, DEFER_TASK, TRY_APPROVED_ALTERNATE_ROUTE, NO_CHANGE | None                                         | Initiate material change or gene/cell production                  |
| E Evaluator         | Training-only label release; sealed evaluation score                | Score ownership only                         | Reveal evaluation labels before response or feed them to O        |
| L Independent limit | Permit or veto one well-typed logical action                        | Independent logical permit                   | Delegate safety constraints to learned networks                   |
| Operator            | Review faults, approve configuration change                         | Manual documented return to OFFLINE          | Bypass unset vendor or biological parameters                      |

# 3. State machine

States are OFFLINE, READY, BASELINE, TRAINING, INTERVENING, EVALUATION, PAUSED, and FAULT. Normal progression is OFFLINE → READY → BASELINE → READY → TRAINING → INTERVENING → EVALUATION → READY. PAUSED can interrupt authorized sessions; FAULT can interrupt any state. FAULT cannot transition directly to READY. A manual-review token is required to return to OFFLINE, followed by a fresh preflight. The state log names every change and its reason. The diagram p02_state_machine.png represents planned transitions, not a hardware control circuit.

# 4. Limit evaluation contract

Every logical action request presents action, state, sensor_age_ns, max_sensor_age_ns, clock_residual_ns, max_clock_residual_ns, storage_available_bytes, minimum_storage_bytes, and watchdog_healthy. A permit is possible only for SIMULATED_TASK_CUE, SIMULATED_RECORDER_READ, and DIGITAL_EVALUATION, with a non-fault state and no failed checks. There are no default physiological values; maximum sensor ages and residuals in the unit tests are arbitrary fixture settings. Any unrecognized action is rejected, and emit_physical() always raises SafetyError. A physical voltage profile, material dispensing or biological intervention cannot be smuggled inside a logical payload.

# 5. Fault handling and recorded evidence

fault_injection_matrix.csv defines F01–F16 with detector, expected behavior, and responsible document. A clock fault, stale support observation, storage shortage or failed watchdog vetoes new task operations; the system records reason and current safety state. The distinction between a stopped **task** and continuing **qualified care** is future-lab specific: P02 cannot switch any care equipment, and it must not pretend that all pumps should stop on a generic software fault. A host reboot cannot replay old stimulation or feedback commands. The simulation's in-memory queue is discarded on restart; production recovery will require explicit sequence reconciliation and review.

# 6. No reward override or hidden optimization

Lotus may rank permissible digital actions by estimated learning value, but L's constraint decisions are lexically prior. A high score cannot compensate for failing sensor freshness or an unapproved device profile. The LotusPolicy.choose() reference produces only four advice classes. It does not learn or actuate in P02. This is a deliberately narrow replacement for any vision of a biological child network directly adjusting chemistry or electrical settings as it sees fit.

# 7. Commissioning tests

Tests cover illegal state transitions, fault reset requiring manual review, unknown physical commands, stale telemetry, clock residual, disk floor, watchdog failure, and Lotus advisory output. The test also verifies that the vendor placeholder has no stimulus or fluid method. The phase exits only after the limit/controller source has been reviewed independently of the training-loop source and the inspection record is archived. A software veto test is not equivalent to a measured independent vendor inhibit path; that remains U03 for P03.

# 8. Sources

\[I01\] CE-DEV-001 v0.4 §§5,17,21–22,25. \[I02\] CE CRYSTAL MATH 001, reconciliation of Heart vs L and scoped roles. \[E01\] ISSCR Guidelines, https://www.isscr.org/resources/guidelines-for-stem-cell-research. \[V02\] https://api-docs.mxwbio.com/index.html. No P02 action is a validated laboratory SOP.

