---
title: "Anaiero.io Website Program S01–S08 — Current Release Roadmap"
revision: "2026-11-15"
owner: "We Develop Software LLC"
status: "Current implementation-aware roadmap: React/Vite + Cloudflare Workers"
---

# ChristEye Anaiero website phases — implementation-aware roadmap

**This roadmap describes the website, not the scientific P00–P08 program.** Historical S01 planning documents proposed Astro/Starlight and GitHub Pages. The actual application now uses **React 19, Vite, TypeScript, the source-driven Markdown importer, GitHub Actions and Cloudflare Workers Static Assets**. The source research and scientific designs remain under their existing project folders. The old Astro plan is preserved in [06-technical-architecture-s01-archived.md](06-technical-architecture-s01-archived.md) for provenance.

Every phase has an outcome and a proof gate. A passing simulated or software test is not evidence of living neural computation.

## Current S01–S08 status

| Phase | Goal | Evidence in repository | Current gate |
|---|---|---|---|
| **S01 — Content and design foundation** | Source audit, information architecture, page designs, scientific evidence classes and demo contracts | This planning directory, 352-source initial registry and design tokens | **Delivered as planning** |
| **S02 — React website engine** | Working reader, React/Vite app, source importer, responsive navigation and stable builds | website/app, locked npm install, Vite and browser tests | **Software implemented; build checks required on changes** |
| **S03 — Full technical library** | Designs, companions, processes, bibliography, JSON and historical research searchable in site | Build-time corpus output, manifest, technical-document reader and CI corpus checks | **Core importer present; full content and citation UX still improving** |
| **S04 — Living system atlas** | Explore Soil, Crest, axons, spinal Roots, Lotus, Milk/Honey and multi-bus boundaries | Interactive portal architecture and existing BIO process specs | **Partially implemented** |
| **S05 — StarChip laboratory** | Executable digital demonstrations, specialist output previews and reproducibility links | Portal studio and digital-only reference fixtures; chip manifests | **Partially implemented; complex creative chips not yet running** |
| **S06 — Science, scholarship and journal** | Evidence/claims, molecular atlas, formal models, scholarly interpretation and posts | Research pages, diagrams, source corpus; posting workflow pending refinement | **Partially implemented** |
| **S07 — Cloudflare launch and interest** | anaiero.io HTTPS, Worker static assets + /api, opt-in form, D1/Turnstile and privacy | wrangler.jsonc; Worker, migration, tests, launch preflight | **Code prepared; account deployment and D1/Turnstile activation not verified** |
| **S08 — Ongoing operations** | Reproducible releases, research updates, StarChip demos, communications, maintenance | CI and documented roadmaps provide a starting framework | **Future operating program** |

The readiness state above is not a numerical percent. **Main's current CI** is the authoritative test status; a green build is distinct from Cloudflare deployment or actual data collection.

## S07 development staging — existing workers.dev site

The WDS owner reports a live development site at **https://anaiero.bradgearon.workers.dev/**. Treat it as staging, not proof of the separate production custom domain. The dedicated [development Worker configuration](../app/wrangler.staging.jsonc) targets Worker **anaiero**, while [production](../app/wrangler.jsonc) targets **anaiero-io**.

The new launch preflight supports **--live --target=dev**; the manual GitHub live smoke workflow has a development/production selector. Cloudflare's workers.dev Browser Integrity Check may challenge automatic clients, so a script error must be compared with ordinary browser access before diagnosing site failure.

An optional real staging opt-in requires an isolated D1 database named **anaiero-interest-dev**, staging-only Turnstile widget and a monitored WDS privacy contact. No staging D1 ID, secret or company address has been configured in this repository. See the [development runbook](../app/S07_DEV_STAGING.md).

## S07 — the active launch gate

| Gate | Required result | Proof | Current status |
|---|---|---|---|
| S07-01 Static artifact | Production assets at the domain root, no GitHub Pages base prefix | node scripts/launch-preflight.mjs --offline | Automated; run in PR CI |
| S07-02 Source integrity | Indexed research readable; patent document and personal logs absent | content manifest, release flags and source tests | Automated; run in PR CI |
| S07-03 Worker safety | /api/* is Worker-first; other assets static; no biological actuation | Wrangler dry run + Worker tests | Automated; run in PR CI |
| S07-04 Live deployment | anaiero.io loads the intended site over valid HTTPS | domain-root smoke test | **Unverified until Cloudflare owner connects account** |
| S07-05 Public API | GET /api/health = ok; config endpoint returns valid status | GitHub Actions manual smoke test | **Unverified until domain is live** |
| S07-06 D1 initialization | Database anaiero-interest, DB binding, migration applied | Cloudflare D1 dashboard / authorized Wrangler CLI | **Not configured in repo** |
| S07-07 Turnstile and consent | Site key, protected secret, hostname, action and privacy mailbox | Disabled-by-default API and negative tests; live form verification | **Must be configured in Cloudflare** |
| S07-08 Live interest test | Explicit consent saved in private D1; status awaiting_confirmation, invalid token rejected | One owner-controlled form test and D1 record audit | **Not performed** |
| S07-09 Newsletter | Confirmed email, unsubscribe, suppression and privacy retention | Separate approved mail-provider and legal operating workflow | **Not part of interest-only launch** |
| S07-10 Rollback | Previous Worker version can be restored; GitHub Pages docs stay accessible | Cloudflare Versions/Deployments and recovery notes | **Operator release check needed** |

**Launch order:** static site first; interest collection second. No separate R2/static-storage purchase is needed. Dynamic request and database usage remain subject to Cloudflare's plan limits.

## How to verify

After building website/app:

~~~bash
node scripts/launch-preflight.mjs --offline
~~~

After an authorized Cloudflare deployment to the custom domain:

~~~bash
node scripts/launch-preflight.mjs --live
~~~

After completing D1 + Turnstile + privacy configuration:

~~~bash
node scripts/launch-preflight.mjs --live --require-interest
~~~

These commands never submit an email or inspect private D1 records. The **final real signup acceptance** still requires an authorized owner test; a healthy API response alone does not prove D1 writes succeed.

GitHub also provides a manual [live production smoke workflow](../../.github/workflows/cloudflare-live-smoke.yml). See [S07 launch runbook](../app/S07_LAUNCH_RUNBOOK.md).

## Quality and disclosure requirements

1. **Source integrity:** import the current original WDS designs/companions; keep historical editions explicitly labeled.
2. **Scientific interpretation:** software, published external neuroscience, local experiment, design and hypothesis claims are separate.
3. **Care safety:** no browser or Worker can feed, stimulate or grow tissue; sustaining care never follows task reward.
4. **Privacy:** no personal emails in public source, logs or GET endpoints; clear consent; Turnstile verified server-side.
5. **Rights:** WDS Public Research Reference License visible; confidential physical patent document excluded.
6. **Accessibility:** mobile, keyboard, headings, diagrams, math and link audits pass before major public announcements.
7. **Operations:** reproducible builds, Cloudflare rollback, explicit monitoring and named privacy contact.
8. **No premature newsletter promise:** interest-only state is not a confirmed subscription.

## Next after S07

**S08** focuses on scheduled journal updates, source-linked research publication, version-aware technical documentation, measured interest conversion, legitimate confirmed-email communications, and richer StarChip demos. The original *scientific* roadmap remains [docs/roadmap.md](../../docs/roadmap.md): physical boards and living experiments are research-gated, regardless of how complete the public website becomes.

© 2026 We Develop Software LLC · [WDS Research Reference License](../../LICENSE.md).
