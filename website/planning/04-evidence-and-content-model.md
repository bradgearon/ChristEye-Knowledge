---
title: "S01 Source of Truth, Evidence and Content Contract"
revision: "2026-11-15"
owner: "We Develop Software LLC"
status: "Planning standard"
---

# Every beautiful page must remain a faithful scientific page

The site is a different **presentation of the same source-controlled Anaiero project**. It may add diagrams, explanations, interactions and simplified summaries, but must never silently replace scientific terminology, retroactively claim a lab result or invent biological procedures. It should serve professional scientists without requiring an engineer to read all 180 manuscript editions first.

## Technical source-to-page pipeline

~~~text
Canonical GitHub source file (Markdown / JSON Schema / Python / image)
  + stable Git blob SHA / document ID
  + claim/evidence annotations
          |
          v
  Verified content registry (source-path / planned URL / status / rights)
          |
          v
  Build-time Markdown normalization + frontmatter validation
          |
          v
  Astro/Starlight full-text page + optional MDX reader aids
          |
          v
  Claim/source panel, cross-links, search index and media assets
          |
          v
  Static GitHub Pages HTML; every page links back to its original source
~~~

**No manual copy/paste of 180 documents**: site pages render from the versioned originals and attach an editorial view layer. The content registry generated during S01 is planning data, not scientific evidence.

## Mandatory page fields

The site-content metadata schema is [page-metadata.schema.json](page-metadata.schema.json), and the statement-level evidence record is [claim-evidence.schema.json](claim-evidence.schema.json). A page has an original source path, a canonical route, source revision, title, audience, kind, rights holder, status, disclosure review and source-of-truth pointer. A claim has an evidence class, a precise measured/preparation scope, an original citation and one or more limitations.

**Research statement classes** (preserve the existing Anaiero terminology):

| Label | Meaning | Required example handling |
|---|---|---|
| **F — Finding** | actual finding in a named study or preparation | identify source study, species/cells, task, endpoint and limitations |
| **X — Synthesis** | integration across reports or an untested extrapolation | never display as a local experimental result |
| **H — Hypothesis** | proposed mechanism or predicted causal outcome | explain the measurement and result that could weaken it |
| **D — Design** | engineering choice, data shape, interface or process requirement | code/source tests distinguish an implementation from a plan |
| **A — Analogy** | Word/Spirit/Lotus etc used as interpretation | distinguish the operational physical/digital correlate |

**Second orthogonal axis:** local-tested-software, local-tested-biological, external-paper, research-proposal, historical-superseded. Evidence class alone is insufficient to say whether a mechanism has been demonstrated *in Anaiero*.

## Narrative depth for scientific articles

Each module or process appears at three levels:

1. **The idea (up to 90 seconds):** what the module accomplishes; its ordinary roles; a graphic; what is currently built.
2. **The engineering or neuroscience pathway (5–20 minutes):** participants, controls, data model, inputs, outputs, state transitions, growth/food/peptide review card, failure handling and schema.
3. **The complete technical record:** full original design and companion source with all tables, equations, constraints, citations, source hash and revision history.

Reader may switch presentation tabs; they may not switch a research hypothesis into a fact.

## Claims about care, love, and power

**Care (physical):** task-independent physiological support, lab-approved culture maintenance, actual source-lot/sensor records and approved intervention review [BIO-10]. No conditioning food or vital support on task success.

**Love (human-facing):** project values, consent, compassion, long-term usefulness, respectful feedback and optional narrative symbolism. Do not infer that an organoid has feelings, or that an anthropomorphic agent is conscious, from electrical activity.

**Power (computational/physical):** event throughput, memory durability, latency, power supply and sensor budgets. A biological system requires nutrient and external electrical power; learned organization is not an energy source.

**Growth:** distinguish cell proliferation, neurite/axon extension, synaptic plasticity, digital capacity and generalized intelligence. Each requires its own measured denominator and success criterion.

## Scientific media rules

A generated picture is a **conceptual illustration** and gets a caption. An original user illustration may appear in narrative sections under the source owner license. An image reproduced from a paper requires rights and a precise citation. Microscopy and biological traces should only be presented as actual Anaiero measurements when there is a traceable experimental dataset with source/assay provenance.

Every visualization has a text fallback and a downloadable JSON/SVG where the source is project-owned. Do not embed hard-to-read science as rasterized text.

## Glossary entity types

| Entity type | Example | Must link to |
|---|---|---|
| Software module | CE-A02 Long-Term Storage | design, companion, code, test, schemas, processes |
| Physical process | BIO-02 axons / Root tissue | source and observable stages, hypothesis status |
| Biological object | axon, astrocyte, lactate, BDNF form | identity, assay, preparation and limits |
| Concept term | Lotus, Milk, Honey, Word, Wind, Crystal | multiple scoped interpretations |
| Scientific claim | "X affects Y" | primary source, preparation, controls, degree of extrapolation |
| Experiment | task or assay | preregistered question, test setup, actual or planned status |
| Demo artifact | output from StarChip or digital simulation | input, source program, version, seed and status |
| Evidence | data figure, paper, source doc | citation, owner, context, integrity digest |

## Document revision and citation requirements

- Maintain original CE-Axx and BIO-xx IDs and historical versions.
- Generate stable heading anchors and keep deep links to original sections.
- Preserve technical source tables verbatim in full-record mode; separately authored summaries may simplify and must link back.
- Back equations with symbol definitions and source; document unresolved mathematical corrections as actual corrections, not silently remove contrary history.
- Preserve code as text with source commits and status; no nonfunctional copy buttons.
- Only index the current revision by default in search; allow an explicit "include historical" option.
- Mark generated user-facing demos as simulation unless tied to an executed source program and reproducible evidence.

## Public rights and source protections

We Develop Software LLC owns the original project research subject to the existing [WDS Research Reference License](../../LICENSE.md). The separate physical patent document is excluded; public technical research may be published under the existing project policy. Raw personal conversations, identifying donor/cell-source secrets, credentials, vendor proprietary manuals and unlicensed third-party assets are not inferred to be public merely because documentation links to them.

A publication filter must operate before generating the site; it is not sufficient to hide a confidential file from the navigation if it is still shipped as a static asset. WDS-specific schema and architecture content remains accessible to the public under the project's stated license.

## Provenance required on each built page

~~~text
Current revision:       v3.1 / 2026-11-15
Original source:        specifications/v3/design/CE-A02...
Source blob hash:       sha from Git content registry
Interpretation:        D design + linked F/X/H research
Executable evidence:    named CI or study, or "not run"
Related process:        BIO-02, BIO-05, BIO-06
Related companion:      CE-A02 Companion
Rights:                 © 2026 We Develop Software LLC
~~~

This metadata must be programmatically generated when possible, not handwritten in a way that drifts apart from Git.

## S01 editorial acceptance

No page should have broken source links, ambiguous authorship, missing evidence classes for headline biological claims, mislabeled simulated traces, or hidden patent-document assets. These are planned build gates for later S-phases, not passed tests now.
