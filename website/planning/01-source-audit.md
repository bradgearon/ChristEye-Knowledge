---
title: "S01 · Source and website audit"
project: "ChristEye Anaiero"
status: "Approved for implementation planning, not a new published science claim"
date: 2026-11-15
rights: "© 2026 We Develop Software LLC"
---

# S01. Source audit: what exists and what is missing

**Audit source:** [ChristEye-Knowledge repository](https://github.com/bradgearon/ChristEye-Knowledge) at commit `2a3ac2737fdaf9a390c59e3475c35d4c68708784`. The content map is in [content-registry.json](content-registry.json), with **352 source files assigned proposed routes**, verified for route uniqueness. This is a site-content inventory, not a count of independent experiments or validated implementations.

## Current publication and evidence

| Asset | Verified source count | Current rendering issue |
|---|---:|---|
| Current engineering designs | 24 | Full Markdown exists, but website cards link to GitHub |
| Current scientific companions | 24 | No first-class in-site theory/evidence tab per module |
| Controlled processes | 12 | No visual, interactive step-through process guide |
| P01, P02, P02B full design documents | 6 + 14 + 21 | Phase documents live in repository, not a phase navigator |
| Source-derived scientific editions | 10 | Hard to browse by molecule, test, or claim |
| Machine-readable schemas | 54 | No schema browser or object relationship diagrams |
| StarChip module manifests | 33 | No demonstrations or capability-focused catalog on website |
| Historical v2 files | 83 plus index | Needs clearly separated archival context |
| Executable Python examples | 2 | No user-facing browser demo; only local runs |

The current site is one `docs/index.html` (**8.8 KB**) with `docs/assets/site.css` (**4.6 KB**). Its current search filters only cards, not the documents. Most cards link to raw `github.com/.../blob/main` Markdown. There is no site compiler for the long-form technical corpus, no cross-disciplinary navigation, and no reproducible examples embedded in site pages.

**Important:** the hosted page could not be independently downloaded through the browser search tool in this audit; this assessment is based on current GitHub source and recorded successful Pages deployments, not an in-browser screenshot.

## What is genuinely demonstrated

- **Software references:** a Python WindBus, Root metrics, digital Memory Bank/Crystal replay, content-addressed objects, and read-only P02B telemetry; GitHub CI is the proper authority for current test status.
- **Research design:** proposed Soil organoid, spinal Root tissue, Crest lineage, axon map, ChemiBus/SustiBus and physical Crystal; none is a demonstrated fully integrated living computer.
- **Third-party literature:** organism, axon, memory and organoid studies cited in [the evidence register](../../research/evidence/anaiero-biology-2026-11.md) are findings in their own preparations, not local Anaiero experimental success.

## Site problems to solve, prioritized

1. **Source discoverability:** six different audiences currently see the same flat catalog. Need structured learning paths and a predictable information hierarchy.
2. **Science comprehension:** the difference between a project concept, a published finding, a testable hypothesis, and an actual Anaiero result is not visually systematic.
3. **No process atlas:** birth → Soil → care → axons → teaching → Root tests → Crystal hypotheses cannot currently be explored as a connected timeline.
4. **Missing practical demos:** the digital prototype's inputs and outputs are invisible to non-programmers; specialized game/music/research chips are manifests, not runnable site experiences.
5. **Technical readability:** schemas, 24 modules, 12 processes and research documents need source-linked TOCs, diagrams, symbol legends, copyable code, and responsive tables.
6. **Editorial design:** current visual language is functional but a single static graphic/cards page; it needs type hierarchy, rich scientific figures and navigation density suitable for long reading.
7. **Content governance:** preserve original file revisions, attributable WDS license, patent document exclusion, privacy controls and explicit claim labels in generated pages.

## Invariants for the redesign

- **One repository source of truth:** do not retype 180 documents as hand-maintained website copies.
- **One claim has one evidence chain:** source IDs, preparation, limitations and methodology travel with the claim.
- **One metaphor has a scoped meaning:** Love/Care is not proof of biological feeling; Milk/Honey are distinct support/feedback functions; Power has measurable energy and compute cost.
- **Digital demo must never appear to run on living cells** unless separately proven.
- **No secret patent documents** and no raw private conversation logs shipped in the public bundle; non-patent WDS-owned experimental work can be included under the existing license.

## Evidence for moving to implementation

Site route registry generated; 0 duplicate routes; content-area counts checked against live repository file tree; source-to-proposed-page paths recorded. These are **planning deliverables**, not a deployed redesign.
