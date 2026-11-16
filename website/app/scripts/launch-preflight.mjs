#!/usr/bin/env node
/**
 * S07 production preflight:
 *   --offline: validate local React output, corpus, Worker routing, D1 schema.
 *   --live: GET-only checks of anaiero.io after Cloudflare deployment.
 *   --live --target=dev: check existing anaiero.bradgearon.workers.dev staging host.
 *   --live --require-interest: fail unless interest API is configured.
 * No credentials, personal data or POST requests.
 */
import assert from 'node:assert/strict';
import {readFileSync,existsSync,readdirSync} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const flags=new Set(process.argv.slice(2));
const offline=flags.has('--offline'),live=flags.has('--live'),required=flags.has('--require-interest');
const targetArg=[...flags].find(x=>x.startsWith('--target='));
const target=targetArg?.slice('--target='.length)||'production';
const normalizedTarget=target==='development'?'dev':target;
const validTargets=new Set(['dev','production']);
if(flags.has('--help')){console.log('Usage: node scripts/launch-preflight.mjs --offline | --live [--target=dev|production] [--require-interest]');process.exit(0);}
if(offline===live || (required&&!live) || !validTargets.has(normalizedTarget) || (targetArg&&offline) || [...flags].some(x=>!['--offline','--live','--require-interest'].includes(x)&&!x.startsWith('--target='))){
  console.error('Choose --offline, or --live [--target=dev|production] [--require-interest].');process.exit(2);
}
const results=[];
function pass(key,result){results.push({key,ok:true,result});}
function fail(key,error){results.push({key,ok:false,result:error instanceof Error?error.message:String(error)});}
function check(key,fn){try{pass(key,fn());}catch(e){fail(key,e);}}
const file=p=>readFileSync(path.join(root,p),'utf8');
const json=p=>JSON.parse(file(p));
if(offline){
  check('Worker assets and API routing',()=>{
    const w=json('wrangler.jsonc');
    assert.equal(w.name,'anaiero-io');
    assert.equal(w.main,'./worker/index.js');
    assert.equal(w.assets.directory,'./dist');
    assert.equal(w.assets.binding,'ASSETS');
    assert.equal(w.assets.not_found_handling,'single-page-application');
    assert.deepEqual(w.assets.run_worker_first,['/api/*']);
    assert.ok(existsSync(path.join(root,w.main)));
    return 'Static assets from dist; API Worker only on /api/*';
  });
  check('Staging Worker isolation',()=>{
    const production=json('wrangler.jsonc'),stage=json('wrangler.staging.jsonc');
    assert.equal(production.name,'anaiero-io');
    assert.equal(stage.name,'anaiero');
    assert.equal(stage.vars.ALLOWED_HOSTNAMES,'anaiero.bradgearon.workers.dev');
    assert.equal(stage.assets.directory,production.assets.directory);
    assert.deepEqual(stage.assets.run_worker_first,['/api/*']);
    if(stage.d1_databases){
      assert.equal(stage.d1_databases.length,1,'Exactly one staging database expected');
      assert.equal(stage.d1_databases[0].binding,'DB');
      assert.equal(stage.d1_databases[0].database_name,'anaiero-interest-dev');
      const prodIds=(production.d1_databases||[]).map(x=>x.database_id).filter(Boolean);
      assert.ok(!prodIds.includes(stage.d1_databases[0].database_id),'Staging must not share production D1');
    }
    return 'Staging and production Worker names/allowed hostnames isolated';
  });
  check('React production HTML at domain root',()=>{
    const html=file('dist/index.html');
    assert.match(html,/<html\b/i);
    assert.match(html,/<script\b/i);
    assert.ok(html.includes('/assets/'),'Missing root-domain script and style URLs');
    assert.ok(!html.includes('/ChristEye-Knowledge/'),'GitHub Pages base leaked');
    const assets=readdirSync(path.join(root,'dist/assets'));
    assert.ok(assets.some(x=>x.endsWith('.js')),'Missing JavaScript assets');
    assert.ok(assets.some(x=>x.endsWith('.css')),'Missing CSS assets');
    return String(assets.length)+' built assets';
  });
  check('Public documents and non-patent release',()=>{
    const manifest=json('dist/content/manifest.json');
    const release=json('dist/content/release.json');
    assert.ok(manifest.documents?.length>=300,'Incomplete source library');
    assert.equal(release.patent_included,false);
    assert.equal(release.physical_actuation,false);
    assert.equal(new Set(manifest.documents.map(d=>d.id)).size,manifest.documents.length,'Duplicate document IDs');
    for(const d of manifest.documents){
      assert.ok(existsSync(path.join(root,'dist/content/documents',d.id+'.json')),'Missing document '+d.id);
      assert.ok(!(/patent/i.test(d.source_path||'')&&/\.(pdf|docx)$/i.test(d.source_path||'')),'Patent file imported');
    }
    return manifest.documents.length+' indexed source documents; patent document excluded';
  });
  check('Opt-in D1 schema and fail-closed consent controls',()=>{
    const sql=file('migrations/0001_interest.sql'),worker=file('worker/index.js');
    for(const x of ['CREATE TABLE IF NOT EXISTS interest_leads','consent_version','interest_area','awaiting_confirmation','source_path'])assert.ok(sql.includes(x),'D1 migration missing '+x);
    for(const x of ['TURNSTILE_SECRET','TURNSTILE_SITE_KEY','PRIVACY_CONTACT','interest_signup','not been enabled yet'])assert.ok(worker.includes(x),'Worker missing '+x);
    return 'D1 migration and fail-closed API requirements present';
  });
}else{
  const base=normalizedTarget==='dev'?'https://anaiero.bradgearon.workers.dev':'https://anaiero.io';
  async function liveCheck(key,fn){try{pass(key,await fn());}catch(e){fail(key,e);}}
  const get=route=>fetch(base+route,{method:'GET',redirect:'follow',cache:'no-store',headers:{'User-Agent':'AnaieroReleasePreflight/1.0 (+https://github.com/bradgearon/ChristEye-Knowledge)'},signal:AbortSignal.timeout(15000)});
  console.log('Read-only Anaiero smoke target: '+base);
  await liveCheck('HTTPS homepage',async()=>{
    const r=await get('/');
    assert.ok(r.ok,'HTTP '+r.status);
    assert.equal(new URL(r.url).protocol,'https:');
    assert.match(r.headers.get('content-type')||'',/text\/html/);
    assert.match(await r.text(),/anaiero/i);
    return 'HTTP '+r.status+', '+new URL(r.url).hostname;
  });
  await liveCheck('Worker API health',async()=>{
    const r=await get('/api/health');
    assert.equal(r.status,200,'HTTP '+r.status);
    const data=await r.json();assert.equal(data.status,'ok');
    return 'API responds: '+String(data.service||'Anaiero');
  });
  await liveCheck('Interest API readiness (no submissions)',async()=>{
    const r=await get('/api/interest/config');
    assert.equal(r.status,200,'HTTP '+r.status);
    const cfg=await r.json();
    assert.equal(typeof cfg.enabled,'boolean');
    assert.ok(!('TURNSTILE_SECRET' in cfg),'Secret exposed');
    if(required)assert.equal(cfg.enabled,true,'Registration is disabled until D1, Turnstile and privacy contact are connected');
    return cfg.enabled?'Enabled: manual D1 test still required':'Safely disabled; no emails accepted yet';
  });
}
for(const r of results)console.log((r.ok?'PASS':'FAIL')+' '+r.key+': '+r.result);
const errors=results.filter(r=>!r.ok);
if(errors.length){console.error(errors.length+' S07 preflight check(s) failed. Deployment/activation is NOT verified.');process.exitCode=1;}
else console.log(offline?'Artifact checks passed; NOT evidence of live Cloudflare deployment.':'Public GET checks passed; actual opt-in/D1 write still requires owner verification.');
