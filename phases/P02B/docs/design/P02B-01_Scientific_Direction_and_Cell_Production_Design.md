---
title: "P02B-01 Scientific Direction and Cell Production Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02B"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02B_Supervisory_Electronics_and_Scientific_Operations/docs/design/P02B-01_Scientific_Direction_and_Cell_Production_Design.docx`  
> **SHA-256 (original DOCX):** `cfc3bfa4393688b275d388a8d9ddd3d5a280551ecbbf07e84e3d72bf416d824e`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Owners:** Principal investigator (PI) and cell-production lead. **Scope:** research oversight, planning and release handoffs, not a biological recipe.

## 1. Responsibilities and authority

**Scientific director / PI:** approves scientific questions, claims, unit of replication, endpoints, randomization, training and retention study design, changing hypotheses, stop rules, ethical compliance and publication claims. Signs the StudyPlan, HypothesisRecord, EvidenceReview, and study go/no-go decision. Cannot substitute a plausible response pattern for measured acquired learning.

**Cell-production and maturation scientist:** owns approved source-bank authorization and consent verification, source lot/lineage, preparation identity, phenotype panel, release testing, maintenance SOP version, quality flags, receiving history, contamination review, cell-loss/viability evidence, and recorded deviations. Signs CellLotRelease, PreparationTraveler, ReceivingReceipt, and CareSOPReference. Their scope is delivering a qualified preparation, not certifying semantic knowledge.

**Organoid developmental scientist (separate future role):** owns spatial patterning, internal regions, geometry/transport, maturity comparisons, imaging accessibility and 3D-specific release. A planar spinal culture does not automatically become a cerebral organoid by changing its chamber.

**Electrophysiology lead:** owns device compatibility, electrode map, noise, actual delivered electrical event receipts, artifact control, readout freeze, and independent stimulation inhibit evidence. **Assay/imaging lead:** owns measurement specificity, calibration, phototoxicity, RNA/protein imaging controls and sample identities. **Biostatistician:** owns power/precision assumptions, nesting, independent units and blinded analysis. **Independent safety/quality officer:** owns approvals, incident escalation and release override restrictions.

## 2. Five release artifacts and what they contain

| **Artifact**        | **Minimal fields**                                                                                                                          | **Gate**                         |
|---------------------|---------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------|
| StudyPlan           | study ID; scientific question; source IDs; primary outcome; units; comparison groups; exclusions; analysis; freeze digest; PI signoff       | Before research training         |
| CellLotRelease      | source ID/consent scope; species and regional identity; batch; test evidence; qualified function; deviation/disposition; production signoff | Before receipt                   |
| PreparationTraveler | preparation ID; step/lot history; environment and supported SOP versions; measured changes; custody; review states                          | Per physical preparation         |
| AssayQualification  | analyte/target; method; actual biological compartment; calibration; controls; precision; burden; drift; failure                             | Before interpreting a new sensor |
| ClaimRecord         | exact claim; evidence class; experimental unit; alternative explanation; supporting data; independent review; limitations                   | Before capability promotion      |

## 3. Operating cycle

\(1\) PI freezes a testable objective. (2) Production lead qualifies source and release configuration. (3) Instrument lead signs the read-only/approved electrical and image interfaces. (4) Quality owner accepts measurement readiness. (5) Experiment runs with source-based audit. (6) Analysis owner independently evaluates learning, retention and safety. (7) PI issues ACT/HOLD/UNRESOLVED and updated claim scope. (8) Cell owner handles retirement and disposal under approved procedure.

## 4. Stop and escalation conditions

Unknown source or donor consent; identity mismatch; contamination suspicion; uncharacterized cell population drift; unknown electrical profile; stale critical sensors; unreviewed intervention; artifact/decoder leakage; loss of qualified care; no verifiable independent inhibit. Any one suspends the affected claim or operation. Learning success cannot waive these limits.

## 5. Success for this document

Owner-by-gate RACI signed; templates validated; critical actions have separate scientific and physical-authority signers; at least one simulated 'bad lot', 'missing calibration' and 'optimistic score with frozen-label leak' case rejected; no direct cell actuation exposed through a learned network.

**Sources:** P01 controlled parameter register; P02 limit design; CE-DEV-001 v0.4 §§7–10, 21–23, 29; ISSCR model-system recommendations https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems .

## Implementation linkage in v6

The runnable reference lives in ../../src/anaiero_p02b/. P02B software permits read-only observations and independent simulated scientific handoff checks only. P02C electrical proof and P03 laboratory operations are separately gated in ../../registers/phase_roadmap.csv. This document remains a **candidate** until an owner signs the current vendor compatibility and qualification records.

