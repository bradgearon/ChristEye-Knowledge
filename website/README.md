# ChristEye Anaiero — Site Redevelopment S01

**Step 1 complete as a planning specification · November 15, 2026**

**Site:** [bradgearon.github.io/ChristEye-Knowledge](https://bradgearon.github.io/ChristEye-Knowledge/)  
**Source:** [ChristEye-Knowledge GitHub](https://github.com/bradgearon/ChristEye-Knowledge)  
**Rights holder:** We Develop Software LLC · [WDS Public Research Reference License](../LICENSE.md)

> This directory describes the **next-generation website plan**. It is not a claim that the Astro implementation, biological demos or redesigned full research atlas have already shipped. The original docs site stays online until a tested and source-complete replacement is ready.

## Objective

Turn the existing large source-controlled Anaiero research corpus into **one composed, accessible, scientific, developer and scholarship site**. Make it possible to understand the Lotus/Heart system, Soil, Spirit, spinal Roots, Crest lineage, axons, learning, Care/Milk/Honey, Chemibus/Sustibus, molecular evidence, growth process, Crystals and specialist StarChips—without compromising source accuracy or inventing living-computer performance.

The site has six reader entrances: **Explore**, **Living Systems**, **Build**, **Science**, **StarChip Studio**, and **Scholarship**, with **Project** for status, licensing and provenance.

## Step 1 deliverables — available now

| Artifact | Technical purpose |
|---|---|
| [01 — Source and current-site audit](planning/01-source-audit.md) | Real repository inventory, site weaknesses, evidence and missing capabilities |
| [352-source content registry](planning/content-registry.json) | Every mapped original source path, blob hash, audience, status and unique proposed route |
| [02 — Information architecture](planning/02-information-architecture.md) | Site navigation, primary audience journeys, canonical routes and cross-link rules |
| [Editorial sitemap (JSON)](planning/site-map.json) | 22 editorial route families with their source documents |
| [03 — Visual and scientific design system](planning/03-design-system.md) | Type, layout, colors, interactive components, figure standards, accessibility, wireframes |
| [Design tokens](planning/design-tokens.json) | Portable color, typography, layout and motion tokens |
| [04 — Evidence and content model](planning/04-evidence-and-content-model.md) | Source of truth, scientific claim classes, rights, relationships, references and versioning |
| [Page metadata schema](planning/page-metadata.schema.json) | Validates editorial source paths, statuses, audiences and licenses |
| [Claim evidence schema](planning/claim-evidence.schema.json) | Evidence class, scope, preparation, alternatives and review provenance |
| [05 — Interactive demos and StarChip studio](planning/05-interactive-labs-and-starchips.md) | Required honest outputs, test fixtures and separation of executed from proposed chip functions |
| [Nine-demo catalog](planning/demo-catalog.json) | Input/output contract, actual code availability, source/seed/rights requirements |
| [06 — Technical site architecture](planning/06-technical-architecture.md) | Astro/Starlight strategy, content import, GitHub Pages base path, CI and safe switchover |
| [07 — Release stages and evidence gates](planning/07-release-plan.md) | S01–S08 phased build and measurable acceptance checks |
| [08 — Flagship page storyboards](planning/08-first-page-storyboards.md) | Detailed editorial compositions for architecture, care, growth, A02, molecules, scientist workflow and Studio |

## What Step 1 found

The existing website is a compact, source-based landing page: one primary static HTML file and a small stylesheet. The repository has the far deeper material: 24 technical designs, 24 companions, 12 controlled processes, P01/P02/P02B documents, a research source atlas, typed schemas, 33 StarChip manifests, source Python and recorded CI tests.

The **first implementation target is CE-A02 long-term storage** because it forces a testable full vertical slice: a genuinely readable full engineering document and scientific companion; source-linked spinal Root and axon science; Milk/Chemibus process relationships; readable JSON; a deterministic digital Crystal replay; and correct evidence labeling.

## Decisions to carry forward

1. **Site framework:** Astro + Starlight, with custom Astro pages for diagrams and interactive labs.
2. **Source:** render existing canonical Markdown and JSON directly from their Git revisions, avoiding divergent hand-maintained copies.
3. **Publication route:** stay on GitHub Pages at /ChristEye-Knowledge; preview and test the new build before switching away from the current docs-folder deployment.
4. **Visual:** coherent modern laboratory/editorial style, comfortable reading typography and source-aware diagrams—not a crowded fantasy map or a fake microscope view.
5. **Claims:** external research, local software, local biological measurements, design, hypothesis and metaphor remain separately labeled.
6. **Studio:** demonstrate actual digital execution when available; game/music/theory chips are clearly proposal previews until built.
7. **Values:** care, love and power are explained in physical, human-facing and computational senses without collapsing philosophical language into biological proof.
8. **Licensing:** publish WDS-owned research with the existing WDS Research Reference License; exclude the separate physical patent document. Personal raw conversation logs are not scraped into the public site.

## Steps after S01

**S02** should establish a real Astro/Starlight build and publish a **preview** of CE-A02, BIO-02, one schema and one scientific source before changing the live site. [S03–S08](planning/07-release-plan.md) cover corpus migration, system atlas, runnable demos, science/scholarship and launch verification.

## S01 acceptance

- All planned content routes unique and mapped back to existing source paths.
- Source counts match the Git tree snapshot used in the audit.
- Every active demo points at real code; proposal-only chips are labeled unimplemented.
- All source and doc links in this planning set resolve.
- Existing WDS license and confidentiality boundary respected.
- New planning files have no dependency on a new build tool or a fabricated experiment.

This is **the editorial and architectural blueprint for the website**, not a replacement for the complete project design documents.
