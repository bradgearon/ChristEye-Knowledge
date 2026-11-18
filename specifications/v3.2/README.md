# ChristEye Anaiero — v3.2 Animon Scientific & Technical Pilot

**Operational science work orders (v3.3):** the [laboratory execution packet](../../laboratory/README.md) supplies the previously missing actual roles, source/genetic material requirements, daily physical care receipts, previous-day readings/limits, source/stage measurement handoffs, axonal trace evidence and independent retention study gates. All eight A09/A17/A16/A02 design/companion documents now embed their own module-specific operating checklists. [Read-only QC program](../../qa/daily_qc.py) and [34-task execution register](../../laboratory/EXECUTION_TASKS.json) provide digital/operational validation. This does not authorize unqualified physical cell procedures or validate a living Animon.


**Revision:** 2026-11-16 · **Owner:** We Develop Software LLC  
**Status:** **4 detailed design/companion pairs for scientific review**; living Animon construction and neural-learning experiments remain unexecuted.

## The Animon: scope and source precedence

An **Animon** is the proposed individual developmental system class described in [ANIMON/1](../../ontology/ANIMON_V1.md). **ChristEye Anaiero** remains the program and underlying WDS architecture. A digital Animon reference is nonliving software, and a proposed Animon physical individual requires real source, qualified tissue and independent laboratory review. **Soil** is the prospective organoid whole; **spinal Root tissue** is a candidate anatomical subsystem; **functional Roots** are separately tested retained relations.

The previous [v3.1 complete library](../v3/README.md) remains intact with all 24 designs, 24 scientific companions and their historical source editions. The current v3.2 pilot is a **more mechanism-specific successor**, not a replacement for unrelated modules or proof of new biological results.

## The complete first pilot: Core → Care → Axon/Root → Retention

~~~mermaid
flowchart LR
  A["ANIMON/1 spec and source"] --> B["CE-A09 Core / Soil"]
  B --> C["CE-A17 independent Care / SustiBus"]
  C --> D["CE-A16 spinal Root/axon evidence"]
  D --> E["CE-A02 learning & delayed retention"]
  B --> F["Measured source or explicitly unknown"]
  C --> F
  D --> F
  E --> F
  F --> G{"Scientific evidence review"}
  G --> H["Claim scoped to actual preparation"]
  G --> I["UNRESOLVED, negative or untested"]
~~~

**This is a dependency and data-evidence graph, not a tissue-development or stimulation procedure.** Any physical biological operations require a real qualified facility and separate SOP/ethics approvals. No live preparation was supplied to this project in these documents.

| Module | v3.2 Engineering Design | v3.2 Scientific Companion | Specific deliverable |
|---|---|---|---|
| **CE-A09** Core and Soil | [Full design](design/CE-A09_Animon_Core_Design.md) | [Scientific companion](companion/CE-A09_Animon_Core_Scientific_Companion.md) | Individual Animon vs living Soil identity, source/region/assay gates |
| **CE-A17** Independent care | [Full design](design/CE-A17_Animon_Care_Design.md) | [Scientific companion](companion/CE-A17_Animon_Care_Scientific_Companion.md) | Milk vs Honey, ChemiBus/SustiBus, continuous care and fault tests |
| **CE-A16** Axon and spinal Root | [Full design](design/CE-A16_Animon_Spinal_Root_Design.md) | [Scientific companion](companion/CE-A16_Animon_Spinal_Root_Scientific_Companion.md) | Four typed evidence edges, candidate source/axon map, causal/task alternatives |
| **CE-A02** Long-term retention | [Full design](design/CE-A02_Animon_Long_Term_Retention_Design.md) | [Scientific companion](companion/CE-A02_Animon_Long_Term_Retention_Scientific_Companion.md) | Digital memory/replay, independent Root tests, source-scoped biology, retention/access |

### What is already implemented in code?

The reference [MemoryBank](../../anaiero/memory.py) supports permissioned lexical retrieval. [CrystalPack](../../anaiero/crystal.py) supports source-hashed, permission-scoped **digital** clones and replay. The [Root metrics](../../anaiero/roots.py) implement macro recall and a chance-corrected retention ratio with guards. The [Lotus](../../anaiero/lotus.py) permits only specific digital actions. These are **not** a living spinal network, verified task-learning organoid or physical memory carrier.

## Typed data records and honest synthetic fixtures

### New v3.2 review and evidence contracts

