---
title: "Anaiero.io Technical Site Architecture — React and Cloudflare"
revision: "2026-11-15"
status: "Current implementation record"
owner: "We Develop Software LLC"
---

# The current Anaiero.io technology, not the superseded Astro proposal

The S01 plan originally considered Astro/Starlight and GitHub Pages. **The implemented site is React + Vite + TypeScript**, with static assets served through **Cloudflare Workers Static Assets**. The original design proposal is retained at [06-technical-architecture-s01-archived.md](06-technical-architecture-s01-archived.md) for historical traceability.

## Source-to-site pipeline

~~~text
ChristEye-Knowledge (GitHub, main)
  ├─ WDS scientific designs and companions
  ├─ 12+ controlled processes and neuroscience references
  ├─ JSON schemas, module manifests and scientific source text
  ├─ website/app/scripts/prepare-content.mjs
  │    └─ build-time JSON documents, source registry, release manifest, figures
  └─ website/app
       ├─ React 19 + TypeScript + React Router HashRouter
       ├─ React Markdown, GFM and math
       ├─ Vite build → dist/
       ├─ Worker script: worker/index.js
       │    ├─ GET /api/health
       │    ├─ GET /api/interest/config
       │    └─ POST /api/interest (disabled until configured)
       ├─ Cloudflare Workers Static Assets: all other static requests
       └─ D1 interest_leads (optional; not configured until Cloudflare owner setup)
~~~

A successful local/CI build produces a **static public artifact**, not a biological experiment or a Cloudflare deployment receipt. The Worker handles /api/* only, preserving low-cost static delivery for the rest.

## Repository and build contract

- **Root directory:** website/app
- **Node:** 22
- **Install:** npm ci --no-audit --no-fund
- **Build:** ANAIERO_SITE_BASE=/ npm run build
- **Artifact check:** node scripts/verify-cloudflare-build.mjs
- **Release preflight:** node scripts/launch-preflight.mjs --offline
- **Deploy:** npx --yes wrangler@4 deploy, authorized within Cloudflare
- **Worker name:** anaiero-io, with dist assets, SPA not-found handling, ASSETS binding and /api/* routing

The actual HashRouter uses fragment routes (for example https://anaiero.io/#/interest). Do **not** assume clean History API routes are configured before explicitly migrating them.

## Custom domain and subdomains

The current deployment target is the apex domain **anaiero.io**, with www redirect or secondary custom domain optional. Begin with /#/library, /#/science, /#/studio and /#/interest within the same Worker/site; later split docs.anaiero.io or science.anaiero.io only if there is a compelling operational reason.

## Source fidelity and claim boundaries

The compiler should reflect original non-patent WDS source documents, retain named CE-Axx, BIO-xx, evidence/experimental status and original Git source links, and never stage the confidential patent document or private conversations. Software and proposed biological mechanisms remain separately labeled. The runtime has no physical care, stimulation or cell-production commands.

## API and secure interest collection

The optional Worker API requires an explicit DB binding to a private D1 database, a public Turnstile site key, private TURNSTILE_SECRET, real PRIVACY_CONTACT, and allowed domain. No account secrets belong in GitHub. Registration stays disabled (503) until all bindings exist. New entries are awaiting_confirmation only, not authorized for marketing delivery. Email confirmation/unsubscribe comes later.

## Deployment, CI and launch checks

GitHub Actions builds the React site, tests content/source exclusion and Worker registration behavior, then runs a Wrangler dry run. S07 now includes a local artifact preflight and a manual **read-only** live domain check. Neither validates a real D1 write. The owner must attach the Cloudflare account, verify HTTPS and run a controlled signup before declaring research-interest collection live.

Detailed operator steps: [S07 launch runbook](../app/S07_LAUNCH_RUNBOOK.md), [Cloudflare deploy guide](../app/CLOUDFLARE_DEPLOY.md) and [interest setup](../app/INTEREST_ROLLOUT.md).

## Rights and provenance

All non-patent research is WDS-owned under the [WDS Public Research Reference License](../../LICENSE.md). The historical Astro plan remains accessible for planning provenance but is no longer the implementing architecture. GitHub Pages documentation remains a fallback rather than the intended primary host for anaiero.io.
