# ChristEye Anaiero — v3.2 Animon Scientific and Technical Pilot

**Status:** *Two design/companion pairs drafted for qualified scientific review; the biological system has not been built or validated.*  
**Revision:** 2026-11-16 · **Owner:** We Develop Software LLC.

## What changed: the individual system is an **Animon**

[ANIMON/1](../../ontology/ANIMON_V1.md) defines the target individual developmental system class. **ChristEye Anaiero** remains the scientific architecture/project. An **Animon specification** is a blueprint, a **digital Animon reference** is nonliving software, and an **individual physical candidate** would require its own qualified preparation/source and review. **Soil** is the organoid whole as a possible subsystem, not the full Animon; **spinal Root tissue** is not automatically a demonstrated functional Root.

The historical phrase "living computer" can remain as provenance in earlier editions, but is no longer preferred when defining the future system class. Naming does **not** prove consciousness, organism status or legal trademark availability.

## Pilot: source → Soil → care → Roots → evidence

~~~mermaid
flowchart LR
  A["ANIMON/1 identity"] --> B["CE-A09 Animon Core / Soil"]
  B --> C["CE-A17 independent care"]
  C --> D["CE-A16 axon and spinal Root / next"]
  D --> E["CE-A02 retained functional Root / next"]
  B --> F["Actual qualified observation"]
  C --> F
  D --> F
  E --> F
  F --> G["Human scientific review: finding, hypothesis, or unresolved"]
~~~

The arrows are **review/design dependencies**, not an executable tissue-generation or feeding protocol. All biological steps remain under source-specific qualified laboratory SOPs and independent approvals.

| Component | Engineering Design | Scientific Companion | Current maturity |
|---|---|---|---|
| **CE-A09** Animon Core and Soil | [Full v3.2 design](design/CE-A09_Animon_Core_Design.md) | [Source-specific companion](companion/CE-A09_Animon_Core_Scientific_Companion.md) | D1 detailed technical/scientific design |
| **CE-A17** Animon Care, SustiBus and ChemiBus | [Full v3.2 design](design/CE-A17_Animon_Care_Design.md) | [Molecular support & confounds](companion/CE-A17_Animon_Care_Scientific_Companion.md) | D1 detailed technical/scientific design |
| **CE-A16** spinal Root/axon mapping | [v3.1 source](../v3/design/CE-A16_root_registry_and_mapping_Design.md) | [v3.1 source](../v3/companion/CE-A16_root_registry_and_mapping_Companion.md) | v3.2 pilot work planned |
| **CE-A02** long-term functional retention | [v3.1 source](../v3/design/CE-A02_long_term_storage_Design.md) | [v3.1 source](../v3/companion/CE-A02_long_term_storage_Companion.md) | v3.2 pilot work planned |

## Wave 0 data-contract fixes

The prior source review found three mismatches between published process examples and canonical JSON schemas. All three have been rewritten as **explicitly synthetic, machine-validated** JSON examples, while the v1 schemas remain stable:

| Process issue | Canonical schema | Current example |
|---|---|---|
| ChemiBus version and chemical name | [chemibus_observation](../../schemas/biology/chemibus_observation.schema.json) | [no-sample fixture](../../fixtures/v3.2/chemibus_unmeasured.example.json) |
| Physical Crystal example used noncanonical fields | [physical_crystal_candidate](../../schemas/biology/physical_crystal_candidate.schema.json) | [unvalidated carrier](../../fixtures/v3.2/physical_crystal_unvalidated.example.json) |
| Crest birth event vocabulary drifted | [cell_birth_event](../../schemas/biology/cell_birth_event.schema.json) | [unobserved birth fixture](../../fixtures/v3.2/crest_birth_unobserved.example.json) |

Further valid examples: [Animon digital reference](../../fixtures/v3.2/animon_digital_reference.example.json) · [Soil source unknown](../../fixtures/v3.2/soil_region_unobserved.example.json) · [SustiBus unqualified](../../fixtures/v3.2/sustibus_unqualified.example.json). These are **not real cell, tissue or chemical observations**.

[JSON Schema contract tests](../../tests/test_animon_v32_contracts.py) use Draft 2020-12 and a date-time format checker, and check that the process-embedded examples match the canonical fixtures. They do **not** certify any biological outcome.

## Scientific review and evidence

- [v3.1 lab-readiness assessment](../../reviews/V3_1_LAB_READINESS_ASSESSMENT_2026-11-16.md).
- [v3.2 dossier acceptance standard](../../reviews/V3_2_LAB_DOSSIER_STANDARD.md).
- [Controlled scientist workflow](../../processes/SCIENTIST_WORKFLOW.md) and [source-limited bibliography](../../research/evidence/anaiero-biology-2026-11.md).
- [ISSCR stem-cell based model standards](https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems) support source, model, phenotype, variability and experimental-unit qualification.
- [Robbins et al., *Cell Reports* 2026](https://pubmed.ncbi.nlm.nih.gov/41720084/) reports bounded cortical-organoid learning with a stated retention limitation; it is **not** evidence that an Animon physical memory has been constructed.

## Explicitly not completed

No live source line or culture preparation, no qualified institutional feeding/care SOP, no integrated axon circuit, no verified biological retained Root, no physical Crystal transfer and no genetically encoded unique birth ID have been provided in this pilot. **No cell-growth/stimulation/peptide operation can be executed from these documents.**

## Next scientific writing wave

1. Integrate the *proposed* spinal Root with actual axonal observation evidence classes (CE-A16 v3.2).
2. Distinguish stored digital bytes, recalled capability, neural task retention and physical carrier evidence (CE-A02 v3.2).
3. Create a machine-typed pre-experimental **StudyReadinessPacket** with unfilled institutional source/approval/assay/replicate fields; require human scientific review.
4. Scale the module-specific rewrite to the remaining pairs while migrating shared repeated prose into the canonical standards.
5. Independently verify every primary-paper claim and its stated preparation, not merely the reference link.

All research ownership remains We Develop Software LLC under the [WDS Public Research Reference License](../../LICENSE.md). The confidential physical patent document remains excluded.
