'use strict';
// Read-only native-to-viewer integration check. No remote data access.
const fs=require('node:fs');
const assert=require('node:assert/strict');
const replay=require('./replay_contract.js');
function main() {
  const [framesFile,overlayFile]=process.argv.slice(2);
  if (!framesFile || !overlayFile) throw Error('usage: node validate_replay.cjs replay.ndjson overlay.ndjson');
  const frames=replay.validateTimeline(replay.parseNDJSON(fs.readFileSync(framesFile,'utf8')));
  const overlays=replay.validateOverlays(replay.parseNDJSON(fs.readFileSync(overlayFile,'utf8'),64));
  assert.ok(frames.length>=2,'Expected at least two committed snapshots');
  assert.ok(overlays.length>0,'Expected source-model overlay');
  const colonies=new Set(frames[0].colonies.map(c=>c.id));
  for(const o of overlays) assert.ok(colonies.has(o.colony),'Overlay for unknown colony');
  console.log('SIM3.1 native-to-viewer replay verified: '+frames.length+' frames; '+overlays.length+' overlays');
}
try{main();}catch(e){console.error(e.message);process.exit(1);}
