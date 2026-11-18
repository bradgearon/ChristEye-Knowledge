# ChristEye Anaiero — Website and Publishing Program

**Current status: React/Vite + Cloudflare Workers Static Assets; S07 launch readiness in progress.**  
**Owner:** We Develop Software LLC · **Site:** https://anaiero.io/ · **Source:** bradgearon/ChristEye-Knowledge · **Revision:** November 15, 2026.

This directory hosts the public Anaiero website, source-linked scientific/engineering documentation, interactive digital prototypes and planned responsible research-interest collection. The physical patent document and private raw conversation logs are excluded.

## Current program

| Link | Why read it |
|---|---|
| [Website phases S01–S08](planning/07-release-plan.md) | Actual technology, phase gates, current status and next work |
| [S07 production launch runbook](app/S07_LAUNCH_RUNBOOK.md) | Steps to connect Workers, DNS, D1, Turnstile and do safe live checks |
| [Current site architecture](planning/06-technical-architecture.md) | React/Vite source ingestion, Worker/static boundary and deploy |
| [Full source inventory](planning/content-registry.json) | Audited project paths and planned page routes |
| [Design system and storyboards](planning/03-design-system.md) | Visual, typography, accessibility and page UX |
| [Scientific evidence model](planning/04-evidence-and-content-model.md) | Findings, designs, hypotheses and primary source scope |
| [Digital/StarChip demos](planning/05-interactive-labs-and-starchips.md) | Browser simulations, fixture/evidence requirements |
| [Cloudflare deployment instructions](app/CLOUDFLARE_DEPLOY.md) | Main app build and domain guide |
| [Interest signup checklist](app/INTEREST_ROLLOUT.md) | Explicit consent and D1/Turnstile activation |
| [Archived S01 architecture proposal](planning/06-technical-architecture-s01-archived.md) | Original Astro/Starlight planning record |

## Staging environment

The project owner reports a live dev site at https://anaiero.bradgearon.workers.dev/. Its staging config and isolated-test checklist live in [S07_DEV_STAGING.md](app/S07_DEV_STAGING.md). Cloudflare account resources and staging interest submissions have not been verified with a connector. The intended anaiero.io production site remains a separate release target.

## Architecture and deployment status

**Already in GitHub:** Vite/React app, corpus importer, technical document reader, Lotus/WindBus/Root demonstrations, the Worker API, D1 migration and consent form. CI tests verify builds and synthetic behavior, but those results **do not prove anaiero.io is deployed**. This session could not externally verify the domain, and no Cloudflare account connector is available here.

**Requires owner-controlled configuration:** Cloudflare Worker application with active anaiero.io Custom Domain and HTTPS. To activate signup, also configure the D1 binding and migration, Turnstile widget/site key/secret, monitored WDS privacy contact and one successful controlled registration test. No separate R2/static-storage purchase is required for the site.

**Scientific distinction:** digital prototypes are not evidence that the complete intelligent living Soil, physically copied memory Crystals, or peptide-delivered semantic knowledge have been demonstrated.

## Reproducible validation

From website/app after npm ci and a root-domain Vite build:

~~~bash
ANAIERO_SITE_BASE=/ npm run build
node scripts/launch-preflight.mjs --offline
~~~

After the Cloudflare site is live:

~~~bash
node scripts/launch-preflight.mjs --live
# After interest is configured:
node scripts/launch-preflight.mjs --live --require-interest
~~~

These are read-only or offline checks. They never collect personal details or deploy to Cloudflare.

## Source of truth and ownership

Project research stays in specifications/, processes/, research/, phases/, schemas/ and registries/. Build-time generated content should not diverge from those sources. © 2026 We Develop Software LLC; [WDS Public Research Reference License](../LICENSE.md). Actual patent documents remain unpublished.

Historical [S01 blueprint](planning/00-s01-original-website-readme.md) is preserved rather than rewritten.

## React portal and GitHub Pages preview

The React/Vite portal is the current implementation of the source-linked research site. It supports the GitHub Pages beta at `/ChristEye-Knowledge/portal/` as well as independent static hosting; its architecture and build instructions are in [the app README](app/README.md). The production Cloudflare deployment and GitHub Pages preview are separate targets.
