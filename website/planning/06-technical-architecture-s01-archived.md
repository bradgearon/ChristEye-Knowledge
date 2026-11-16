> **Archived S01 proposal — superseded by React/Vite and Cloudflare Workers.**

---
title: "S01 Technical Website Architecture"
revision: "2026-11-15"
status: "Approved candidate architecture, not yet installed"
---

# Build a single source-driven site without replacing the existing GitHub source

## Architectural decision: Astro + Starlight + custom interactive islands

**Candidate platform:** Astro with Starlight as the docs foundation. The project requires two unusually different presentation types:

1. **Large canonical technical library:** nearly 180 Markdown conversions plus current module designs, processes, papers, schemas and hardware plans. Starlight provides a readable docs shell, sidebars, search, code, heading URLs, dark/light modes and MD/MDX support.
2. **Designed editorial atlas and specialized demos:** the root landing page, living-system atlas, molecule/network explorer, architectural visualizations, process stepper and chip gallery need custom layouts and selective browser-side JavaScript, not plain Markdown.

Starlight pages can live alongside custom Astro pages. Reference: [Starlight pages](https://starlight.astro.build/guides/pages/) and [Astro static GitHub Pages deployment](https://docs.astro.build/en/guides/deploy/github/). Do not pin an unverified future package version in this planning document; resolve and lock actual dependencies during S02.

### Why not simply expand docs/index.html?

The current single HTML file has a card search that cannot index internal source sections. The technical source contains hundreds of Markdown pages, equations, metadata, nested diagrams and 44+ JSON schemas. Handwriting HTML pages for all of them would make versions drift and remove the original provenance.

### Why not treat GitHub Markdown as the final website?

It hides the architecture behind cross-repository-looking links and provides no native educational sequence, no demo data viewer, no scientific claim cards and no cross-discipline search. It remains the canonical source while the website provides authored, traceable views.

## Proposed package layout

~~~text
ChristEye-Knowledge/
├─ README.md                         Canonical project entry
├─ processes/                        Existing canonical human/science processes
├─ specifications/                   Existing v3.1 24× paired modules
├─ phases/                           Existing P01, P02, P02B source
├─ research/                         Existing scientific sources and evidence
├─ schemas/                          Existing JSON Schemas
├─ registries/                       Existing 33 chip manifests
├─ anaiero/                           Existing digital prototype
├─ anaiero_p02b/                      Existing synthetic read-only supervisor
├─ docs/                              Current live Pages site; keep until parity
└─ website/
   ├─ README.md                       Planning, source audit and navigation
   ├─ planning/                        The S01 locked blueprint and content registry
   ├─ scripts/                         S02+ source-import, lint, claim/rights and link audit
   ├─ app/                             S02+ Astro + Starlight project
   │  ├─ astro.config.mjs
   │  ├─ package.json + lockfile
   │  ├─ src/components/
   │  ├─ src/pages/                   Custom atlas / studio / landing pages
   │  ├─ src/content/docs/            Generated or mounted source-driven technical pages
   │  ├─ src/styles/
   │  └─ public/                      WDS-approved visual and demo assets
   ├─ fixtures/                        S04+ deterministic, explicitly synthetic demo data
   └─ generated/                       Ignored build-time source reflections and indexes
~~~

**Do not move the canonical source Markdown**. The build process reads the original repository folders using the content registry and generates stable pages. Generated content can be rebuilt from the same Git commit, avoiding a maintenance burden from duplicate Markdown.

## Specific build-time pipeline

~~~mermaid
flowchart TD
  A[Canonical Markdown, JSON, Python, images] --> B[Manifest check]
  R[website/planning/content-registry.json] --> B
  B --> C[Source SHA and publication review]
  C --> D[Content normalization]
  D --> E[Scientific page metadata + source links]
  E --> F[Starlight Markdown / MDX pages]
  S[Schema registry] --> G[JSON schema and relationship browser]
  D --> H[Search index]
  F --> I[Astro static build]
  G --> I
  H --> I
  J[Demo fixtures, rights and tests] --> I
  I --> Q[CI: source links, claims, accessibility, demo behavior]
  Q --> P[GitHub Pages static output]
~~~

### Content normalization details

- Read source by the exact path and Git revision recorded in the site map.
- Merge an editorial metadata file with existing YAML frontmatter; never overwrite the original research/observations in-place.
- Convert relative links in source Markdown to the corresponding generated site route **when a route exists**. Unconverted links remain an explicit direct GitHub source link.
- Copy any local source images to a deterministic asset path and preserve the original caption, source and licensing conditions.
- Preserve diagrams as editable Mermaid/SVG where provided; add text equivalents.
- Normalize code blocks, tables, math (including LaTeX delimiters) and in-document anchors. Handle older DOCX-converted HTML details blocks safely; do not arbitrarily execute scripts embedded in sources.
- Flag any failed content import, duplicate route or unresolved source as a CI error, not a silently omitted page.
- Build search indexes from the full rendered text. Historical documents should be excluded by default or ranked below the current release.

## Base URL and GitHub Pages

Astro static export planned configuration:

~~~javascript
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

export default defineConfig({
  site: 'https://bradgearon.github.io',
  base: '/ChristEye-Knowledge',
  integrations: [
    starlight({ title: 'ChristEye Anaiero' })
  ]
});
~~~

**Important:** the exact Starlight configuration, package versions and navigation syntax must be validated in S02, including all absolute and relative links under the repository base path. This is illustrative design code, not a tested build.

The existing published branch source currently uses the repository's docs folder and GitHub Pages successfully. During S02, build a separate preview artifact from the new Astro source without replacing the currently working site. Only switch GitHub Pages to GitHub Actions deployment when the new build has parity, user-visible review and the source/rights tests pass. Use the official Astro GitHub Pages action and document the switch.

## Client execution and backend constraints

GitHub Pages is a static host. Browser demos can be pure JavaScript or bundled components with deterministic fixtures. There is no server-side Python execution simply because the repository contains Python. For a working Python digital prototype, choose one explicit method:

- **Recorded reference run:** CI executes Python and publishes a versioned JSON result fixture, reproducible by source commit and test.
- **Ported browser algorithm:** a reviewed client-side reimplementation of a narrow deterministic algorithm, compared against the Python reference for a shared fixture.
- **Live external service (future only):** requires its own backend, authentication, scoped rights, cost controls, access policy and security review. Not part of Step 1.

Never simulate a physical organism and label it real. Synthetic observations are marked synthetic both in data and visual interface.

## Web security and license considerations

WDS-owned research can be published under the existing [WDS Public Research Reference License](../../LICENSE.md). The actual patent document must be excluded from the build and repository. Raw conversation history with private content or third-party records cannot be blindly indexed. Treat scientific source excerpts and imported screenshots according to their original copyrights.

- Static Markdown rendering must avoid untrusted HTML or script execution and unsafe URL schemes.
- Any upload (later phase) needs explicit user consent, limits and non-storage defaults; no anonymous browser access to private training material.
- Content security policy, dependencies and GitHub Actions permissions require review.
- Generate a sitemap, metadata, canonical URLs and version-aware redirects for existing public routes.
- Pin build tools and packages with a lockfile and record the Git SHA in generated output.

## Feature acceptance and performance

Aim for a first meaningful static render, accessible core content without JavaScript, and interactive enhancements that do not block reading. Publish build time, page count, link quality, performance and accessibility checks in CI. Target WCAG AA and documented mobile behavior. Do not assert achieved Lighthouse scores until tested.

### Build S02 technical tasks

1. Scaffold Astro/Starlight under website/app and install with a reproducible lockfile.
2. Implement a safe source import for one design (CE-A02), one process (BIO-02), one schema and one research page.
3. Resolve generated page links and asset paths under /ChristEye-Knowledge with unit tests.
4. Add page metadata and evidence state chips without changing source scientific claims.
5. Assemble a minimal homepage and source navigation; make one preview artifact.
6. Validate source correspondence before migrating the rest of the library.

S02 should not be declared complete solely because a starter theme compiles; the canonical material must appear in the resulting pages with correct links and evidence.

[Source inventory](01-source-audit.md) · [Information architecture](02-information-architecture.md) · [Design system](03-design-system.md) · [Scientific content model](04-evidence-and-content-model.md).
