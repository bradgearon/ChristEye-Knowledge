import assert from 'node:assert/strict';
import {test} from 'node:test';
import worker from '../worker/index.js';

const origin='https://anaiero.io';
const makeEnv=()=>{
  const statements=[];
  const DB={prepare:(sql)=>({
    bind:(...values)=>({
      run:async()=>{statements.push({sql,values});return {success:true};}
    })
  })};
  return {env:{
    ASSETS:{fetch:async req=>new Response('asset:'+new URL(req.url).pathname)},
    DB,TURNSTILE_SECRET:'private-secret',TURNSTILE_SITE_KEY:'public-site-key',
    PRIVACY_CONTACT:'privacy@example.test',ALLOWED_HOSTNAMES:'anaiero.io,www.anaiero.io'
  },statements};
};
const form={
  email:'Researcher@Example.com',interest:'research',consent:true,
  consentVersion:'2026-11-15-v1',turnstileToken:'test-turnstile-token',
  source:'/interest',website:''
};
const req=(method,path,body,headers={})=>new Request(origin+path,{
  method,
  headers:{...(body===undefined?{}:{'content-type':'application/json'}),origin,...headers},
  ...(body===undefined?{}:{body:JSON.stringify(body)}),
});
const siteverify=(overrides={})=>({
  success:true,hostname:'anaiero.io',action:'interest_signup',...overrides
});
async function mockVerify(response,callback){
  const previous=globalThis.fetch;
  globalThis.fetch=async (url,options)=>{
    assert.equal(url,'https://challenges.cloudflare.com/turnstile/v0/siteverify');
    assert.equal(options.method,'POST');
    assert.equal(options.body.get('secret'),'private-secret');
    assert.equal(options.body.get('response'),form.turnstileToken);
    return new Response(JSON.stringify(response),{headers:{'content-type':'application/json'}});
  };
  try {return await callback();} finally {globalThis.fetch=previous;}
}

test('unconfigured site exposes only disabled public state',async()=>{
  const env={ASSETS:{fetch:async()=>new Response('static page')}};
  const config=await worker.fetch(req('GET','/api/interest/config'),env);
  assert.equal(config.status,200);
  const result=await config.json();
  assert.equal(result.enabled,false);
  assert.equal(result.siteKey,null);
  assert.equal(result.privacyContact,null);
  assert.ok(!JSON.stringify(result).includes('TURNSTILE_SECRET'));
  const post=await worker.fetch(req('POST','/api/interest',form),env);
  assert.equal(post.status,503);
  assert.match((await post.json()).error,/not been enabled/);
});

test('worker reserves /api routes, allows assets and serves health',async()=>{
  const env={ASSETS:{fetch:async req=>new Response('static:'+new URL(req.url).pathname)}};
  const health=await worker.fetch(req('GET','/api/health'),env);
  assert.equal((await health.json()).status,'ok');
  const bad=await worker.fetch(req('GET','/api/arbitrary'),env);
  assert.equal(bad.status,404);
  const method=await worker.fetch(req('GET','/api/interest'),env);
  assert.equal(method.status,405);
  const asset=await worker.fetch(req('GET','/studio'),env);
  assert.equal(await asset.text(),'static:/studio');
});

test('configured domain publishes sitekey, privacy contact and consent version only',async()=>{
  const {env}=makeEnv();
  const config=await (await worker.fetch(req('GET','/api/interest/config'),env)).json();
  assert.equal(config.enabled,true);
  assert.equal(config.siteKey,'public-site-key');
  assert.equal(config.privacyContact,'privacy@example.test');
  assert.equal(config.consentVersion,'2026-11-15-v1');
  assert.ok(!JSON.stringify(config).includes('private-secret'));
  const preview=await worker.fetch(new Request('https://preview.workers.dev/api/interest/config'),env);
  assert.equal((await preview.json()).enabled,false);
});

test('origin and consent gate blocks requests before Turnstile',async()=>{
  const {env,statements}=makeEnv();
  const cross=await worker.fetch(req('POST','/api/interest',form,{origin:'https://evil.example'}),env);
  assert.equal(cross.status,403);
  const noConsent=await worker.fetch(req('POST','/api/interest',{...form,consent:false}),env);
  assert.equal(noConsent.status,400);
  const badVersion=await worker.fetch(req('POST','/api/interest',{...form,consentVersion:'2025'}),env);
  assert.equal(badVersion.status,400);
  const badEmail=await worker.fetch(req('POST','/api/interest',{...form,email:'invalid'}),env);
  assert.equal(badEmail.status,400);
  assert.equal(statements.length,0);
});

