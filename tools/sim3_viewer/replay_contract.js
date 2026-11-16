// SIM3.1 pure validation module for browser and Node. No network or instrument I/O.
// JSON schema counterparts live under schemas/software; validation here is
// deliberately fail-closed for files supplied to the interactive renderer.
(function(root, factory) {
  'use strict';
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.CEVLabReplay = api;
})(typeof globalThis === 'object' ? globalThis : this, function() {
  'use strict';
  const hex = /^[0-9a-f]{64}$/;
  const name = /^[A-Za-z0-9._-]{1,80}$/;
  const integer = (x,lo,hi) => Number.isSafeInteger(x) && x >= lo && x <= hi;
  const bounded = x => integer(x, -1000000000, 1000000000);
  function guard(ok, reason) { if (!ok) throw new Error(reason); }
  function validCell(cell, knownColonies) {
    guard(cell && typeof cell === 'object' && knownColonies.has(cell.colony), 'Unknown cell colony');
    guard(integer(cell.id,1,2147483647) && integer(cell.parent,0,2147483647), 'Invalid cell ID');
    guard(name.test(cell.kind), 'Invalid cell type');
    for (const key of ['x','y','z','energy','maturity','learning','birth','death'])
      guard(bounded(cell[key]), 'Invalid cell '+key);
    guard(typeof cell.alive === 'boolean', 'Invalid cell viability');
    guard(cell.birth >= 0 && cell.death >= 0, 'Invalid lineage timestamp');
    if (!cell.alive) guard(cell.death >= cell.birth, 'Invalid death tick');
  }
  function validateFrame(frame) {
    guard(frame && frame.origin === 'SIMULATED_UNCALIBRATED' && frame.format === 'CEVLABFRAME/1', 'Unsupported frame or scientific origin');
    guard(name.test(frame.run) && integer(frame.tick,0,Number.MAX_SAFE_INTEGER), 'Invalid run/tick');
    guard(integer(frame.law_version,1,65535), 'Invalid law version');
    guard(hex.test(frame.config_sha256) && hex.test(frame.snapshot_sha256), 'Invalid snapshot/config hash');
    guard(Array.isArray(frame.colonies) && frame.colonies.length >= 1 && frame.colonies.length <= 64, 'Invalid colonies');
    guard(Array.isArray(frame.cells) && frame.cells.length <= 10000, 'Invalid cells');
    guard(Array.isArray(frame.links) && frame.links.length <= 1024, 'Invalid links');
    const colonies = new Set();
    for (const colony of frame.colonies) {
      guard(colony && name.test(colony.id) && !colonies.has(colony.id), 'Duplicate or invalid colony ID');
      colonies.add(colony.id);
      for (const key of ['food','data','signal','stress']) guard(bounded(colony[key]), 'Invalid colony '+key);
    }
    const cells = new Set();
    for (const cell of frame.cells) {
      validCell(cell, colonies);
      const key = cell.colony + '/' + cell.id;
      guard(!cells.has(key), 'Duplicate cell identity');
      cells.add(key);
    }
    for (const cell of frame.cells) {
      if (cell.parent !== 0) guard(cells.has(cell.colony + '/' + cell.parent), 'Missing parent lineage');
    }
    for (const link of frame.links) {
      guard(link && colonies.has(link.from) && colonies.has(link.to), 'Unknown link endpoint');
      guard(name.test(link.channel) && bounded(link.units), 'Invalid link');
    }
    return frame;
  }
  function validateTimeline(frames) {
    guard(Array.isArray(frames) && frames.length > 0 && frames.length <= 10000, 'Empty or oversized timeline');
    const first = validateFrame(frames[0]);
    let previous = first.tick - 1;
    for (const frame of frames) {
      validateFrame(frame);
      guard(frame.run === first.run && frame.config_sha256 === first.config_sha256 &&
        frame.law_version === first.law_version, 'Mixed runs, configurations or simulation laws');
      guard(frame.tick === previous + 1, 'Missing, duplicate or out-of-order tick');
      previous = frame.tick;
    }
    return frames;
  }
  function validateOverlays(overlays) {
    guard(Array.isArray(overlays) && overlays.length <= 64, 'Invalid overlay count');
    const names = new Set();
    for (const o of overlays) {
      guard(o && o.origin === 'SIMULATED_UNCALIBRATED' && o.format === 'CEVLMOVERLAY/1', 'Unsupported source overlay');
      guard(name.test(o.colony) && !names.has(o.colony), 'Invalid overlay colony');
      guard(hex.test(o.source_sha256) && integer(o.tick,0,Number.MAX_SAFE_INTEGER), 'Invalid overlay provenance');
      guard(Array.isArray(o.cells) && o.cells.length <= 10000 && Array.isArray(o.molecules) && o.molecules.length <= 10000, 'Invalid overlay data');
      guard(Array.isArray(o.connections) && o.connections.length <= 10000 && Array.isArray(o.active_processes), 'Invalid overlay connections');
      guard(o.fields && o.fields.side === 8 && Array.isArray(o.fields.layers) && o.fields.layers.length === 6, 'Invalid Soil grid');
      for (const field of o.fields.layers) {
        guard(name.test(field.name) && Array.isArray(field.values) && field.values.length === 64 && field.values.every(bounded), 'Invalid Soil field');
      }
      names.add(o.colony);
    }
    return overlays;
  }
  function parseNDJSON(text, maxRecords = 10000, maxBytes = 8 * 1024 * 1024) {
    guard(typeof text === 'string' && text.length <= maxBytes, 'Oversized file');
    const lines = text.split(/\r?\n/).filter(line => line.trim().length);
    guard(lines.length <= maxRecords, 'Too many records');
    return lines.map((line, i) => {
      try { return JSON.parse(line); }
      catch (_) { throw new Error('Invalid JSON at line ' + (i+1)); }
    });
  }
  return Object.freeze({ validateFrame, validateTimeline, validateOverlays, parseNDJSON });
});
