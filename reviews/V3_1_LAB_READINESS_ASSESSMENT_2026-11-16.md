---
title: "ChristEye Anaiero — scientific and technical lab-readiness review"
date: "2026-11-16"
revision_under_review: "v3.1"
owner: "We Develop Software LLC"
status: "Source-grounded documentation audit; not laboratory authorization"
---

# Anaiero v3.1 — Scientific and Technical Lab-Readiness Assessment

**Scope:** The [24 design/companion pairs](../specifications/v3/README.md), 12 BIO/SYS process specifications, [scientist workflow](../processes/SCIENTIST_WORKFLOW.md), ten [biological JSON schemas](../schemas/biology/README.md), [module registry](../specifications/v3/rigorous-module-registry.json), and [literature register](../research/evidence/anaiero-biology-2026-11.md).

**Review conclusion:** excellent progress toward a coherent science/engineering *research proposal and handoff*. **Not yet a lab-executable or experimentally validated living-computer system.** No actual cell/assay qualification, experimental neural learner or transferable physical Crystal is demonstrated in these source documents. This review does not authorize biological interventions or assert regulatory compliance.

## Review ratings (subjective documentation scores, not experimental results)

| Category | Rating / 10 | What supports it |
|---|---:|---|
| Overall architecture | 8.2 | Separated roles, sources, buses, evidence and safety |
| Biological ontology | 8.0 | Soil organoid, axons as neuronal processes, candidate spinal Root versus functional Root |
| Developer documentation | 7.4 | 24 module interfaces, cross-links and code-oriented records |
| Biological data models | 6.8 | Ten versioned JSON schema files, although examples currently drift |
| Literature integration | 6.7 | Real relevant scientific papers, insufficient claim-by-claim tracing |
| Module-specific mechanisms | 4.3 | Most modules still repeat generic templates |
| Study design and statistics | 4.5 | Valid control classes, but no module-specific primary outcome/statistical plan |
| Reproducibility and laboratory handoff | 3.1 | Named preparation, SOP qualification, real instruments and study signoffs are missing |
| Safety and oversight as a design | 7.8 | Independent care, human signoff, no automatic physiological actuation |
| Physical laboratory execution | 2.5 | Not established by this document set |

## What we have and should preserve

- The physical **Soil** is the proposed organoid as a whole, while the surrounding exchange environment is separate.
- **Spinal Root tissue** is a proposed physical candidate; a **functional Root** means an independently tested retained relation. They cannot be declared one-to-one.
- **Axons, dendrites and dendritic spines are neuronal structures**, not separately cloned cells. New cell births, axonal extension, synapse remodeling and task-specific learning need independent measures.
- **Crest** records source, clone lineage, cell observation and track; a heritable lineage barcode does not automatically give each daughter a globally unique genetic ID.
- **SustiBus** independently records qualified care, **ChemiBus** records chemical assays/proposals, **WindBus** handles digital data. Essential Milk/care must not depend on Honey/task feedback.
- A **digital CrystalPack** can support versioned source replay; a portable physical Crystal or peptide-based installed semantic competence is still a hypothesis.

## Verified structural finding: excessive generic repetition

Across current A13–A24 designs, the main v3.1 sections average approximately **18.4k characters**. Twenty-four paragraphs totaling approximately **10.5k characters** are identical across all twelve. Companions in the same range average approximately **11.5k characters**, with sixteen repeated paragraphs totaling **7.3k characters**. These are audit observations from the repository texts, not a scientific metric.

The 20-section design and 14-section companion template is useful, but currently crowds out the uniquely measured mechanism. **A02 is the better prototype**: [long-term storage design](../specifications/v3/design/CE-A02_long_term_storage_Design.md) contains a worked digital persistence data model, a staged test battery, and a specific scientific limitation table; its [companion](../specifications/v3/companion/CE-A02_long_term_storage_Companion.md) separates retention, retrieval, axon participation and stronger material-carrier claims.

**Corrective action:** centralize common ontology, care and ethics rules in one versioned standard. Give each module a unique mechanism figure, specific data source, testable hypothesis, control matrix and acceptance/negative result. Historical v3 source should remain accessible, but distinctly archival.

## Exact record-contract defects (highest priority)

| Issue | Process source | Canonical schema | Corrective gate |
|---|---|---|---|
| **M01 ChemiBus vocabulary and version drift** | [BIO-04](../processes/BIO-04-sustibus-chemibus.md) example uses schema_version chemibus.v1, analyte, chemical_form, data_validity, assay_and_calibration_ref | [chemibus_observation.schema.json](../schemas/biology/chemibus_observation.schema.json) expects version 1.0.0, analyte_identity, molecular_form, interpretation, assay_id and calibration_ref | Adopt one versioned vocabulary and publish a machine-valid example |
| **M02 Physical Crystal record drift** | [BIO-08](../processes/BIO-08-crystals.md) example uses kind, region_and_boundary, claimed_content_id, retention_evidence and outcome | [physical_crystal_candidate.schema.json](../schemas/biology/physical_crystal_candidate.schema.json) expects physical_boundary_ref, learned_relation_ref, delayed_retention_ref and evidence_status, among other fields | Mark as research-only, preserve explicit non-installable state, validate example |
| **M03 Crest birth event mismatch** | [BIO-01](../processes/BIO-01-crest-lineage.md) describes division_detected_at, imaging_run_id and confirmation_method | [cell_birth_event.schema.json](../schemas/biology/cell_birth_event.schema.json) expects observed_at, division_evidence_ref and observation_method | One canonical provenance model with a real observation and a not-observed state |

