# Anaiero.io — Interest registration rollout

Revision: November 15, 2026. Owner: We Develop Software LLC. **Status: implemented but disabled until Cloudflare configuration is complete.**

This is an extension of the existing Cloudflare Workers Static Assets portal. The React registration page is at /#/interest and privacy notice at /#/privacy. Only /api/* invokes the Worker; normal site assets remain static.

## Implemented API

- GET /api/health: Worker health.
- GET /api/interest/config: readiness, public Turnstile site key, public privacy contact and current consent policy. No secrets or subscriber data.
- POST /api/interest: explicit consent, category and origin validation, bounded JSON, spam honeypot, Turnstile server-side Siteverify, D1 insertion.
- Unknown /api/* returns JSON 404 rather than SPA HTML.
- Without database and verification variables, the endpoint returns 503 and the React form says registration is not open.

Every new entry has status **awaiting_confirmation**. It is not a verified email subscription. This service sends no messages.

## Activation steps in Cloudflare

1. Make sure anaiero.io is active as a Cloudflare Custom Domain of Worker anaiero-io.
2. Create a Cloudflare D1 database named anaiero-interest. No paid R2 bucket is required.
3. Add a DB binding with the real D1 UUID to website/app/wrangler.jsonc, using the example in [wrangler.d1.example.jsonc](wrangler.d1.example.jsonc). Commit the binding so repository deployments preserve it.
4. Run the migration from website/app: npx wrangler d1 migrations apply anaiero-interest --remote
5. Create a Turnstile widget for anaiero.io (and www.anaiero.io if used).
6. Set Worker variables/secrets:
   - TURNSTILE_SITE_KEY: public widget site key.
   - TURNSTILE_SECRET: private Worker Secret — never commit or share it.
   - PRIVACY_CONTACT: an active privacy/delete-request mailbox for We Develop Software LLC.
   - ALLOWED_HOSTNAMES: optional comma-separated hostname allowlist; default is anaiero.io,www.anaiero.io.
7. Redeploy from the connected GitHub repository and verify https://anaiero.io/api/interest/config returns enabled:true only after setup.
8. Submit one test from /#/interest; verify a private D1 row with status awaiting_confirmation. Confirm invalid and expired tokens fail.

If the actual live Worker is running on a different hostname, update its allowlist and Turnstile hostnames only after verifying domain ownership.

## Local authorized CLI example

~~~bash
cd website/app
npx wrangler login
npx wrangler d1 create anaiero-interest
# Add the returned UUID as DB binding in wrangler.jsonc.
npx wrangler d1 migrations apply anaiero-interest --remote
ANAIERO_SITE_BASE=/ npm run build
npx wrangler deploy
~~~

The default production configuration intentionally omits the DB binding. This lets the site deploy without paid storage or a database. Keep the form disabled until all requirements are met.

## Privacy, spam and operations

- Submissions are same-origin, subject to consent, hostname/action checks, and server-side Turnstile validation.
- No endpoint lists, exports or deletes subscriber records publicly. Email values are private in D1; do not include them in logs or GitHub.
- Confirm that the privacy mailbox works and decide a retention schedule before sending marketing communications.
- The endpoint does not send emails and does not verify inbox ownership. Before announcements, add a proper email provider, double-opt-in confirmation links, unsubscribe/suppression functionality and provider DNS authentication.
- Later add Cloudflare WAF rate limiting where abuse or cost requires it.
- Review and honor deletion requests using an authorized operator, not unauthenticated public APIs.

Official references:
- https://developers.cloudflare.com/workers/static-assets/
- https://developers.cloudflare.com/d1/
- https://developers.cloudflare.com/turnstile/get-started/server-side-validation/

Copyright © 2026 We Develop Software LLC. WDS Public Research Reference License.
