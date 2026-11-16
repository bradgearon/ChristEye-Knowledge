# Anaiero interest registration — security and rollout

The Worker endpoint at POST /api/interest collects only email, category of interest, source path, and recorded explicit consent. Email addresses are stored privately in Cloudflare D1, **never published via GitHub or a read endpoint**.

## Disabled by default

The endpoint responds 503 until all of these exist in the Cloudflare Worker environment:
- D1 binding **DB** with the database created and migrations run.
- Secret **TURNSTILE_SECRET** (server-only).
- Runtime variable **PRIVACY_CONTACT** pointing to a working contact mailbox for deletion/privacy requests.
- Runtime variable **ALLOWED_HOSTNAMES** with permitted hostnames, initially anaiero.io and www.anaiero.io.
- React build variable **VITE_TURNSTILE_SITE_KEY**, provisioned by the Cloudflare build configuration.

Do not commit real secrets or a real email mailbox without company approval. The site shows a transparent "signups opening" status until configured.

## Privacy and abuse boundaries

- Require explicit opt-in and a link to the privacy statement.
- Reject unexpected cross-origin, malformed, oversized and invalid-form requests.
- Validate Turnstile token server-side; reject bad hostname and action when supplied.
- Store no IP addresses or raw browser fingerprint data.
- Restrict database access through Cloudflare roles; back up/export only through an authorized operator.
- Enable Cloudflare WAF rate limiting as needed. A Turnstile check is an abuse control, not a guarantee.
- Status awaiting_confirmation means a database entry **is not** proof of a verified email. Configure a legitimate email provider and confirmation/unsubscribe workflow before marketing automation.
- Handle deletion requests through the configured privacy mailbox; avoid accepting public GET/DELETE actions that expose subscriber enumeration.

## Database

Apply migrations with Wrangler after D1 binding has a real UUID. Never manually expose the D1 database as a public API.

See [Cloudflare Turnstile validation](https://developers.cloudflare.com/turnstile/get-started/server-side-validation/) and [Cloudflare D1](https://developers.cloudflare.com/d1/).