test('content type and payload size must be bounded',async()=>{
  const {env}=makeEnv();
  const wrong=await worker.fetch(req('POST','/api/interest',form,{'content-type':'text/plain'}),env);
  assert.equal(wrong.status,400);
  const large=await worker.fetch(req('POST','/api/interest',{...form,email:'x'.repeat(6000)}),env);
  assert.equal(large.status,400);
});

test('valid Turnstile proof records only explicit consent and required fields in private D1',async()=>{
  const {env,statements}=makeEnv();
  const res=await mockVerify(siteverify(),()=>worker.fetch(req('POST','/api/interest',form),env));
  assert.equal(res.status,202);
  const result=await res.json();
  assert.equal(result.ok,true);
  assert.equal(statements.length,1);
  assert.match(statements[0].sql,/ON CONFLICT\(email\)/);
  assert.equal(statements[0].values[1],'researcher@example.com');
  assert.equal(statements[0].values[2],'research');
  assert.equal(statements[0].values[3],'2026-11-15-v1');
  assert.equal(statements[0].values[5],'/interest');
  assert.equal(JSON.stringify(result).includes('researcher@example.com'),false);
});

test('Turnstile refuses wrong hostname or action without persisting',async()=>{
  const {env,statements}=makeEnv();
  for(const override of [{hostname:'evil.example'},{action:'login'},{success:false}]){
    const res=await mockVerify(siteverify(override),()=>worker.fetch(req('POST','/api/interest',form),env));
    assert.equal(res.status,400);
  }
  assert.equal(statements.length,0);
});

test('verification failures return errors without storing user email',async()=>{
  const {env,statements}=makeEnv();
  const old=globalThis.fetch;
  globalThis.fetch=async()=>{throw Error('API unreachable');};
  try{
    const res=await worker.fetch(req('POST','/api/interest',form),env);
    assert.equal(res.status,503);
    assert.ok(!(await res.text()).includes(form.email));
    assert.equal(statements.length,0);
  }finally{globalThis.fetch=old;}
});

test('honeypot does not write to D1',async()=>{
  const {env,statements}=makeEnv();
  const res=await worker.fetch(req('POST','/api/interest',{...form,website:'spam.example'}),env);
  assert.equal(res.status,202);
  assert.equal(statements.length,0);
});


const devOrigin='https://anaiero.bradgearon.workers.dev';
const devRequest=(method,path,body,headers={})=>new Request(devOrigin+path,{
  method,
  headers:{...(body===undefined?{}:{'content-type':'application/json'}),origin:devOrigin,...headers},
  ...(body===undefined?{}:{body:JSON.stringify(body)})
});

test('live development hostname remains disabled without its explicit allowlist',async()=>{
  const {env,statements}=makeEnv();
  const cfg=await worker.fetch(devRequest('GET','/api/interest/config'),env);
  assert.equal(cfg.status,200);
  assert.equal((await cfg.json()).enabled,false);
  const submit=await worker.fetch(devRequest('POST','/api/interest',form),env);
  assert.equal(submit.status,503);
  assert.equal(statements.length,0);
});

test('explicit workers.dev allowlist enables only the staging hostname',async()=>{
  const {env}=makeEnv();
  env.ALLOWED_HOSTNAMES='anaiero.bradgearon.workers.dev';
  const staging=await (await worker.fetch(devRequest('GET','/api/interest/config'),env)).json();
  assert.equal(staging.enabled,true);
  assert.equal(staging.siteKey,'public-site-key');
  const production=await (await worker.fetch(req('GET','/api/interest/config'),env)).json();
  assert.equal(production.enabled,false);
});

test('staging hostname and Turnstile action must match before inserting into staging D1',async()=>{
  const {env,statements}=makeEnv();
  env.ALLOWED_HOSTNAMES='anaiero.bradgearon.workers.dev';
  const valid=await mockVerify(siteverify({hostname:'anaiero.bradgearon.workers.dev'}),()=>worker.fetch(devRequest('POST','/api/interest',form),env));
  assert.equal(valid.status,202);
  assert.equal(statements.length,1);
  const mismatched=await mockVerify(siteverify({hostname:'anaiero.io'}),()=>worker.fetch(devRequest('POST','/api/interest',form),env));
  assert.equal(mismatched.status,400);
  assert.equal(statements.length,1,'Mismatched validation must not be stored');
  const malicious=await worker.fetch(devRequest('POST','/api/interest',form,{origin:'https://untrusted.example'}),env);
  assert.equal(malicious.status,403);
});
