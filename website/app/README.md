# ChristEye Anaiero — React research portal

A static Vite + React + TypeScript site, deployed at `https://bradgearon.github.io/ChristEye-Knowledge/` with a GitHub Pages base path. It imports the existing scientific/engineering corpus at **build time**, not by scraping GitHub in the browser.

## Architecture

- **Source of truth:** existing `specifications/v3`, `processes`, `research`, `schemas`, `registries`, source examples, and the S01 `website/planning/content-registry.json`. The site does not maintain another copy of these source documents.
- **Build:** `scripts/prepare-content.mjs` verifies allowed source paths and writes public JSON `content/manifest.json`, individually lazy-loaded full documents, `content/search-index.json`, real module and StarChip registries, and molecule records. It refuses the confidential patent file, private logs and unsupported binary types.
- **UI:** React 19 + React Router HashRouter (reliable deep links on GitHub Pages without server-side rewrites), React Markdown + GFM + math, and accessible CSS. Interactive atlas, filtered library and playable **digital-only** simulations.
- **Release:** Actions installs, builds, checks, and deploys the static `dist`. No server, API keys, database, or live biological equipment is required.

## Develop

```bash
cd website/app
npm install
npm run dev
npm run build
npm run test:content
```

Requirements: Node 22.12+ recommended. The deployed site is entirely static. For direct source links see the [WDS repository](https://github.com/bradgearon/ChristEye-Knowledge).

## Evidence and rights

The public pages distinguish software test results, vendor/external scientific findings, Anaiero proposed biology and conceptual illustrations. A browser demo cannot control incubators, feed biological cells, or stimulate neural tissue. The actual patent document is excluded. © 2026 We Develop Software LLC; [WDS Public Research Reference License](../../LICENSE.md).
