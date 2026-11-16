import assert from 'node:assert/strict';
import { test } from 'node:test';
import worker from '../worker/index.js';

test('reserved API path is disabled rather than serving SPA HTML',async ()=>{
  let assetCalls=0;
  const env={ASSETS:{fetch:()=>{assetCalls++;return new Response('index');}}};
  const res=await worker.fetch(new Request('https://anaiero.io/api/interest'),env);
  assert.equal(res.status,404);
  assert.match(res.headers.get('content-type')||'',/application\/json/);
  assert.equal(assetCalls,0);
  const body=await res.json();
  assert.equal(body.error,'This API is not enabled.');
});
test('non-API requests pass through to static asset binding',async ()=>{
  const env={ASSETS:{fetch:async r=>new Response('asset:'+new URL(r.url).pathname)}};
  const res=await worker.fetch(new Request('https://anaiero.io/docs'),env);
  assert.equal(await res.text(),'asset:/docs');
});
