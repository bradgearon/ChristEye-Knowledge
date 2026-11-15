# Anaiero documentation style guide

**Effective:** November 15, 2026. **Owner:** We Develop Software LLC. **License:** [WDS Public Research Reference License](../LICENSE.md).

## Documents must be complete

An index describes *where* a technical answer lives; it is not the answer. Every scientific or engineering subsystem needs (a) a [20-section design document](document-standard.md) and (b) a companion containing its underlying references, equations, assumptions, alternatives and controlled experiments.

The original Word document is the archival source. GitHub Markdown is the primary accessible publication format.

## Fixed editorial hierarchy

1. A descriptive title, document ID, P-phase and revision date.
2. A two-sentence scope stating exactly what the document specifies, and what is not demonstrated.
3. An architecture diagram or boundary table when the module exchanges data.
4. Typed records / source-to-recipient contracts, with an example that is consistent with published JSON.
5. Process stages with gate/owner/release evidence.
6. Equations in `\( ... \)` or fenced LaTeX with symbol and unit definitions; derivations are not empirical confirmation.
7. Separate safety and experimental controls.
8. Acceptance/failure conditions, including inconclusive outcomes.
9. Inline citations anchored in [the P01 bibliography](../phases/P01/references.md) or the named paper.
10. WDS copyright and a reference to the [license](../LICENSE.md).

## Terminology

**Soil** in the organoid branch denotes the declared living organoid. The **Sea** denotes its surrounding exchange environment. **Lotus** is a scoped coordination role, not automatically one molecular structure. **Heart** as biological response is not the external interlock. **Spirit**, **Star**, **Wind** and **Crystal** require ordinary physical/digital referents and declared boundaries.

**Bread / Meat:** foundational versus advanced structured curricula. **Milk:** qualified sustaining support. **Honey:** selective feedback/recruitment. **Water:** transport/access. No metaphor sets a reagent dose or an electrical profile.

## Types and source integrity

- Use a consistent `RecordName` and `snake_case` fields.
- Use actual JSON Schema files in [schemas/v6](../schemas/README.md), not retyped approximations.
- State which module is the authority for every field.
- Identify UTC and instrument-local clock domains separately.
- Missing values must be explicit, never assumed to be zero or normal.
- Preserve preparation/batch ID; electrodes and trials are not independent cultures.
- Keep source documentation, generated interpretations and observed measurements separate.

## Dates, links and revisions

Use YYYY-MM-DD for machine-readable metadata and human-readable date in prose; do not silently overwrite older historic dates. Link to the nearest complete design, companion, test and source entry. Keep a CHANGELOG entry for every changed experimental interpretation or gate. Previous contradictory claims remain traceable.

## Public disclosure

The actual patent document is intentionally excluded. Research and experiments are credited to WDS under the project license. Avoid silently copying external proprietary manuals or leaking private logs/credentials. Public descriptions of inventions may affect patent rights; publication does not establish confidentiality.
