---
title: "S01 Delivery Roadmap and Release Gates"
revision: "2026-11-15"
rights: "© 2026 We Develop Software LLC"
status: "Prioritized site program; only S01 actions are in this release"
---

# The site is a scientific product, so release it with evidence gates

We are **not** rewriting the biological project phases P01–P08. This is a separate **website program S01–S08** that publishes and explains those sources. Every phase has a demonstrable artifact and exit checks; a date does not substitute for a finished test.

## Delivery plan

| Website stage | Main output | Prerequisite | Measurable exit gate |
|---|---|---|---|
| **S01 Foundation and audit** | source inventory, IA, design tokens, evidence schema, demo registry, page storyboards, architecture, plan | current repo | exact source hashes, route uniqueness, no secret patent references/assets, prioritized backlog, review PR |
| **S02 Website engine** | Astro/Starlight project in website/app, safe content importer, working preview | S01 | correctly renders CE-A02 design+companion, BIO-02, one JSON Schema and one study, valid deep links, no live site regression |
| **S03 Full documentation ingestion** | all current modules, processes, phase documents, research and selected historical editions render natively | S02 | 24+24 current docs, 12 processes, source/route matrix and search coverage, 0 unresolved canonical routes |
| **S04 System and growth atlas** | interactive layers for Lotus, Soil, Root, axon, buses, care, growth and Crystal; text fallbacks | S03 | each diagram has click-through sources, scientific status and keyboard equivalent; no fictional lab data |
| **S05 Working StarChip studio** | digital replay, Lotus, WindBus, P02B lab and first specialist chip output gallery | S02/S04 | four digital labs have passing fixtures and source/seed links; speculative chips labeled previews |
| **S06 Science and scholarship** | molecule atlas, evidence/experiment register, formal models, interpretation lens, care/love/power chapters | S03/S04 | every headline living claim has evidence state, source, scope and limitation; original source discoverable |
| **S07 QA, site switch and launch** | responsive production site on GitHub Pages with permanent routes, tests and WDS rights | S02–S06 | accessibility, broken links, demo execution, claim review, rights disclosure, Pages deploy and rollback verified |
| **S08 Maintenance and enrichment** | versioned updates, new StarChip outputs, research decisions and source citations | S07 | CI detects drift, pages updated with source revision, negative findings preserved |

### S01 implementation scope

S01 produces documents and machine records only. It does not claim that a fully navigable Astro site exists yet. The current Pages site stays intact, except for an optional link to this plan. [Audit](01-source-audit.md) · [Content registry](content-registry.json) · [IA](02-information-architecture.md) · [Design system](03-design-system.md) · [Editorial model](04-evidence-and-content-model.md) · [Demos](05-interactive-labs-and-starchips.md) · [Architecture](06-technical-architecture.md) · [Storyboards](08-first-page-storyboards.md).

## Dependencies and highest-value vertical slice

The first real publishable vertical slice in S02 should be **Long-Term Storage / Roots** (CE-A02). This one feature exercises all site requirements:

1. Render a full 20-section engineering design from the canonical file.
2. Render its scientific companion with real citations, source/date/scope.
3. Cross-link the actual spinal Root and axon process (BIO-02).
4. Render the corresponding Root and axon JSON Schemas.
5. Display the digital content-addressed storage system as implemented; clearly separate physical Root and Crystal hypotheses.
6. Show one deterministic digital Crystal replay fixture and its test receipt.
7. Link the original full Markdown, WDS license and scientific claim audit.
8. Verify page on mobile, keyboard-only, no JavaScript and search.

**Why A02?** It exercises engineering, neuroscience, data, learning, care, uncertainty, theory and one runnable example without requiring live equipment. If this is coherent and trustworthy, the remaining 23 modules can reuse its page template.

## Workstream ownership and review matrix

| Workstream | Accountable role | What it must sign off |
|---|---|---|
| Site product/editorial | WDS project owner | story, rights, audience, accuracy and priorities |
| Science/experimental | scientific director | source scope, study status, claims, mechanisms and negative alternatives |
| Cell/care | cell-production lead | source, Milk/SustiBus, identity and non-actuation boundary |
| Neuroscience | developmental neuroscience / electrophysiology lead | axon, spinal Root, true electrical observations and instrument provenance |
| Build/CI | web/developer lead | source imports, schema renderer, links, secure static build and test receipts |
| Interface/visual | UX/accessibility lead | figures, responsive reading and all-user navigation |
| WDS licensing | rights holder and selected legal counsel | copyright/license and patent document exclusion |

These are review **functions**, not assertions that named human staff have approved any work yet.

## Requirements and explicit acceptance checks

**SC-01 Source fidelity:** canonical file/commit and document ID shown; no silent paraphrase in full-source view.

**SC-02 Scientific honesty:** all hero claims refer either to observed software behavior, specific external research or a labeled hypothesis. Live organoid learning is not falsely claimed.

**SC-03 Documentation parity:** 24 current designs and companions and 12 biological/software process specifications rendered fully, with stable direct source links.

**SC-04 Typing and versioning:** JSON Schema pages generated from original .schema.json, with version information and examples, not handwritten substitutes.

**SC-05 Demonstration proof:** every active run has input, seed where applicable, actual code, deterministic expected output, status, version and reproducible test.

**SC-06 Living safety:** no demo/browser control can issue a physical chemical or stimulation command; Milk support never depends on reward.

**SC-07 Disclosure:** patent document absent from artifacts and site output; WDS license visible; private raw conversations not auto-indexed.

**SC-08 Site quality:** linked routes and sources resolve, images have alt text, responsive diagrams, readable equations, keyboard/screen-reader access and appropriate performance budget measured in CI.

**SC-09 Historical integrity:** old versions remain accessible but marked superseded and default search prioritizes current v3.1.

**SC-10 Rollback:** a failed site migration cannot make the old docs disappear; previous successful deployment and source remain available.

## Proposed issue epics after S01

- SITE-EPIC-01: build pipeline and source transformation.
- SITE-EPIC-02: foundational scientific UI and component kit.
- SITE-EPIC-03: 180-document import and schema browser.
- SITE-EPIC-04: living systems + process atlases.
- SITE-EPIC-05: runnable digital labs and specialist StarChip gallery.
- SITE-EPIC-06: scholarly/evidence views and citations.
- SITE-EPIC-07: QA / GitHub Pages release and communications.

Do not clutter the repo with 100 implementation issues before the first S02 vertical slice has proven content import and page design.

## What can be told publicly when S01 is finished

"ChristEye Anaiero's website redesign has a complete source-driven architecture plan, route inventory, design system and documented demo requirements." Do **not** say the redesigned site or biological platform is completed.

**Next implementation step:** S02 Astro/Starlight scaffold + CE-A02 vertical slice. Preserve the current live website until it meets the publishing gate.
