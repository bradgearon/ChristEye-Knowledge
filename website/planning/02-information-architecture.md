---
title: "S01 Information Architecture"
revision: "2026-11-15"
source: "ChristEye-Knowledge current main"
status: "planning document; no new scientific claims"
---

# Structure the site around people, processes and source evidence

The website must function as **an explanation, an engineering desk, a neuroscience atlas, and a reproducible experiment portal**. A reader should never have to download an archive to understand one system.

## Primary navigation

| Entrance | Reader | Core pages | Distinct treatment |
|---|---|---|---|
| Explore | New visitor | Introduction, system layers, Lotus/Heart, Roots/Soil | Stepwise diagrams and short explanatory paths |
| Living Systems | Neuroscientist, cell scientist | Crest, Soil, axons, spinal Root, growth, care | Cellular identity, sources, measurements, experimental uncertainty |
| Build | Programmer, embedded engineer | WindBus, module contracts, schemas, hardware, tests | Copyable examples, packet diagrams and GitHub source links |
| Science | Researcher and reviewer | Molecule atlas, hypotheses, papers, methods, experiments | Evidence class, source scope, alternative explanations |
| StarChip Studio | Creator and technical buyer | Module catalog, work examples, digital laboratories | Real digital outputs or clear future-prototype descriptions |
| Scholarship | Scholar and curious reader | Concept dictionary, interpretation, sources and history | Interpretive framing distinct from verified biology |
| Project (secondary) | Collaborator, reviewer | Roadmap, release gates, WDS license, contribution | Honest milestone and rights information |

## Recommended reader journeys

**Novice in five minutes:** What is Anaiero? → Two-plane architecture (digital now; biological proposed) → Seven named functions → One genuine software demo → What remains an experiment → Browse deeper.

**Developer in twenty minutes:** Install the digital prototype → run WindBus and Crystal replay → open the exact schema → inspect CE-A02 design and companion → see test output → check source and revisions.

**Neuroscientist in thirty minutes:** Soil = organoid → cell lineage and Crest → axons and proposed spinal Root tissue → neuronal/glial and receptor atlas → electrical/chemical measurement → learning protocol → delayed retention → actual evidence limitations.

**Scholar:** Word, Bread, Milk, Honey, Water, Wind and Tree meanings → ChristEye role mapping → original interpretive sources → testing versus metaphor → annotated references.

**Creative user:** Metroidvania chip, music chip, book chip or researcher chip → representative inputs → visible outputs → test that proves software behavior → why a physical Crystal is a distinct scientific claim.

## Exact route families

~~~text
/
├─ start/
├─ architecture/
│  ├─ lotus-heart/
│  ├─ buses/
│  └─ roots-memory/
├─ living/
│  ├─ crest-lineage/
│  ├─ soil/
│  ├─ roots-axons/
│  ├─ care/
│  ├─ growth/
│  └─ processes/{process-id}/
├─ science/
│  ├─ molecules/
│  ├─ evidence/
│  ├─ equations/
│  ├─ experiments/
│  └─ research/{source-id}/
├─ build/
│  ├─ quickstart/
│  ├─ modules/{module-id}/{design-or-science}/
│  ├─ schemas/{schema-id}/
│  ├─ hardware/
│  ├─ phases/
│  └─ code/
├─ studio/
│  ├─ chips/{chip-id}/
│  └─ labs/{demo-id}/
├─ scholarship/
├─ library/
│  └─ history/v2/
└─ project/
~~~

The root path is *not* the GitHub Pages deployment root. Every generated URL needs the existing base prefix **/ChristEye-Knowledge** when hosted on GitHub Pages.

## Core system atlas editorial flow

**What it is → What participates → What changes → What we observe → How learning is tested → How it is maintained → What is not proven.**

For each atlas component, show two tracks: **Digital realization** and **Biological research counterpart**. A named Soil, Root or Spirit must have an ordinary physical referent if a biological mechanism is claimed.

### Example: Long-term storage CE-A02

One page presents:
- The working digital content-addressed archive and retrieval flow.
- The hypothesized spinal Root tissue and axon mapping separately.
- Named cellular, receptor and scaffold assay candidates.
- Delayed retention test sequence and negative explanations.
- SustiBus care and ChemiBus molecular observation channels.
- Source-linked design, companion, schemas, literature, and original version.
- Version/status markers and tested claims only where independently supported.

A two-page tab layout is fine as long as the **full original content is reachable and indexed**.

### Example: the care, love and power trio

**Care** means continuity of essential support, respect for living research systems, and valid measures of health, not conditional reward. **Love** means the human-facing values of dignity, stewardship, compassionate design and non-manipulative feedback, not a claim that a culture feels loved. **Power** means measurable computation/throughput/energy and future platform capability, not a biological source of free energy. Link each to its own scientific, technical and scholarly interpretations.

## Access and navigation constraints

- Every 24-module design has a companion tab, process dependencies, data schemas, tests and source citations.
- A claim with an uncertain evidence state is visible with its limitation, not hidden.
- The first screen contains at least one accurate system diagram and a clearly labeled runnable software demonstration.
- Search indexes content **inside** the actual 180 documentation editions, not only card titles.
- Archival v2 routes are indexed separately and clearly marked superseded.
- An original manuscript or full design is available from within the website, with direct GitHub source links as an optional secondary action.
- Each page offers in-page table of contents, accessible code, citation links and mobile-friendly tables.

## Route inventory

[site-map.json](site-map.json) contains the initial **22 editorial route families**. [content-registry.json](content-registry.json) holds each source-path-to-route mapping; no content copy becomes the scientific authority simply by appearing in the website.

WDS Research Reference License applies. The physical patent document is excluded; non-patent WDS-owned research may be published.
