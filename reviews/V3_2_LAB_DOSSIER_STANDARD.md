---
title: "Anaiero v3.2 — scientific design and companion acceptance standard"
date: "2026-11-16"
owner: "We Develop Software LLC"
status: "Proposed engineering/research handoff standard; not a biological SOP"
---

# V3.2 — a qualified scientist's review packet

**Canonical naming decision:** The proposed target class is **Animon**, described in [ANIMON/1](../ontology/ANIMON_V1.md). Each dossier must identify Animon specification versus nonliving digital reference versus authentic physical candidate; Soil and spinal Root tissue remain subsystems with their own independently reviewed evidence. [v3.2 pilot](../specifications/v3.2/README.md).

**Purpose:** make every module understandable and assessable by a scientific director and engineering team. Preserve original source documents, avoid repeated generic material, and distinguish **research design**, **software tested**, **instrument qualified**, **preparation qualified**, and **local biological result** as different evidence stages.

## Engineering design — mandatory unique content

| Required section | An actual reviewer needs to see |
|---|---|
| Module claim | One focused capability, user/scientific outcome and explicit exclusions |
| Participants | Named source/recipient digital modules, real biological cell/axon/glial candidates, qualified preparation where relevant |
| Substrate | Soil organoid, separate spinal Root candidate, or digital-only system — no false one-to-one mapping |
| Interfaces | Actual bus names, data producers/consumers, versioned schemas, units, clock, source IDs and access restrictions |
| State and failure | Source receipt, baseline, HOLD, missingness, invalid assay, retry/rollback for digital paths and independent living care |
| Identity | source_lot_id, clone_lineage_id, cell_instance_id and track IDs with uncertainty; born cells not automatically genetically encoded |
| Measurements | assay family, specimen/location, spatial/temporal bounds, raw/derived data, calibration and evidence link |
| Test and acceptance | primary/secondary endpoints, comparator, false positives, protected competencies and meaning of negative results |
| Growth/care proposal | separate digital/software changes from living intervention review; PI/cell lead, SOP reference, independent care and safety gate |
| Implementation | source code/test where available, honest unimplemented stage and issues |
| Reading path | matching scientific companion, BIO process, schema, literature and evidence claim status |

Every example JSON must validate against the actual source-controlled schema. Shared definitions and policies should be linked, not duplicated verbatim across 24 modules.

## Scientific companion — each mechanism requires an evidence row

A companion's primary content is a **claim-to-source matrix**, not an introductory essay.

| Field | Mandatory meaning |
|---|---|
| claim_id and class | Finding F, synthesis X, hypothesis H, engineering D or analogy A |
| experimental model | Named original study preparation (e.g., mouse, rat, cultured neurons, human organoid, digital simulation) |
| primary source | Real article DOI, publication, relevant figure/result or explicit "no evidence" |
| actual finding | What the original study measured, with method limitations |
| project hypothesis | What is **proposed for Anaiero** and what does not carry across models |
| relevant molecules | Actual ordinary form, compartment and receiving population where relevant, not an ingredient list |
| candidate axon/Root link | Anatomical, statistical, functional and learned-task edges kept distinct |
| alternative | Maturation, support/viability, decoder change, bulk excitability, shared drive or artefact |
| study discriminator | A meaningful future measurement/control/negative outcome, not a claimed biological result |
| readiness status | Owner and next review, not an automatic actuation or lab-ready signoff |

Reference literature for model-quality standards: [ISSCR stem-cell model systems](https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems) and [ISSCR reporting](https://www.isscr.org/basic-research-standards/reporting).

## StudyReadinessPacket — a non-executing biological research handoff

A future qualified scientist should be able to populate or explicitly reject the following fields:

- Study ID; module and version; principal scientific question; alternative/falsifier.
- Specific chosen source/preparation and approved ethical/consent/source review.
- Laboratory scientific director, cell-production lead, independent safety and reviewer roles.
- Vendor model and locally approved SOP reference **without inserting guessed culture or stimulation values**.
- Instrument/assay references, calibration, data integrity, regional/spatial/temporal coverage and validity.
- Experimental unit and hierarchy (preparations, lines, batches, cells, electrodes, trials), potential confounders and independent controls.
- Outcomes, decision and unknown states, declared comparison, statistical model/rationale and negative-result handling.
- Qualified maintenance/care protocol and failure escalation separate from education/reward.
- Complete audit hashes, source references, case IDs, review signatures and publication rights.

**State machine:** DRAFT → SCIENTIFIC_REVIEW → INSTRUMENT_REVIEW → LAB_APPROVAL (only by qualified laboratory) → RUN/READOUT (only after separate approved SOP) → ANALYSIS → REVIEWED_RESULT or REJECTED/UNRESOLVED. A written packet alone does not automatically move to LAB_APPROVAL.

## Machine-verifiable acceptance

1. All source-linked JSON example fixtures validate against their specific schema version.
2. Each physical evidence record has preparation/sample identity, calibration or meaningful missingness, source and time.
3. OBSERVED / LAB_RESULT status requires externally reviewed evidence reference; templates and hypothetical values remain DESIGN/H.
4. Every current design and companion cross-link to the same canonical process and model schema; changes are versioned.
5. Every module-specific scientific proposition has cited original experimental context, meaningful alternative and falsification question.
6. Digital reference functionality has reproducible tests; claims of biological performance require distinct actual data.
7. No physical patent, private raw conversation content, unidentified donor information or confidential material appears in public outputs.

## First four modules to refine

- **CE-A09 (Core/Soil):** qualified source identity, regional/cell coverage, acquisition/observation and explicit unknowns.
- **CE-A17 (Care/Bus):** ChemiBus and SustiBus schema-valid examples, task-independent care receipts and source/operator review.
- **CE-A16 (Spinal Roots/Axons):** unique structural versus functional connectivity tests, tracing confidence and no automatic physical memory assignment.
- **CE-A02 (Long-term storage):** digital CAS reference separated from delayed functional Root evidence, independent reader and candidate mechanism hypotheses.

A successful first pilot is an evidence-bearing **LAB-REVIEW READY** dossier — *not* a live tissue recipe or a demonstrated physical Anaiero device.

© 2026 We Develop Software LLC · WDS Public Research Reference License. Patent document excluded.
