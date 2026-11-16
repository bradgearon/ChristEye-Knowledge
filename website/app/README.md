# ChristEye Anaiero — React research portal

A Vite + React + TypeScript technical research portal. Its production target is **https://anaiero.io/** through Cloudflare Workers Static Assets; GitHub Pages remains an existing preview/fallback. The Cloudflare Worker serves only /api/* and interest capture remains disabled until D1, Turnstile and privacy details are configured. It imports the existing scientific/engineering corpus at **build time**, not by scraping GitHub in the browser.

## Architecture

- **Source of truth:** existing `specifications/v3`, `processes`, `research`, `schemas`, `registries`, source examples, and the S01 `website/planning/content-registry.json`. The site does not maintain another copy of these source documents.
- **Build:** `scripts/prepare-content.mjs` verifies allowed source paths and writes public JSON `content/manifest.json`, individually lazy-loaded full documents, `content/search-index.json`, real module and StarChip registries, and molecule records. It refuses the confidential patent file, private logs and unsupported binary types.
- **UI:** React 19 + React Router HashRouter (reliable deep links on GitHub Pages without server-side rewrites), React Markdown + GFM + math, and accessible CSS. Interactive atlas, filtered library and playable **digital-only** simulations.
- **Release:** Actions installs, builds, checks, and deploys the static `dist`. No external backend or live biological equipment is needed for the static knowledge site; the opt-in registration service separately requires approved D1 and Turnstile configuration.

## Develop

```bash
cd website/app
npm install
npm run dev
npm run build
npm run test:content
```

Requirements: Node 22.12+ recommended. The primary document bundle is static; the optional Worker API handles public research-interest registration when explicitly configured. For direct source links see the [WDS repository](https://github.com/bradgearon/ChristEye-Knowledge).

## Evidence and rights

The public pages distinguish software test results, vendor/external scientific findings, Anaiero proposed biology and conceptual illustrations. A browser demo cannot control incubators, feed biological cells, or stimulate neural tissue. The actual patent document is excluded. © 2026 We Develop Software LLC; [WDS Public Research Reference License](../../LICENSE.md).

## Public beta and independent hosting

The Vite base path is configurable through `ANAIERO_SITE_BASE`. For the GitHub Pages beta published under `/ChristEye-Knowledge/portal/`, the release workflow uses `ANAIERO_SITE_BASE=/ChristEye-Knowledge/portal/`. For production at the repository root, use `/ChristEye-Knowledge/`. For a custom web server hosted at the domain root, use `/`.

The output directory `dist/` is a self-contained static artifact; copy its entire contents to the configured public URL path. No Python backend is necessary for the existing interactive digital simulations. The generated corpus is **public** by design, so do not add credentials, personal logs, or confidential patent documents to the import registry.

Run `npm ci && npm run build && npm run test:content` from this directory. The build must succeed before any Pages deployment.

## Current S07 deployment and verification

See [S07 launch runbook](S07_LAUNCH_RUNBOOK.md), [deployment guide](CLOUDFLARE_DEPLOY.md), and [interest activation](INTEREST_ROLLOUT.md). After a root-domain build, use `node scripts/launch-preflight.mjs --offline`. Once the domain is live, use `node scripts/launch-preflight.mjs --live` or the manual GitHub Actions smoke test. No Cloudflare account credentials are stored in Git.

## Development Worker and safe testing

The owner-reported staging URL is https://anaiero.bradgearon.workers.dev/. Use the **staging Worker config** [wrangler.staging.jsonc](wrangler.staging.jsonc), the [S07 dev checklist](S07_DEV_STAGING.md), and read-only preflight `node scripts/launch-preflight.mjs --live --target=dev`. Do not apply the production D1 database or production Turnstile secrets to the staging Worker. Any actual Cloudflare account deployment must be authorized by the owner.