| Contract | Purpose | Design-only example |
|---|---|---|
| [StudyReadinessPacket](../../schemas/animon/study_readiness_packet.schema.json) | Explicit preparation/PI/assay/control/care/analysis dependencies and no physical execution rights | [Unapproved packet](../../fixtures/v3.2/study_readiness_unapproved.example.json) |
| [RootEvidenceEdge](../../schemas/animon/root_evidence_edge.schema.json) | Distinguishes structure, statistics, influence and task claims in one versioned graph | [Unobserved candidate edge](../../fixtures/v3.2/spinal_root_edge_unobserved.example.json) |
| [AxonProjection](../../schemas/biology/axon_projection.schema.json) | Source/target candidate and qualified spatial/functional observation fields | [No axon observed](../../fixtures/v3.2/axon_projection_unobserved.example.json) |
| [FunctionalRootAssessment](../../schemas/biology/functional_root_assessment.schema.json) | Frozen evaluator, actual task/score/delay, controls and protected old skills | [No Root measured](../../fixtures/v3.2/functional_root_unobserved.example.json) |

**Important:** Fixtures contain fake sources, null observations and explicit DESIGN/UNRESOLVED statuses. They are tested for schema consistency, not offered as actual measurements. The all-zero SHA-256 string in the functional Root fixture is a **synthetic placeholder**, not the digest of a real reader artifact.

### Prior v3.2 Wave 0 contract repairs

| Earlier drift | Resolved source record | Valid illustrative record |
|---|---|---|
| ChemiBus version and chemical fields | [ChemiBus schema](../../schemas/biology/chemibus_observation.schema.json) | [Unmeasured observation](../../fixtures/v3.2/chemibus_unmeasured.example.json) |
| Physical Crystal noncanonical fields | [Crystal candidate schema](../../schemas/biology/physical_crystal_candidate.schema.json) | [Unvalidated candidate](../../fixtures/v3.2/physical_crystal_unvalidated.example.json) |
| Crest cell-birth names | [CellBirthEvent schema](../../schemas/biology/cell_birth_event.schema.json) | [Unobserved birth](../../fixtures/v3.2/crest_birth_unobserved.example.json) |
| Overall identity | [Animon spec](../../schemas/animon/animon_spec.schema.json) | [Nonliving digital reference](../../fixtures/v3.2/animon_digital_reference.example.json) |
| Soil and care | [Soil schema](../../schemas/biology/soil_region.schema.json) and [SustiBus schema](../../schemas/biology/sustibus_care_status.schema.json) | [Unobserved Soil](../../fixtures/v3.2/soil_region_unobserved.example.json) and [unqualified care](../../fixtures/v3.2/sustibus_unqualified.example.json) |

All current fixtures have tests under [test_animon_v32_contracts.py](../../tests/test_animon_v32_contracts.py). Valid JSON shape is not scientific validation; real source/assay/unit/clock/approval evidence remains separately necessary.

## Review versus laboratory operation

**D1 technical specification:** typed records and source-linked distinctions can be reviewed and validated as software/documentation.

**D2 pre-experimental handoff:** a named, approved institution must choose real source model, instrumentation, primary question, independent biological unit and statistics, qualified care, controls and owners. The [StudyReadinessPacket](../../schemas/animon/study_readiness_packet.schema.json) has all these fields but does not fill them with fabricated values.

**D3 physical protocol and D4 data:** not provided or claimed. A written study plan does **not** authorize a lab to grow tissue, change molecular conditions or perform neural stimulation.

## Sources and professional standards

- [v3.1 lab-readiness review](../../reviews/V3_1_LAB_READINESS_ASSESSMENT_2026-11-16.md) and [v3.2 scientific dossier standard](../../reviews/V3_2_LAB_DOSSIER_STANDARD.md)
- [12-stage scientist workflow](../../processes/SCIENTIST_WORKFLOW.md), [BIO-02 axons](../../processes/BIO-02-axon-spinal-root.md), [BIO-06 teaching](../../processes/BIO-06-teaching.md), [BIO-07 joining](../../processes/BIO-07-joining.md)
- [Gibbons et al. 2026 corticospinal connectoid](https://pubmed.ncbi.nlm.nih.gov/42201810/) and [Robbins et al. 2026 bounded organoid learning](https://pubmed.ncbi.nlm.nih.gov/41720084/)
- [ISSCR stem-cell-based model standards](https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems) and [ISSCR reporting standards](https://www.isscr.org/basic-research-standards/reporting)

## Following science work

1. **Scientific-review packet evaluation:** have the designated PI, neuroscience investigator, cell-care lead, biosafety role and embedded engineer review the four module dossiers and source/model uncertainties.
2. **Digital graph proof:** add an actual append-only, evidence-status-enforcing graph in software and test contradictory/expired evidence, without any physical device commands.
3. **Digital retained-Root demonstration:** build a true independent held-out/delayed reference test; currently replay and lexical retrieval alone do not establish learned competence.
4. **Module expansion:** use the same evidence matrix and schema-valid fixture standard for CE-A11, A06, A07, A04 and the remaining v3.1 pairs.
5. **Specimen-specific biology:** the institution must independently choose a source, equipment and approved methods before any living study.

WDS owns this research under the [WDS Public Research Reference License](../../LICENSE.md). The confidential physical patent file is excluded; any commercial Animon branding requires separate trademark review.