A schema-valid JSON object is **not** a credible scientific measurement. Add semantic rules: actual source/assay identity, unit, missingness reason, valid stage transition, and human review evidence before promotion to OBSERVED. Maintain migrations when fields change.

## Scientific source scope

- [2026 organoid functional readouts](https://www.nature.com/articles/s44222-026-00507-6) confirms limitations involving geometry, sampling and reproducibility.
- [Gibbons et al., 2026 connectoid](https://pubmed.ncbi.nlm.nih.gov/42201810/) reports structurally separated corticospinal axon connectivity in a specific study; it does **not** demonstrate an Anaiero spinal memory bank.
- [Robbins et al., 2026 organoid task learning](https://pubmed.ncbi.nlm.nih.gov/41720084/) reports feedback-driven task improvements that **did not persist after a 45-minute rest** in that study; this is a key limitation for Anaiero Root claims.
- [Dewa et al., 2025 astrocytic multiday trace](https://www.nature.com/articles/s41586-025-09619-2) studies mouse memory stabilization, not organoid knowledge transfer.
- [BDNF/MMP-9 synaptic plasticity study, 2025](https://pubmed.ncbi.nlm.nih.gov/40991708/) supports particular localized synaptic mechanisms, not a peptide carrying a book.
- [ISSCR model-system standards](https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems) and [reporting standards](https://www.isscr.org/basic-research-standards/reporting) support source identity, cell/microenvironment quality, defined biological experimental units, replicability and clear reporting. The ISSCR **2025 Guidelines v1.2 revision** specifically updated embryo-model recommendations, not all stem-cell organoid operating protocols.

## Remaining laboratory science and engineering gaps

| Priority | Gap | Release evidence needed |
|---|---|---|
| P0 | Qualified source/model and owner | Actual preparation family, scientific source record, PI/cell-production roles, legal/ethical review |
| P0 | Instrument and sample validity | Qualified MEA/imaging/chemistry data source, calibration/version, spatial/temporal scope, sensor failure states |
| P0 | Study-specific questions | One primary question per study, explicit alternative explanations, independent biological unit and holdout plan |
| P0 | Source-to-claim matrix | Mechanism-specific primary paper and model, actually observed property, limits, citation/claim ID |
| P0 | Canonical schema/example consistency | Machine-valid examples, migration and cross-document schema test |
| P1 | Reproducibility plan | Predefined outcomes/units, variance and batch structure, statistical rationale, negative results |
| P1 | Safety implementation evidence | Independent service continuity and digital fault-injection tests, separately qualified physical SOP references |
| P1 | Module-specific diagrams and data | Actual unique state machines, test fixture and failure path, not just shared template |
| P2 | Advanced physical carrier proposals | Independent content-specific carrier and receiver evidence before physical Crystal claims |

No document should include an invented culture medium, electrical regimen, genomic insertion procedure or peptide dosage and label it laboratory-ready merely because it is detailed.

## Staged v3.2 correction plan

**Wave 0 — canonical core:** repair M01–M03, define study readiness and scientific claim ledgers, standardize measurement provenance/units and remove repeated global text from module-specific sections.

**Wave 1 — physical basis and care:** CE-A22 independent safety, CE-A17 care, CE-A09 core Soil, CE-A15 development, CE-A16 spinal Root, CE-A11 connective circuits, CE-A24 observer. Deliver each as an engineering design + mechanistic companion + valid schema/test.

**Wave 2 — learning:** CE-A01 episodic learning, CE-A02 long-term retention, CE-A06 stabilization, CE-A07 enhancement, CE-A12 Lotus, CE-A18 metabolic distinctions. Distinguish digital source storage from independent biological retention and access.

**Wave 3 — maintenance and scaling:** CE-A03 healing, CE-A04 growth, CE-A05 renewal, CE-A08 health, CE-A19 routing, CE-A20 filesystem, CE-A21 human-centered feedback. Separate digital service lifecycle from living physiological care.

**Wave 4 — Crystal hypotheses:** CE-A10 compiler, CE-A13 copying, CE-A14 replay, CE-A23 molecular Word / physical Crystal. Digital packs are a different mechanism from biological material transfer; do not imply the latter has been demonstrated.

## First integrated v3.2 pilot

One cross-module slice: **CE-A09 Soil → CE-A17 independent care → CE-A16 axon/spinal Root → CE-A02 delayed learned relation**. Link BIO-01, 02, 03, 04, 05, 06, 07 and 10, with the actual tissue protocol and medium chosen only by a qualified scientific institution.

Each pair needs:
1. Unique ordinary physical/digital participants and measurable states.
2. Versioned input/output and at least one **schema-valid** fixture.
3. A precise research question, a primary endpoint and meaningful counterexamples.
4. A linked per-claim primary-literature comparison including where its model differs from Anaiero.
5. Independent care and human scientific review with HOLD/UNRESOLVED outcomes.
6. A source-qualified SOP **reference slot**, no fabricated wet-lab parameters.

**Recommended first action:** fix M01–M03 and define the shared v3.2 [dossier standard](V3_2_LAB_DOSSIER_STANDARD.md). Then rewrite CE-A09/17/16/02 as the first coherent review packet rather than making all 48 documents longer at once.

© 2026 We Develop Software LLC. Public research reference license. The physical patent document remains excluded.
