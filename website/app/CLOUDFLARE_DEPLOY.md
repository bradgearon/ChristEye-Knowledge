# Anaiero.io — Cloudflare Workers deployment

Revision: November 15, 2026. Owner: We Develop Software LLC.

## Cost

Workers Static Assets requires no separate object-storage purchase. Static asset requests are free and unlimited, and there is no additional charge for static asset storage. The free Workers tier includes an allowance for future dynamic endpoints. D1 or R2 is not required to host this particular React SPA.

Sources:
- https://developers.cloudflare.com/workers/static-assets/billing-and-limitations/
- https://developers.cloudflare.com/workers/platform/limits/
- https://developers.cloudflare.com/workers/ci-cd/builds/

## Source and release

This deployment uses the already tested React portal at website/app in the ChristEye-Knowledge repository. The build transforms the original source registry into static documentation; it does not publish the confidential patent document. It uses HashRouter, so links may include a URL fragment such as /#/studio. Upgrade to clean URLs later without mixing that change into first deployment.

The Cloudflare-specific configuration is in website/app/wrangler.jsonc. It includes an API Worker entrypoint for /api/* while ordinary assets remain static. The code now supports opt-in interest registration, **but registration remains disabled without D1, Turnstile and a monitored privacy contact; no deployed configuration has been verified in this conversation**. The current GitHub Pages site remains available as fallback.

## One-time dashboard steps

1. Add anaiero.io as an active Cloudflare DNS zone in your account. If the domain was bought elsewhere, follow Cloudflare's provided nameserver directions at the registrar.
2. Go to Workers & Pages -> Create Application -> Connect GitHub. Grant access to bradgearon/ChristEye-Knowledge.
3. Choose production branch main once the Cloudflare release PR has been merged. Until then, use deploy/cloudflare-anaiero-io only for review.
4. Set Root directory: website/app
5. Set Build command exactly:
   npm ci --no-audit --no-fund && ANAIERO_SITE_BASE=/ npm run build && node scripts/verify-cloudflare-build.mjs && node scripts/launch-preflight.mjs --offline
6. Set Deploy command exactly:
   npx --yes wrangler@4 deploy
7. Save and Deploy. Check the generated workers.dev address, homepage, document library, science section, interactive lab and mobile navigation before attaching the domain.
8. Worker -> Settings -> Domains & Routes -> Add -> Custom Domain -> anaiero.io. Cloudflare issues HTTPS certificate and creates appropriate DNS records for the active zone.
9. Add www.anaiero.io separately or configure www-to-apex redirect, per Cloudflare Custom Domains guidance.

Docs:
- https://developers.cloudflare.com/workers/ci-cd/builds/configuration/
- https://developers.cloudflare.com/workers/configuration/routing/custom-domains/

Cloudflare builds can automatically authorize their own deployments through GitHub integration; do not put API tokens or passwords in repository code or in chat.

## Verification

For a local build, run from website/app:

~~~bash
npm ci --no-audit --no-fund
ANAIERO_SITE_BASE=/ npm run build
node scripts/verify-cloudflare-build.mjs
node scripts/launch-preflight.mjs --offline
npx --yes wrangler@4 deploy
~~~

The final deploy requires authorization in your Cloudflare account. GitHub CI verifies packaging without credentials; it is not a real live deployment.

## Troubleshooting: response missing default_environment.script

If Cloudflare shows:

`WorkerResource.getWorkerResult: response missing default_environment.script`

the dashboard is asking for a Worker script it cannot find in the current Worker resource metadata. **This is not a storage-plan charge or a Vite TypeScript error.** Cloudflare supports assets-only deployments, so the error may indicate an uninitialized Worker record or a dashboard state issue, rather than being proof that the earlier Wrangler configuration was invalid.

The repository now supplies `main: "./worker/index.js"` and `assets.binding: "ASSETS"` in `wrangler.jsonc`. This creates a real Worker entry point while preserving Cloudflare's static-asset-first routing and reserving `/api/*` for a future explicitly enabled API. A route to this code does not by itself enable registrations; the Worker fails closed until every required binding and runtime secret exists.

**Recommended recovery, in order:**

1. In Cloudflare, go to **Workers & Pages** and inspect whether a Worker named **anaiero-io** exists. The project name must match the `name` in the configuration.
2. If it exists and has a **Settings → Builds** section, connect GitHub `bradgearon/ChristEye-Knowledge` on branch `main` (or retry the latest build if already connected). Root directory: `website/app`. Confirm the build/deploy commands below.
3. If Cloudflare cannot load that Worker due to missing script metadata, create a **new Worker using the Hello World template**, named `anaiero-io` if available, and deploy that initial script. Then open **Settings → Builds → Connect** to attach GitHub. This establishes an existing Worker with a default script before the full React build replaces it.
4. If the name is already held by an incomplete resource, do not delete a live deployment blindly. Use a temporary Worker name for the bootstrap, or correct/remove only the unused broken placeholder after checking it has no active traffic; the Wrangler `name` must be changed to match any different name.
5. Alternatively, on a computer with Git and Node 22, run `npx wrangler login` in `website/app`, then `ANAIERO_SITE_BASE=/ npm run build` and `npx wrangler deploy`; this can create the Worker directly before connecting the GitHub integration. Keep Cloudflare credentials out of chat and source.

Cloudflare officially documents the [existing-Worker connection flow](https://developers.cloudflare.com/workers/ci-cd/builds/) and [Worker-script/asset routing](https://developers.cloudflare.com/workers/static-assets/). The exact error string has not been confirmed as a documented Cloudflare bug; if it persists after successful deployment, collect the dashboard page URL and Cloudflare build log without exposing secrets.

## Interest registration

The public React portal now contains a research-interest and privacy page, plus a consent-checked Worker API. **Registration is disabled by default** until a D1 binding, Turnstile keys and an active privacy mailbox are configured. No email is automatically sent. See [INTEREST_ROLLOUT.md](INTEREST_ROLLOUT.md) and [S07_LAUNCH_RUNBOOK.md](S07_LAUNCH_RUNBOOK.md). Once the domain is live, the read-only check is `node scripts/launch-preflight.mjs --live`.

## Subdomains and later expansion

The main site can host documentation and science under paths first. Dedicated docs.anaiero.io and science.anaiero.io Workers can be configured later. No separate asset-storage plan is required to create those domains.

## Rollback

Roll back the Worker deployment from Cloudflare if a production update fails. The original GitHub Pages docs remain accessible at https://bradgearon.github.io/ChristEye-Knowledge/.

Copyright © 2026 We Develop Software LLC. WDS Public Research Reference License. The physical patent document is excluded.
