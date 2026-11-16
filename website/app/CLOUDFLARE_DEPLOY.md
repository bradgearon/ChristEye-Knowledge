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

The Cloudflare-specific configuration is in website/app/wrangler.jsonc. It is static-only and has no backend or database permissions. The current GitHub Pages site remains available as fallback.

## One-time dashboard steps

1. Add anaiero.io as an active Cloudflare DNS zone in your account. If the domain was bought elsewhere, follow Cloudflare's provided nameserver directions at the registrar.
2. Go to Workers & Pages -> Create Application -> Connect GitHub. Grant access to bradgearon/ChristEye-Knowledge.
3. Choose production branch main once the Cloudflare release PR has been merged. Until then, use deploy/cloudflare-anaiero-io only for review.
4. Set Root directory: website/app
5. Set Build command exactly:
   npm ci --no-audit --no-fund && ANAIERO_SITE_BASE=/ npm run build && node scripts/verify-cloudflare-build.mjs
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
npx --yes wrangler@4 deploy
~~~

The final deploy requires authorization in your Cloudflare account. GitHub CI verifies packaging without credentials; it is not a real live deployment.

## Interest form

The passing public React portal is static and does NOT yet collect email interest. No signups are saved. The future service should use a small Worker API, Cloudflare Turnstile server-side verification, an optional D1 database, explicit consent and a properly configured privacy contact and confirmation/unsubscribe process. It should be activated only after those are configured.

## Subdomains and later expansion

The main site can host documentation and science under paths first. Dedicated docs.anaiero.io and science.anaiero.io Workers can be configured later. No separate asset-storage plan is required to create those domains.

## Rollback

Roll back the Worker deployment from Cloudflare if a production update fails. The original GitHub Pages docs remain accessible at https://bradgearon.github.io/ChristEye-Knowledge/.

Copyright © 2026 We Develop Software LLC. WDS Public Research Reference License. The physical patent document is excluded.
