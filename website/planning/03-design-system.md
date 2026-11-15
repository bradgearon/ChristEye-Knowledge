---
title: "S01 Design System · ChristEye Anaiero"
revision: "2026-11-15"
status: "Visual and interaction specification"
rights: "© 2026 We Develop Software LLC"
---

# Visual direction: a scientific atlas that feels alive

The page should feel **credible enough for a laboratory PI, legible enough for an engineer, intriguing enough for a game developer, and thoughtful enough for a scholar**. Keep the luminous energy of the Anaiero identity, but avoid crowded sci-fi diagrams, fake lab measurements, blurry glowing labels and overwhelming ornament.

The visual metaphor is an atlas of **living relationships and verified interfaces**, not a literal full mind already built. A warm structural line may show a proposed axon or glial association; a crisp teal cable shows a digital protocol; a violet caption identifies a research hypothesis. Shape, label and legend—not color alone—must convey the distinction.

## Three consistent visual domains

| Domain | Visual language | Meaning |
|---|---|---|
| **Digital and engineering** | clean teal/cyan circuit lines, squared connectors, monospaced labels | working software, data schemas, external instrumentation |
| **Living and care** | restrained gold, fine organic tracing, explicitly drawn cells/axon/dendrites and fluid exchange | source-specific physical proposals and measured structures |
| **Theory and scholarship** | parchment/ink reading surfaces, soft violet annotations, source sidebars | philosophical reading, derived hypotheses, reference literature |

Each diagram must display a provenance caption: *conceptual illustration*, *source-derived diagram*, *synthetic simulation*, *actual experiment* or *third-party study figure*. Never imply generated art is a microscopy image.

## Color and typography

The exact **light/dark design tokens** live in [design-tokens.json](design-tokens.json). Deep midnight navy with calm teal works for the interactive system atlas; the reading surface can switch to an off-white scientific-paper appearance. Use small gold and violet accents only when explaining living tissue and research rather than as decorative badges.

Choose system or permissively licensed open-source fonts without bundling non-licensed assets. Suggested stacks:
- Heading/UI: Inter or system-ui, with generous letter spacing and visible hierarchy.
- Scientific/scholarly passages: Source Serif 4 or Georgia fallback, used for longer interpretive chapters.
- Code, event IDs and schema properties: IBM Plex Mono or ui-monospace.

**Always preserve technical notation**: micro units, Greek symbols, equation indices, cell marker names and LaTeX math must remain copyable real text. A beautiful screenshot cannot replace a readable equation.

## Homepage composition — first 2 screens

**Screen 1 / The promise and the truth:**

~~~text
┌───────────────────────────────────────────────────────────────────────────────┐
│ ChristEye Anaiero  Explore  Living Systems  Build  Science  Studio  Library   │
├───────────────────────────────────────────────────────────────────────────────┤
│ RESEARCH + ENGINEERING                              [Evidence status]         │
│                                                                               │
│ A computer designed to learn,                        System atlas:            │
│ remember and grow through                            LOTUS → WIND BUS         │
│ connected systems.                                    ↔ SPIRIT / ROOTS         │
│                                                      ↔ SOIL (RESEARCH)        │
│ [Explore architecture]  [Try digital demo]                                    │
│                                                                               │
│ Live today: software reference.  Research: living networks and material       │
│ knowledge transfer.  No complete organoid computer is claimed.               │
└───────────────────────────────────────────────────────────────────────────────┘
~~~

**Screen 2 / Choose a lens:** engineer, neuroscience, research scientist, scholar, creator; each links to an actual starting point. A shallow "What has been demonstrated?" panel follows, with verified digital demonstration source and careful scientific limits.

## Long-form technical page anatomy

