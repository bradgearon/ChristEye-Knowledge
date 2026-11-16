/**
 * ChristEye Anaiero — static site and opt-in interest API.
 * Static assets bypass this Worker script by Cloudflare routing; /api/* runs here.
 *
 * Registration is fail-closed until a private D1 binding, a site key, a Turnstile
 * secret and a real privacy contact are all configured for anaiero.io.
 * This Worker never touches biological systems or private research logs.
 */
const CONSENT_VERSION = '2026-11-15-v1';
const CATEGORIES = new Set(['research','engineering','collaboration','studio','general']);
const MAX_BODY_BYTES = 4096;
const DEFAULT_HOSTS = 'anaiero.io,www.anaiero.io';

function reply(status, data) {
  return new Response(JSON.stringify(data), {
    status,
    headers: {
      'Content-Type': 'application/json; charset=utf-8',
      'Cache-Control': 'no-store, max-age=0',
      'X-Content-Type-Options': 'nosniff',
      'Referrer-Policy': 'no-referrer',
      'Content-Security-Policy': "default-src 'none'; frame-ancestors 'none'",
    },
  });
}
function allowedHosts(env) {
  return String(env.ALLOWED_HOSTNAMES || DEFAULT_HOSTS)
    .split(',').map(s => s.trim().toLowerCase()).filter(Boolean);
}
function enabled(env, hostname) {
  return Boolean(
    env.DB && typeof env.DB.prepare === 'function' &&
    env.TURNSTILE_SECRET && env.TURNSTILE_SITE_KEY &&
    typeof env.PRIVACY_CONTACT === 'string' && /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(env.PRIVACY_CONTACT) &&
    allowedHosts(env).includes(hostname.toLowerCase())
  );
}
function isEmail(email) {
  return email.length <= 254 && /^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/.test(email) &&
    !/[\r\n\u0000-\u001F]/.test(email);
}
async function boundedJson(request) {
  const length=Number(request.headers.get('content-length') || 0);
  if (length > MAX_BODY_BYTES) return { error: 'Request exceeds size limit.' };
  if (!/^application\/json(?:\s*;|$)/i.test(request.headers.get('content-type') || '')) {
    return { error: 'JSON content type required.' };
  }
  if (!request.body) return { error: 'Empty request body.' };
  const reader=request.body.getReader();
  const parts=[];
  let total=0;
  while(true) {
    const {done,value}=await reader.read();
    if(done)break;
    total += value.byteLength;
    if(total>MAX_BODY_BYTES) {
      await reader.cancel();
      return { error:'Request exceeds size limit.' };
    }
    parts.push(value);
  }
  const buf=new Uint8Array(total);
  let offset=0;
  for(const part of parts){buf.set(part,offset);offset+=part.byteLength;}
  let value;
  try { value=JSON.parse(new TextDecoder().decode(buf)); }
  catch { return { error:'Invalid JSON.' }; }
  if (!value || typeof value!=='object' || Array.isArray(value)) return { error:'Invalid form data.' };
  return { value };
}
async function verifyTurnstile(token, env, host) {
  const form=new FormData();
  form.set('secret',String(env.TURNSTILE_SECRET));
  form.set('response',token);
  // No remote IP: do not store IP addresses in Anaiero's interest list.
  const res=await fetch('https://challenges.cloudflare.com/turnstile/v0/siteverify',{
    method:'POST',body:form,
  });
  if(!res.ok)return false;
  const data=await res.json();
  return data?.success === true &&
    data.action === 'interest_signup' &&
    typeof data.hostname === 'string' &&
    data.hostname.toLowerCase() === host;
}
async function register(request, env, hostname) {
  if (!enabled(env,hostname)) {
    return reply(503,{error:'Interest registration has not been enabled yet.'});
  }
  const origin=request.headers.get('origin');
  if(origin !== new URL(request.url).origin){
    return reply(403,{error:'Submit this form from anaiero.io.'});
  }
  const payload=await boundedJson(request);
  if(payload.error)return reply(400,{error:payload.error});
  const body=payload.value;

  // Honeypot: give no attacker-visible signal about filtering.
  if(typeof body.website === 'string' && body.website.trim()){
    return reply(202,{ok:true,message:'Request received.'});
  }
  const email=typeof body.email==='string' ? body.email.trim().toLowerCase() : '';
  const interest=typeof body.interest==='string' ? body.interest : '';
  const source=typeof body.source==='string' ? body.source : '/interest';
  const token=typeof body.turnstileToken==='string' ? body.turnstileToken : '';
  if(!isEmail(email) || !CATEGORIES.has(interest) ||
     body.consent !== true || body.consentVersion !== CONSENT_VERSION ||
     !/^\/[a-zA-Z0-9_/#-]{0,120}$/.test(source) ||
     token.length<8 || token.length>2048){
    return reply(400,{error:'Please provide a valid email, topic, consent and verification.'});
  }
  let valid=false;
  try { valid=await verifyTurnstile(token,env,hostname); }
  catch { return reply(503,{error:'Verification is unavailable. Please retry.'}); }
  if(!valid)return reply(400,{error:'Verification expired or was not accepted. Please retry.'});
  try {
    const now=new Date().toISOString();
    await env.DB.prepare(
      'INSERT INTO interest_leads (id,email,interest_area,consent_version,consent_at,source_path,status,created_at) '+
      "VALUES (?,?,?,?,?,?,'awaiting_confirmation',?) "+
      'ON CONFLICT(email) DO UPDATE SET interest_area=excluded.interest_area, '+
      'consent_version=excluded.consent_version, consent_at=excluded.consent_at, '+
      'source_path=excluded.source_path'
    ).bind(crypto.randomUUID(),email,interest,CONSENT_VERSION,now,source,now).run();
    return reply(202,{
      ok:true,
      message:'Your interest has been recorded. Email updates begin only after confirmation is available.',
    });
  }catch {
    // Do not expose a database error or include submitted personal information in logs.
    return reply(503,{error:'The registration could not be saved. Please retry.'});
  }
}
export default {
  async fetch(request, env) {
    const url=new URL(request.url);
    if(url.pathname==='/api/health'){
      return request.method==='GET'
        ? reply(200,{service:'Anaiero',status:'ok'})
        : reply(405,{error:'Method not allowed.'});
    }
    if(url.pathname==='/api/interest/config'){
      if(request.method!=='GET')return reply(405,{error:'Method not allowed.'});
      const active=enabled(env,url.hostname);
      return reply(200,{
        enabled:active,
        siteKey:active ? String(env.TURNSTILE_SITE_KEY) : null,
        privacyContact:active ? String(env.PRIVACY_CONTACT) : null,
        consentVersion:CONSENT_VERSION,
      });
    }
    if(url.pathname==='/api/interest'){
      if(request.method!=='POST')return reply(405,{error:'Method not allowed.'});
      return register(request,env,url.hostname.toLowerCase());
    }
    if(url.pathname.startsWith('/api/'))return reply(404,{error:'Unknown API endpoint.'});
    return env.ASSETS.fetch(request);
  },
};
