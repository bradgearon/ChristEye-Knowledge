# S07 Launch Runbook — anaiero.io

**ChristEye Anaiero · We Develop Software LLC · November 15, 2026**  
**Status:** account-independent checks implemented; real Cloudflare deployment and interest activation **not verified**.

The current application is a React/Vite static site with a Cloudflare Worker for /api/* and a private optional D1 interest database. There is **no separate object storage bucket to purchase** for these compiled static assets. Use the existing GitHub repository as the deployment source and keep the existing GitHub Pages documentation as a fallback.

## 1. Do not skip the production boundary

The GitHub build is a *candidate artifact*. Until anaiero.io is linked in Cloudflare, **nobody has demonstrated that the current app is live at that hostname**. A green GitHub Actions run or Wrangler dry-run is not a live deployment.

The Worker API is **fail-closed**: in the absence of a valid DB binding, Turnstile configuration and monitored privacy contact, GET /api/interest/config reports enabled:false and POST /api/interest returns 503. A web visitor is never promised an active signup when that configuration is absent.

## 2. Deploy the static website first

1. Open Cloudflare Dashboard → Workers & Pages. Confirm whether Worker **anaiero-io** already exists; if so, reuse it rather than accidentally creating a competing Worker.
2. Confirm **anaiero.io** is an active Cloudflare zone. If domain registration uses another registrar, follow Cloudflare's nameserver instructions there. DNS and domain ownership remain controlled by the owner.
3. In the Worker's **Builds** settings, connect GitHub repository **bradgearon/ChristEye-Knowledge**, production branch **main**.
4. Use root directory **website/app**.
5. Build command:

~~~bash
npm ci --no-audit --no-fund && ANAIERO_SITE_BASE=/ npm run build && node scripts/verify-cloudflare-build.mjs && node scripts/launch-preflight.mjs --offline
~~~

6. Deploy command:

~~~bash
npx --yes wrangler@4 deploy
~~~

7. Confirm the Worker is deployed with assets.directory = ./dist and run_worker_first = ["/api/*"]. Wrangler configuration is in [wrangler.jsonc](wrangler.jsonc). No live biological interfaces are attached.
8. In Worker Settings → Domains & Routes → Add Custom Domain → **anaiero.io**. Let Cloudflare provision the DNS record and certificate. Decide whether **www.anaiero.io** should redirect to the apex (recommended) or be another Custom Domain.
9. Test the root homepage, HashRouter links such as **https://anaiero.io/#/library**, science atlas, Lotus/StarChip demos and the new **/#/interest** disabled state.

Do not enable the interest service before the static site and privacy page have been reviewed.

## 3. Verify publicly available endpoints (read-only)

From any Node 22 workstation, after checkout:

~~~bash
cd website/app
node scripts/launch-preflight.mjs --live
~~~

Or open [GitHub Actions → Anaiero live production smoke test](https://github.com/bradgearon/ChristEye-Knowledge/actions/workflows/cloudflare-live-smoke.yml) and run it with **require_interest = false**.

The test requests only:
- GET https://anaiero.io/ — HTTPS and recognizable Anaiero HTML;
- GET https://anaiero.io/api/health — Worker returns status=ok;
- GET https://anaiero.io/api/interest/config — well-formed enabled true/false status.

It does not send or read any email addresses. If it fails, inspect DNS, hostname, GitHub Build configuration, Worker deploy logs and custom-domain routes first. Do not treat failed access as proof of a Cloudflare pricing issue.

## 4. Activate opt-in research interest only after privacy review

**Prerequisites controlled by the WDS owner:** a real monitored privacy email contact, the agreed consent and retention policy, Cloudflare zone/Worker access, and Turnstile widget for the correct hostname.

1. In the **same Cloudflare account**, create D1 database named **anaiero-interest** using the dashboard or authenticated CLI:

~~~bash
cd website/app
npx wrangler d1 create anaiero-interest
~~~

2. Copy the resulting **database_id** into the D1 binding in [wrangler.jsonc](wrangler.jsonc). See the nondeployable [binding example](wrangler.d1.example.jsonc). The UUID is configuration, not a secret, but it should come from the actual D1 resource. Commit the D1 binding through GitHub so future builds preserve it.
3. Apply the versioned [D1 migration](migrations/0001_interest.sql) to the **remote database**, after confirming the binding and backup/change window:

~~~bash
npx wrangler d1 migrations apply anaiero-interest --remote
~~~

4. Create a Cloudflare Turnstile widget with allowed hostnames **anaiero.io** (and **www.anaiero.io** if used).
5. Add Worker variables/secrets (not GitHub source, not chat):
   - TURNSTILE_SITE_KEY: public widget key, Worker variable.
   - TURNSTILE_SECRET: **private Worker Secret**.
   - PRIVACY_CONTACT: monitored privacy-removal email, Worker variable.
   - ALLOWED_HOSTNAMES (optional): exact active hostnames, default anaiero.io,www.anaiero.io.
6. Deploy the version with the binding. GET https://anaiero.io/api/interest/config must return enabled:true, the public site key and public contact; **never the private secret**.
7. Use the form at https://anaiero.io/#/interest with a WDS-controlled test address. Verify a successful 202 response, an **awaiting_confirmation** record in D1, and correct source/consent fields. Test one invalid/expired token and a refusal. Remove the test row under the approved data-management process.
8. Run the manual GitHub smoke workflow again with **require_interest = true**. This validates configuration readback, **not** that real D1 writes work; the previous manual test provides that evidence.

Turnstile validation is server-side. The current service is not a newsletter engine: it records expressed interest but does not verify email ownership or send messages.

## 5. Before emailing people

Confirm an approved mail provider and sender domain, double-opt-in workflow, consent confirmation receipt, unsubscribe and suppression handling, retention schedule, security roles and a process for access/removal requests. Do not treat an unconfirmed interest lead as a subscriber or email the list just because D1 contains addresses.

## 6. Operational acceptance and rollback

| Check | What the operator verifies | Evidence |
|---|---|---|
| Public HTTPS | Main page loads at anaiero.io and domain certificate valid | Read-only live smoke |
| Correct source | Deployed build includes current docs, science and interactive digital-only content | Built artifact check and live site review |
| Static assets | JavaScript/CSS assets and document JSON load | Cloudflare deployment and browser network inspection |
| Worker API | /api/health returns ok; /api/interest/config returns enabled or disabled | Read-only live smoke |
| Privacy | /#/privacy contact is valid and consent/version is explicit | WDS owner review |
| D1 | Live registration reaches correct private database, no public list route | Controlled form + D1 record check |
| Spam/security | Wrong token/hostname refused, excessive requests controlled appropriately | Worker tests and Cloudflare WAF/log review |
| Source disclosure | Patent binary document absent; WDS license present | CI corpus checks and release review |
| Communications | Awaiting-confirmation leads not emailed without confirmation workflow | WDS operations check |
| Recovery | Prior Worker version restorable; original GitHub Pages docs remain available | Cloudflare Versions rollback drill |

Cloudflare supports static asset serving and D1 migration without R2 object storage. A launch can happen entirely on the Free plan subject to its current limits; optional paid services should be added only for an established need.

## 7. What is not established

The account settings, production DNS, real traffic, database bindings, Turnstile keys, D1 migration execution and subscriber entries have **not** been inspected through a connected Cloudflare account in this conversation. GitHub-only checks cannot prove those outcomes.

[Website roadmap](../planning/07-release-plan.md) · [Cloudflare deployment guide](CLOUDFLARE_DEPLOY.md) · [Interest implementation](INTEREST_ROLLOUT.md) · [WDS license](../../LICENSE.md).
