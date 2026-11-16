/**
 * Anaiero public interest API.
 * Only /api/* invokes the Worker; all other paths are Cloudflare static assets.
 * Never exposes email addresses, raw conversation logs, or biological actuation.
 */
interface Env {
  ASSETS: Fetcher;
  DB?: D1Database;
  TURNSTILE_SECRET?: string;
  PRIVACY_CONTACT?: string;
  ALLOWED_HOSTNAMES?: string;
}
type SiteVerify = { success?: boolean; hostname?: string; action?: string; 'error-codes'?: string[] };
const json = (status: number, payload: object) => new Response(JSON.stringify(payload), {
  status,
  headers: {
    'content-type': 'application/json; charset=utf-8',
    'cache-control': 'no-store',
    'x-content-type-options': 'nosniff',
    'referrer-policy': 'no-referrer',
  },
});
const allow = new Set(['research', 'engineering', 'collaboration', 'studio', 'general']);
const isEmail = (v: string) => v.length <= 254 && /^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/u.test(v);
const expectedHosts = (env: Env) => (env.ALLOWED_HOSTNAMES || 'anaiero.io,www.anaiero.io')
  .split(',').map(h => h.trim().toLowerCase()).filter(Boolean);
async function handleInterest(request: Request, env: Env) {
  const host = new URL(request.url).hostname.toLowerCase();
  const origin = request.headers.get('origin');
  if (origin && origin !== new URL(request.url).origin) return json(403, { error: 'The form must be submitted from this website.' });
  if (!env.DB || !env.TURNSTILE_SECRET || !env.PRIVACY_CONTACT) {
    return json(503, { error: 'Interest registration is not configured yet.' });
  }
  if (!expectedHosts(env).includes(host)) return json(403, { error: 'Domain is not authorized for registration.' });
  const type = request.headers.get('content-type') ?? '';
  const bytes = Number(request.headers.get('content-length') || '0');
  if (!type.includes('application/json') || bytes > 2500) return json(415, { error: 'Invalid submission.' });
  let body: Record<string, unknown>;
  try { body = await request.json() as Record<string,unknown>; } catch { return json(400, { error: 'Malformed submission.' }); }
  if (!body || typeof body !== 'object') return json(400, { error: 'Invalid submission.' });
  // Honeypot: quietly absorb bot-like submissions without inserting data.
  if (body.website) return json(202, { ok: true, message: 'Thank you for your interest.' });
  const email = typeof body.email === 'string' ? body.email.trim().toLowerCase() : '';
  const interest = typeof body.interest === 'string' ? body.interest : '';
  const token = typeof body.turnstileToken === 'string' ? body.turnstileToken : '';
  const source = typeof body.source === 'string' ? body.source.slice(0, 128) : '/interest';
  if (!isEmail(email) || !allow.has(interest) || body.consent !== true ||
      !token || token.length > 2048 || token.length < 8) {
    return json(400, { error: 'Please complete the email, interest, consent and verification fields.' });
  }
  let verification: SiteVerify;
  try {
    const form = new FormData();
    form.set('secret', env.TURNSTILE_SECRET);
    form.set('response', token);
    const response = await fetch('https://challenges.cloudflare.com/turnstile/v0/siteverify', {
      method: 'POST', body: form,
    });
    if (!response.ok) throw Error('Validation unavailable');
    verification = await response.json() as SiteVerify;
  } catch {
    return json(503, { error: 'Verification is temporarily unavailable. Please try again.' });
  }
  if (!verification.success || (verification.hostname && verification.hostname.toLowerCase() !== host) ||
      (verification.action && verification.action !== 'interest')) {
    return json(400, { error: 'Verification was not successful. Please retry.' });
  }
  try {
    const now = new Date().toISOString();
    await env.DB.prepare(
      "INSERT INTO interest_leads (id, email, interest_area, consent_version, consent_at, origin_path, status, created_at) " +
      "VALUES (?, ?, ?, ?, ?, ?, 'awaiting_confirmation', ?) " +
      "ON CONFLICT(email) DO UPDATE SET interest_area=excluded.interest_area, " +
      "consent_version=excluded.consent_version, consent_at=excluded.consent_at, origin_path=excluded.origin_path"
    ).bind(crypto.randomUUID(),email,interest,'2026-11-15-v1',now,source,now).run();
    return json(202, { ok: true, message: 'Your interest has been recorded. Updates will begin after the list is configured.' });
  } catch {
    // Deliberately do not log the email or expose database errors publicly.
    return json(503, { error: 'Could not record your interest. Please try again.' });
  }
}
export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);
    if (url.pathname === '/api/health' && request.method === 'GET') {
      return json(200, { service: 'Anaiero public API', status: 'ok', version: '0.1.0' });
    }
    if (url.pathname === '/api/interest') {
      if (request.method !== 'POST') return json(405, { error: 'Method not allowed.' });
      return handleInterest(request, env);
    }
    if (url.pathname.startsWith('/api/')) return json(404, { error: 'Unknown API route.' });
    return env.ASSETS.fetch(request);
  }
} satisfies ExportedHandler<Env>;