~~~text
Breadcrumbs  /  Design ID CE-A02  /  v3.1                Search / version
TITLE — Long-Term Storage
One-paragraph scope, actual implementation versus biological proposal
[Overview] [Engineering Design] [Scientific Companion] [Schemas] [Tests]
---------------------------------------------------------------------------
LEFT: stage graph  |  CONTENT: problem → operation → measurement → gate  | TOC
                    visual detail in panels, code with copy action
                    source references beside the claim
---------------------------------------------------------------------------
Molecule/axon sidebar: named structure, tissue region, evidence source, limits
Growth + feeding/peptide review: signed human gate and explicit status
Related pages: BIO-02, BIO-05, BIO-06, CE-A16, sources, old edition
~~~

Keep the reading column **near 78 characters** and avoid 80%-width science tables inside cramped sidebars. Render tables with sticky headers and horizontal scrolling when needed; retain accessible text alternatives.

## Reusable science/developer components

| Component | Responsible information | Action/behavior |
|---|---|---|
| **SystemAtlas** | hierarchical network nodes, each tied to a source ID | click a node to inspect ordinary physical/digital meaning |
| **RoleTwin** | digital representation compared with biological counterpart | synchronized tabs with separate evidence and stage |
| **ProcessStepper** | source-approved stages, inputs, outputs, holds and criteria | timeline that never auto-advances a biological research gate |
| **MoleculeRecord** | entity, form, compartment, receptor/assay, evidence ID | filter by neural/glial/energy/ECM class and open source |
| **ScientificClaim** | claim text, status F/X/H/D/A, preparation, source, competing explanation | citation reveal and "what would falsify this?" |
| **SchemaExplorer** | original JSON Schema, examples, references and constraints | copy sample and open canonical JSON file |
| **BusTrace** | timestamped WindBus/ChemiBus/SustiBus events and identities | step through simulated input/output with validity checks |
| **RootEvidenceGraph** | four edge types and tested temporal Root | toggle structural versus statistical versus causal versus task |
| **GrowthReviewCard** | observed deficit, candidate change, independent authorizations | show decision and physical veto, never issue a real command |
| **CareLoop** | independent support status versus learning feedback | contrast Milk/Water and Honey, without false chemical inference |
| **ChipDemoViewer** | synthetic or verified digital example and test receipt | export mock output and view source code |
| **ResearchProvenance** | source papers, experiments, original version and limitations | permanent citation anchors and revision history |

## Diagram design guidelines

- Render proposed **Crest cell lineage** as ancestor branching with uncertain observation IDs; do not depict a unique DNA code magically appearing in every new daughter.
- Draw an **axon as part of a neuron**, with soma, dendrites and possible synapse; distinguish axon projection from accepted task influence.
- Show **Soil as the whole organoid**, surrounded by a Sea exchange environment. Never label matrix alone as Soil.
- Draw **three physically separate care/data buses**: SustiBus (care status), ChemiBus (sample observations), WindBus (typed digital data). BioBridge is an instrument boundary, not a permission to alter tissue.
- For a Crystal show two separate columns: digital pack (can copy/replay) and physical research carrier (hypothesis).
- For *Power* show externally supplied energy, compute usage, sensor bandwidth and latency budgets; no impossible biological energy claims.
- For *Love* show design goals around thoughtful user interaction, consent, care continuity and non-manipulative support. Do not depict a culture's emotional experience as measured.

## Accessibility is part of the scientific release gate

At minimum: WCAG AA contrast checked computationally; keyboard-accessible interactive controls and tooltips; reduced motion; equations and schema text accessible without images; alt text for every figure; captions for actual videos; copyable citations; focus outline; high-resolution diagram zoom and mobile touch affordances; test with screen reader and keyboard navigation before publishing.

**No custom font files are required or redistributed in the site source.**

## Intended feeling

A cross between a disciplined research laboratory notebook, a systems-engineering interface, and an illustrated academic atlas. One visual voice for 180 documents, even where the underlying content crosses software, biology, art and philosophy.

[Site routes](02-information-architecture.md) · [Evidence and editorial contract](04-evidence-and-content-model.md).
