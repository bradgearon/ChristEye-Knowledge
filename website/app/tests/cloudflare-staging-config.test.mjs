import assert from 'node:assert/strict';
import {test} from 'node:test';
import {readFileSync} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const prod=JSON.parse(readFileSync(path.join(root,'wrangler.jsonc'),'utf8'));
const staging=JSON.parse(readFileSync(path.join(root,'wrangler.staging.jsonc'),'utf8'));

test('staging and production Workers have separate deployment names',()=>{
  assert.equal(prod.name,'anaiero-io');
  assert.equal(staging.name,'anaiero');
  assert.notEqual(prod.name,staging.name);
  assert.equal(staging.workers_dev,true);
  assert.equal(staging.vars.ALLOWED_HOSTNAMES,'anaiero.bradgearon.workers.dev');
});

test('both Workers serve identical static assets with API-first routing',()=>{
  for(const worker of [prod,staging]){
    assert.equal(worker.main,'./worker/index.js');
    assert.equal(worker.assets.directory,'./dist');
    assert.equal(worker.assets.binding,'ASSETS');
    assert.equal(worker.assets.not_found_handling,'single-page-application');
    assert.deepEqual(worker.assets.run_worker_first,['/api/*']);
  }
});

test('staging does not contain production D1 IDs or secrets',()=>{
  if(Object.hasOwn(staging,'d1_databases')){
    assert.equal(staging.d1_databases.length,1);
    assert.equal(staging.d1_databases[0].binding,'DB');
    assert.equal(staging.d1_databases[0].database_name,'anaiero-interest-dev');
    assert.ok(!((prod.d1_databases||[]).map(d=>d.database_id).includes(staging.d1_databases[0].database_id)));
  }
  for(const field of ['TURNSTILE_SECRET','TURNSTILE_SITE_KEY','PRIVACY_CONTACT']){
    assert.ok(!Object.hasOwn(staging.vars,field),'Cloudflare secrets and company contact are set in account');
  }
});

test('production config does not allow a dev hostname by default',()=>{
  assert.ok(!String(prod.vars?.ALLOWED_HOSTNAMES||'anaiero.io,www.anaiero.io').includes('workers.dev'));
});
