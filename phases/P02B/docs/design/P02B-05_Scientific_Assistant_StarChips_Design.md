---
title: "P02B-05 Scientific Assistant StarChips Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02B"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02B_Supervisory_Electronics_and_Scientific_Operations/docs/design/P02B-05_Scientific_Assistant_StarChips_Design.docx`  
> **SHA-256 (original DOCX):** `61e16a98bb4a8d8ecb0b1dba31c7049b5b2dddf43fb424d0bed3c5e59760b232`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Scope:** AI-assisted scientists operating on the Anaiero host, not independent biological authorities. **Runtime:** software-only research agent with explicit review queues.

## 1. Two distinct science-facing software modules

**Research Director StarChip (SCD)** observes qualified data, hypothesis status, pending unknowns and resource constraints. It can propose a StudyPlanDraft, MechanismHypothesis, EvidenceGap, BudgetScenario and ReviewRequest. It may compare literature and simulate designs, but it cannot mark hypotheses as established, execute a laboratory SOP, approve an ethics protocol, or issue an incubator/MEA command. The human PI signs scientific releases.

**Cell Foundry StarChip (SCF)** models the approved preparation catalog: source lineage, possible target populations, receiving preparation versions, maturation-state milestones, cell inventory, material history and quality tests. It can create ProductionRequestDraft, LotCompatibilityAssessment, InventoryReservation, ReceivingChecklist and DeviationAlert. It cannot invent a biological production recipe or replace a qualified laboratory's approved source/SOP. A human production scientist owns and signs the actual traveler.

## 2. Interface contracts

| **Service**  | **Input required**                                                               | **Output**                                        | **Who authorizes**           |
|--------------|----------------------------------------------------------------------------------|---------------------------------------------------|------------------------------|
| SCD Plan     | claim, evidence, source, estimated independent preparations, target, uncertainty | versioned StudyPlanDraft                          | PI + statistical reviewer    |
| SCD Diagnose | failed evaluation, clock/assay validity, protected tasks                         | ranked competing hypotheses with reason           | scientific director          |
| SCF Develop  | approved GrowthRequest, lineage catalogue, chamber geometry and capacity         | ProductionRequestDraft with known/unknown fields  | production lead              |
| SCF Release  | quality records, stage results, locked SOP and lot ID                            | release recommendation / REVIEW / HOLD            | production lead + quality    |
| SCF Care     | qualified maintenance plan + current telemetry                                   | review alert for service overdue or unusual value | operator/OEM controller only |

## 3. Software message flow

Lotus identifies a capability gap → creates NeedRequest → SCD compares retraining, access/decoder repair and capacity hypotheses → PI reviews a locked study plan → SCF receives an approved production request (only if physical expansion justified) → laboratory produces and releases a distinct preparation → instrumentation qualifies interface → independent testing evaluates contribution. SCD receives the published outcome as evidence. **Neither SCD nor SCF has a physical command route.**

## 4. Training the StarChips

Start with structured, literature-linked cases and *rejected* proposals from prior experiments, not only success examples. Train to output uncertainty, named alternative explanations, sources and the correct human approver. Evaluate on hidden case reviews: missing source lot; incompatible chamber; apparent learning caused by decoder adaptation; sensor drift mistaken for growth; false reading of lactate as an engram; unsupported automatic peptide intervention. Require *abstention* and UNRESOLVED when critical data are missing. Use audit and human feedback to revise software policy, keeping test cases sequestered.

## 5. Acceptance

One hundred percent of synthetic unauthorized actuator requests are rejected; every generated experiment has an evidence-status field and independent evaluator; no suggested CellLotRelease without real lab signature; every grow/refresh proposal carries no-growth, retrain and instrument-repair alternatives; model-specific source citations are retained and never converted into biological experiment results.

**Sources:** Anaiero P01 controlled unknowns; P02 WindBus ACL; CE-DEV-001 v0.4 source/production/joining sections; ISSCR source and model reproducibility https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems .

## Implementation linkage in v6

The runnable reference lives in ../../src/anaiero_p02b/. P02B software permits read-only observations and independent simulated scientific handoff checks only. P02C electrical proof and P03 laboratory operations are separately gated in ../../registers/phase_roadmap.csv. This document remains a **candidate** until an owner signs the current vendor compatibility and qualification records.

