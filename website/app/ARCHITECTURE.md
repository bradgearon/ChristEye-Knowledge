# ChristEye Anaiero — React Static Portal Architecture

**Revision:** 0.2.0 · November 15, 2026  
**Rights:** © 2026 We Develop Software LLC · WDS Public Research Reference License  
**Built:** React 19, Vite 7, TypeScript, React Router, Markdown/KaTeX, Lucide, scoped CSS, deterministic local examples.

## Purpose

Publish the project-owned neuroscience, process, software, and specialist chip documentation as a **readable static site**, with a living visual system atlas and interactive digital-only laboratories. It must run on GitHub Pages without an API server, container, database, interpreter service or live cell/chemical equipment. A separate server can later provide authenticated computational workloads without changing the public research content model.

## Data flow

~~~mermaid
flowchart TB
 Repo[Canonical existing GitHub source]
 Registry[Source-path and blob registry]
 Security[Public source path / extension checks]
 Builder[Node build-time importer]
 Docs[352 individual approved JSON documents]
 Search[Bounded full-content search index]
 Meta[Modules, manifests, molecular crosswalk]
 React[React static SPA]
 Pages[GitHub Pages /ChristEye-Knowledge/portal]
 Repo --> Registry --> Security --> Builder
 Repo --> Builder
 Builder --> Docs
 Builder --> Search
 Builder --> Meta
 Docs --> React
 Search --> React
 Meta --> React
 React --> Pages
~~~

### Content sources

- \`website/planning/content-registry.json\`: maps the original source file path, Git blob SHA, content type and planned route. It is used as the approved import manifest.
- \`scripts/prepare-content.mjs\`: refuses undeclared or unsafe paths, nontext files, generated secrets and patent-document filenames, then exports static content.
- \`public/content/manifest.json\`: website navigation and IDs, built from approved source files.
- \`public/content/documents/*.json\`: **lazy-loaded**, complete source text per document. No copy is hand-edited in the app.
- \`public/content/search-index.json\`: bounded lowercased source text for search; downloaded when user requests deep search.
- \`public/content/modules.json\`: original 24 scientific/engineering module profiles.
- \`public/content/chips.json\`: original 33 specialist module manifests.
- \`public/content/molecules.json\`: parsed research molecular and structural crosswalk with its stated evidence limitations.

The site build is verified against the original source paths and does not invent experimental measurements.

## Front-end components

| Source | View | Engineering intent |
|---|---|---|
| \`src/App.tsx\` | layout, navigation, Explore/Living/Science/Build/Scholarship screens | readable audience paths and source links |
| \`src/Atlas.tsx\` | selectable diagram with digital, living and research lenses | system boundaries visible and accurately described |
| \`src/Reader.tsx\` | searchable complete-document library, MD/GFM/math viewer, GitHub source links | full original documentation rather than summaries |
| \`src/Studio.tsx\` | Lotus, WindBus, Crystal, SustiBus, bandwidth, world-graph and music-event demos | safe, local digital examples with explicit scope |
| \`src/data.ts\` | strongly typed static data loader, source link resolver | stable content IDs and static hosting base path |
| \`src/styles.css\` | original responsive editorial design | dark/light mode, accessible interaction controls, long-form science readability |

All React routes use \`HashRouter\` to support static GitHub Pages deep links without server rewrite rules. The site source imports \`import.meta.env.BASE_URL\` from Vite; the base is set using \`ANAIERO_SITE_BASE\`.

## Deployment choices

**Selected beta:** Publish at \`/ChristEye-Knowledge/portal/\` by composing the build into the original \`docs/\` tree in the deployment artifact, leaving the current homepage and its documentation available. The workflow \`.github/workflows/react-publish.yml\` builds the source and attempts a GitHub Pages deployment with both sites in a single artifact. It does not commit generated data back into the canonical repository.

**Later permanent root:** Once the portal's content and accessibility pass owner review, set \`ANAIERO_SITE_BASE=/ChristEye-Knowledge/\` and make the React output the primary site; add redirects from old URLs where needed. Preserve the original Markdown and source assets.

**Own hosting:** Set \`ANAIERO_SITE_BASE=/\` when hosting at a domain root, run \`npm ci && npm run build\`, and copy all \`dist/\` contents to that public directory. Because routes use URL fragments, no web-server rewrite rule is needed. If served under a subpath, use that exact slash-delimited URL prefix.

**Future backend:** Real Python/GPU execution, persistent private accounts, and connected research instruments require a **separate service** with auth, permissioning, privacy, quota and independent physical safety. They cannot be added by merely serving a static site; the current version deliberately avoids pretending otherwise.

## Testing and safe evidence

GitHub CI runs source importer validation, TypeScript, Vite bundling, and Playwright browser checks. Screenshots and compiled artifacts are attached to the validation run. Browser demos are local reference models, not direct GPU StarChip or living-tissue execution.

To reproduce:

~~~bash
cd website/app
npm ci
npm run prepare
npm run test:content
npm run build
npx playwright install chromium
npm run test:e2e
~~~

Do not include the separate confidential patent document in any public asset; academic source material is used with original evidence labels and the existing WDS license. Historical research is clearly distinguishable from current module plans. Source status remains versioned and evidence-limited.

## Decisions superseding S01

S01 proposed Astro/Starlight. This implementation deliberately selects **React + Vite + HashRouter** for richer single-page interactions and a fully static Pages artifact. It preserves S01's navigation, science sources, disclosure rules, visual design vocabulary and evidence distinctions rather than replacing that planning history.

[Original site program plan](../README.md) · [Source registry](../planning/content-registry.json) · [WDS license](../../LICENSE.md).
