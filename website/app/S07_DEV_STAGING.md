# S07 — Anaiero Cloudflare development/staging release

**Development URL:** https://anaiero.bradgearon.workers.dev/  
**Owner:** We Develop Software LLC · **Status:** staging config prepared; account bindings not verified.

You confirmed the development website is live. That makes this a staging site for reviewed, non-sensitive experiments. It does not mean the separately owned anaiero.io domain or production registration database has been deployed.

## Existing Worker identity and safety

Cloudflare workers.dev hostnames use WORKER.ACCOUNT-SUBDOMAIN.workers.dev. The public staging hostname suggests the existing Worker name is **anaiero**. Verify that name in Cloudflare before changing its build settings.

- **wrangler.staging.jsonc** targets Worker **anaiero** on the existing workers.dev hostname.
- **wrangler.jsonc** targets production Worker **anaiero-io**, intended for anaiero.io.
- Staging allowlist: **anaiero.bradgearon.workers.dev** only.
- Both serve the same React static bundle with /api/* routed to the Worker script.
- The staging config contains **no D1 database binding, Turnstile key or privacy contact** and therefore cannot activate interest collection on its own.
- Never bind the staging Worker to the production D1 database, and never put Cloudflare API credentials or Turnstile secrets in GitHub.

Cloudflare documentation confirms workers.dev is intended for development and hobby use, not the primary commercial custom domain. It may also challenge scripted access via Browser Integrity Check. See https://developers.cloudflare.com/workers/configuration/routing/workers-dev/

## Configure the existing development Worker

First open Cloudflare Dashboard → Workers & Pages → **anaiero** → Settings → Builds. Confirm that this is the Worker currently serving the URL above; do not create a duplicate or delete its deployment. If connected to the GitHub repository bradgearon/ChristEye-Knowledge, use:

**Repository:** bradgearon/ChristEye-Knowledge  
**Branch:** main after this PR passes tests and merges  
**Root directory:** website/app

**Build command:**

~~~bash
npm ci --no-audit --no-fund && ANAIERO_SITE_BASE=/ npm run build && node scripts/verify-cloudflare-build.mjs
~~~

**Staging-specific deploy command:**

~~~bash
npx --yes wrangler@4 deploy --config wrangler.staging.jsonc
~~~

For the future anaiero.io production Worker, retain the default wrangler.jsonc and regular Wrangler deploy command. Do not switch the two names.

## Test the real development URL without any private data

~~~bash
cd website/app
node scripts/launch-preflight.mjs --live --target=dev
~~~

The manual GitHub Actions smoke workflow also allows selecting **development** rather than production. The check sends **only GET requests** to the homepage, Worker health and interest-config endpoints. It cannot create leads or read the D1 database.

If the check fails with Cloudflare's browser integrity protection but the page is visible in a browser, treat it as a possible automated-client challenge. The script uses a descriptive User-Agent as recommended by Cloudflare.

## Optional development-only research-interest trial

If you want to test a real signup in staging, use a **different D1 database** from production, with test emails only.

1. Create D1 **anaiero-interest-dev** in the same Cloudflare account as Worker anaiero.
2. Add a DB binding for that staging D1 UUID to **wrangler.staging.jsonc**. Never reuse the production database ID. D1 IDs are configuration, not secret keys.
3. Apply the existing migration to the staging database. After the staging binding exists:

~~~bash
npx wrangler d1 migrations apply anaiero-interest-dev --remote --config wrangler.staging.jsonc
~~~

4. Create a Turnstile widget with **anaiero.bradgearon.workers.dev** as the permitted hostname. Check the dashboard accepts the exact hostname. Do not authorize the entire workers.dev parent domain.
5. Set these in the staging Worker environment:
   - ALLOWED_HOSTNAMES = anaiero.bradgearon.workers.dev (already in staging config)
   - TURNSTILE_SITE_KEY = public site key for this staging widget
   - TURNSTILE_SECRET = private secret configured **only as a Worker Secret**
   - PRIVACY_CONTACT = monitored WDS privacy/removal email
6. Deploy with the staging-specific Wrangler config, then verify GET https://anaiero.bradgearon.workers.dev/api/interest/config returns enabled:true.
7. Submit one WDS-controlled test address through https://anaiero.bradgearon.workers.dev/#/interest. Verify the D1 row is marked awaiting_confirmation and invalid tokens do not write records. Remove the test data through authorized D1 administration.

**Important:** the optional D1 binding must be saved in the appropriate account/deployment configuration before starting real data collection. An empty database binding is not a functioning form; a working widget without a verified secret is not an acceptable anti-spam system.

## Separate production activation later

On anaiero.io, use **anaiero-io**, a different D1 resource named **anaiero-interest**, a different Turnstile widget, a real company privacy mailbox and the WDS-reviewed consent policy. Connect the domain via Cloudflare Custom Domain, which handles DNS and HTTPS on an active Cloudflare zone.

Unconfirmed D1 interest entries are **not** email subscribers. A future mailer requires confirmed opt-in, unsubscribe and suppression lists, verified sender authentication and data removal procedures.

## Where to read more

- [S07 website launch runbook](S07_LAUNCH_RUNBOOK.md)
- [Interest registration rollout](INTEREST_ROLLOUT.md)
- [Current website roadmap](../planning/07-release-plan.md)
- [Cloudflare Preview resource isolation](https://developers.cloudflare.com/workers/previews/resources/)
- [Cloudflare Turnstile hostname management](https://developers.cloudflare.com/turnstile/additional-configuration/hostname-management/)

Copyright © 2026 We Develop Software LLC · WDS Public Research Reference License. The physical patent document remains excluded.
