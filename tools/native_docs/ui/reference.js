/* Anaiero Native Reference v2 — static, versioned, dependency-free reader.
   The initial request loads only versions.json and the compact search index.
   Detailed declarations and source text are fetched on demand. */
(() => {
"use strict";
const $ = id => document.getElementById(id);
const safe = value => String(value == null ? "" : value).replace(/[&<>"']/g, c => ({ "&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;" }[c]));
const simple = value => String(value || "").replace(/\s+/g, " ").trim();
const typeKinds = new Set(["class","struct","union","enum","alias","typedef"]);
const callable = new Set(["method","constructor","destructor","operator"]);
const valueKinds = new Set(["field","constant","variable","enumerator","macro"]);
const descriptions = {
  application:"Composition and executable application entry points",
  services:"Domain services, state transitions and orchestration",
  contracts:"Typed interfaces, capabilities and records",
  models:"Portable model state and types",
  infrastructure:"Storage, process, transport and host adapters",
  living:"Simulation models and durable living-state runtime",
  wind:"WindBus data transport and media infrastructure",
  sources:"Other native library declarations"
};
const layerNames = Object.keys(descriptions);
const ui = { version:null, versions:[], symbols:[], files:[], symbolById:new Map(), fileById:new Map(),
             sourceIndex:new Map(), detailBuckets:new Map(), sources:new Map(),
             query:"", layer:"all", kind:"types", internal:false, selected:"", cursor:0, routeToken:0 };
const uiURL = new URL(location.href);
const sourceURL = (path, line) => "https://github.com/" + ui.version.sourceRepo + "/blob/" +
  encodeURIComponent(ui.version.commit) + "/" + String(path).split("/").map(encodeURIComponent).join("/") + (line ? "#L" + line : "");
const routeFor = entry => "#/symbol/" + encodeURIComponent(entry.id);
const fileRoute = file => "#/file/" + encodeURIComponent(file.id);
const count = value => Number(value||0).toLocaleString("en-US");

function toast(message) {
  const el = $("toast");
  el.textContent = message; el.style.display = "block";
  clearTimeout(toast.timer); toast.timer = setTimeout(() => { el.style.display = "none"; }, 2300);
}
function loading(message) {
  $("article").innerHTML = '<div class="crumb">ANAIERO / NATIVE REFERENCE</div><h1>' + safe(message) + '</h1><p class="subtitle">Loading source-verified documentation…</p>';
}
async function fetchJSON(path) {
  const response = await fetch(path, {cache:"default"});
  if (!response.ok) throw new Error(path + " returned HTTP " + response.status);
  return response.json();
}
function versionLink(slug, hash) {
  const url = new URL(location.href);
  url.searchParams.set("v", slug);
  url.hash = hash || "#/overview";
  return url.pathname + url.search + url.hash;
}
function renderVersionControl() {
  $("version").innerHTML = ui.versions.map(v => '<option value="' + safe(v.version) + '"' +
    (v.version === ui.version.version ? " selected" : "") + '>' + safe(v.version) + '</option>').join("");
  $("status").textContent = ui.version.version + " · " + count(ui.version.symbols) + " indexed";
  $("total-count").textContent = count(ui.symbols.filter(s => typeKinds.has(s.kind) && s.access === "public").length);
}
function renderLayers() {
  $("layers").innerHTML = layerNames.map(layer => {
    const number = ui.symbols.filter(s => s.layer === layer && typeKinds.has(s.kind)).length;
    if (!number) return "";
    return '<button type="button" class="navlink' + (ui.layer === layer ? " active" : "") +
      '" data-layer="' + safe(layer) + '"><span>◦</span><span>' + safe(layer[0].toUpperCase() + layer.slice(1)) +
      '</span><small>' + count(number) + '</small></button>';
  }).join("");
  document.querySelectorAll("[data-layer]").forEach(btn => {
    btn.classList.toggle("active", btn.dataset.layer === ui.layer);
  });
}
function kindMatches(symbol) {
  if (ui.kind === "all") return true;
  if (ui.kind === "types") return typeKinds.has(symbol.kind);
  if (ui.kind === "methods") return callable.has(symbol.kind);
  if (ui.kind === "functions") return symbol.kind === "function";
  if (ui.kind === "values") return valueKinds.has(symbol.kind);
  return true;
}
function rank(item, query) {
  if (!query) return 1;
  const n = String(item.name || "").toLowerCase();
  const q = String(item.qualified || item.path || "").toLowerCase();
  const d = String(item.description || "").toLowerCase();
  if (n === query) return 200;
  if (n.startsWith(query)) return 150;
  if (q.endsWith("::" + query)) return 120;
  if (n.includes(query)) return 95;
  if (q.includes(query)) return 70;
  if (d.includes(query)) return 25;
  return 0;
}
function renderResults() {
  const query = ui.query.trim().toLowerCase();
  const forFiles = ui.kind === "files";
  const data = forFiles ? ui.files : ui.symbols;
  const results = [];
  for (const item of data) {
    if (ui.layer !== "all" && item.layer !== ui.layer) continue;
    if (!forFiles && (!kindMatches(item) || (!ui.internal && (item.access === "private" || item.access === "protected")))) continue;
    const score = rank(item, query);
    if (score) results.push({item, score});
  }
  results.sort((a,b) => query ? b.score-a.score || a.item.name.localeCompare(b.item.name) :
    a.item.layer.localeCompare(b.item.layer) || a.item.name.localeCompare(b.item.name));
  $("result-count").textContent = count(results.length) + " results";
  $("list-title").textContent = ui.layer === "all" ? "Browse native API" : ui.layer[0].toUpperCase() + ui.layer.slice(1);
  $("scope").textContent = query ? "SEARCH RESULTS" : "API INDEX";
  document.querySelectorAll(".facet").forEach(x => x.classList.toggle("active", x.dataset.kind === ui.kind));
  const el = $("results");
  const visible = results.slice(0, 220);
  let last = "", html = "";
  for (const item of visible) {
    const d = item.item;
    if (!query && d.layer !== last) {
      last = d.layer; html += '<div class="group">' + safe(d.layer) + '</div>';
    }
    const active = ui.selected === d.id;
    html += '<a class="result' + (active ? " active" : "") + '" href="' + (forFiles ? fileRoute(d) : routeFor(d)) +
      '" data-result-index="' + visible.indexOf(item) + '"><strong>' + safe(d.name) + '</strong><small>' +
      safe(forFiles ? d.path : (d.qualified + " · " + d.kind)) + '</small></a>';
  }
  if (results.length > visible.length) html += '<div class="empty">Showing first ' + visible.length + ' results. Narrow your search to find more.</div>';
  if (!visible.length) html = '<div class="empty">No matching entries in this version. Try searching all modules or showing internals.</div>';
  el.innerHTML = html;
  ui.cursor = Math.max(0, Math.min(ui.cursor, visible.length-1));
  renderLayers();
}
function renderIntro() {
  ui.selected = "";
  $("article").innerHTML = '<div class="crumb">ANAIERO / DEVELOPER REFERENCE / ' + safe(ui.version.version) + '</div>' +
    '<h1>Explore the native C++20 library.</h1>' +
    '<p class="subtitle">A source-pinned reference covering contracts, services, simulation, host infrastructure and application entry points. Search symbols, inspect declaration signatures, follow member relationships and review exact source lines.</p>' +
    '<div class="pills"><span class="pill green">Immutable source snapshot</span><span class="pill">C++20</span><span class="pill">' + count(ui.version.headers) + ' headers</span></div>' +
    '<div class="cards">' +
    '<div class="card"><div class="metric">' + count(ui.version.symbols) + '</div><p>Indexed declarations</p></div>' +
    '<div class="card"><div class="metric">' + count(ui.version.headers) + '</div><p>Native headers</p></div>' +
    '<div class="card"><div class="metric">' + count(ui.versions.length) + '</div><p>Source versions</p></div>' +
    '</div>' +
    '<div class="section"><h2>Start browsing</h2><div class="cards">' +
    '<div class="card"><a href="#/guides">Architecture & guides ↗</a><p>Understand services, runtime boundaries, sessions and WindBus.</p></div>' +
    '<div class="card"><a href="#/versions">Version and provenance ↗</a><p>Pin documentation to a specific native Git commit.</p></div>' +
    '<div class="card"><a href="legacy.html">Legacy reference ↗</a><p>Previous reference, annotated descriptions and standalone offline export.</p></div>' +
    '</div></div>' +
    '<div class="note">Scope: syntactic C++ declaration indexing with a separate compiler-build CI gate. The reference does not assert complete semantic cross-references, and digital test results are not biological validation.</div>';
  renderResults();
}
function heading(title, eyebrow, subtitle) {
  return '<div class="crumb">ANAIERO / ' + safe(eyebrow) + '</div><h1>' + safe(title) + '</h1>' +
    (subtitle ? '<p class="subtitle">' + safe(subtitle) + '</p>' : "");
}
function badge(text) { return '<span class="pill">' + safe(text) + '</span>'; }
function contextLink(parent) {
  if (!parent) return "";
  const e = ui.symbols.find(x => x.qualified === parent && typeKinds.has(x.kind));
  return e ? '<a href="' + routeFor(e) + '">' + safe(parent) + '</a>' : safe(parent);
}
async function detailOf(id) {
  const bucket = id.slice(2,4);
  if (!/^[a-f0-9]{2}$/.test(bucket)) throw new Error("Invalid symbol ID");
  if (!ui.detailBuckets.has(bucket)) {
    const data = await fetchJSON(ui.version.bucketPrefix + bucket + ".json");
    ui.detailBuckets.set(bucket, data);
  }
  return ui.detailBuckets.get(bucket)[id] || null;
}
async function fileOf(id) {
  if (!/^f-[a-f0-9]{16}$/.test(id)) throw new Error("Invalid source ID");
  if (!ui.sources.has(id)) {
    ui.sources.set(id, await fetchJSON(ui.version.filesPrefix + id + ".json"));
  }
  return ui.sources.get(id);
}
async function openSymbol(id, token) {
  const preview = ui.symbolById.get(id);
  if (!preview) {
    $("article").innerHTML = heading("Symbol not found","INVALID LINK","This symbol is not indexed in the selected version.") +
      '<p><a href="#/overview">Go to overview</a></p>';
    return;
  }
  ui.selected = id;
  renderResults();
  loading(preview.qualified);
  const entry = await detailOf(id);
  if (token !== ui.routeToken) return;
  if (!entry) throw new Error("Symbol detail missing from shard: " + id);
  const members = ui.symbols.filter(s => s.parentId === id &&
    (ui.internal || (s.access !== "private" && s.access !== "protected")));
  const source = sourceURL(entry.path,entry.line);
  let html = heading(entry.qualified,entry.layer.toUpperCase(),entry.description || "Source-derived C++ declaration") +
    '<div class="pills">' + badge(entry.kind) + badge(entry.access || "unknown access") +
    badge(entry.descriptionSource || "declaration") + badge(ui.version.version) + '</div>';
  html += '<div class="toolbar"><button class="btn" type="button" data-copy-link>Copy permalink</button>' +
    '<a class="btn" target="_blank" rel="noopener noreferrer" href="' + safe(source) + '">Original source ↗</a>' +
    '<button type="button" class="btn" data-show-source="' + safe(entry.fileId) + '" data-line="' + entry.line + '">Preview source</button></div>';
  html += '<div class="section"><h2>Declaration</h2><div class="code"><div class="code-head">' + safe(entry.path) +
    ' · line ' + entry.line + '</div><pre><code>' + safe(entry.signature) + '</code></pre></div></div>';
  if (entry.parent) html += '<div class="section"><h2>Declaring type</h2><p>' + contextLink(entry.parent) + '</p></div>';
  if (entry.details && entry.details.length) html += '<div class="section"><h2>Design notes</h2>' +
    entry.details.map(x => '<p class="summary">' + safe(x) + '</p>').join("") + '</div>';
  if (entry.returns) html += '<div class="section"><h2>Returns</h2><p>' + safe(entry.returns) + '</p></div>';
  if (members.length) {
    const groups = new Map();
    for (const item of members) {
      const key = item.kind;
      if (!groups.has(key)) groups.set(key, []);
      groups.get(key).push(item);
    }
    html += '<div class="section"><h2>Members (' + members.length + ')</h2>';
    for (const [kind, items] of groups) {
      html += '<h3>' + safe(kind[0].toUpperCase()+kind.slice(1)) + '</h3>';
      const families = new Map();
      for (const item of items) {
        if (!families.has(item.name)) families.set(item.name, []);
        families.get(item.name).push(item);
      }
      for (const [name, overloads] of Array.from(families).slice(0,180)) {
        const rows = overloads.map(m => '<div class="member"><div><a href="' + routeFor(m) +
          '"><strong>' + safe(m.name) + '</strong></a><div class="sig">' + safe(m.signature || m.qualified) +
          '</div></div><small>' + safe(m.access || "") + '</small></div>').join("");
        html += overloads.length > 1 ?
          '<details class="overloads"><summary><strong>' + safe(name) + '</strong> · ' +
          overloads.length + ' overloads</summary>' + rows + '</details>' : rows;
      }
      if (families.size > 180) html += '<p class="summary">Additional member families can be found with search.</p>';
    }
    html += '</div>';
  }
  html += '<div class="section" id="source-preview"></div>' +
    '<div class="note">The source commit is fixed at <code>' + safe(ui.version.commit) +
    '</code>. Source annotations are labeled by provenance; unverified statements should not be interpreted as experimental evidence.</div>';
  $("article").innerHTML = html;
}
function numberedSource(contents, start, end, selected) {
  const sourceLines = contents.split(/\r?\n/);
  const a = Math.max(1, start || 1), b = Math.min(sourceLines.length, end || sourceLines.length);
  const rows = [];
  for (let n=a; n<=b; n++) {
    const line = sourceLines[n-1];
    rows.push((n === selected ? "▶ " : "  ") + String(n).padStart(5," ") + " │ " + line);
  }
  return rows.join("\n");
}
async function showSource(id, line) {
  const area = $("source-preview");
  if (!area) return;
  area.innerHTML = '<h2>Source preview</h2><p class="summary">Loading source…</p>';
  const file = await fileOf(id);
  const lines = file.source.split(/\r?\n/);
  const content = numberedSource(file.source, Math.max(1,line-12), Math.min(lines.length,line+65),line);
  area.innerHTML = '<h2>Source preview</h2><p class="summary">Source SHA-256: <code>' + safe(file.sha256) + '</code></p>' +
    '<div class="code"><div class="code-head">' + safe(file.path) + '<a target="_blank" rel="noopener noreferrer" href="' + safe(sourceURL(file.path,line)) +
    '">GitHub ↗</a></div><pre><code>' + safe(content) + '</code></pre></div>';
}
async function openFile(id, token) {
  const meta = ui.fileById.get(id);
  if (!meta) { $("article").innerHTML = heading("File not found","INVALID LINK"); return; }
  ui.selected = id;renderResults();loading(meta.name);
  const file = await fileOf(id);
  if (token !== ui.routeToken) return;
  const symbols = ui.symbols.filter(s => s.fileId === id && (ui.internal || (s.access !== "private" && s.access !== "protected")));
  let html = heading(file.name, "SOURCE FILE", file.path) +
    '<div class="pills">' + badge(file.layer) + badge(file.lineCount+" lines") + badge(file.symbolCount+" declarations") + '</div>' +
    '<p><a target="_blank" rel="noopener noreferrer" href="' + safe(sourceURL(file.path,1)) + '">Open source at immutable Git commit ↗</a></p>';
  if (symbols.length) html += '<div class="section"><h2>Declarations</h2>' + symbols.slice(0,140).map(s=>
    '<div class="member"><a href="' + routeFor(s) + '"><strong>' + safe(s.qualified) +
    '</strong></a><small>line ' + s.line + '</small></div>').join("") + '</div>';
  html += '<div class="section"><h2>Full source</h2><div class="code"><div class="code-head">' + safe(file.path) +
    '</div><pre><code>' + safe(numberedSource(file.source,1,file.lineCount,0)) + '</code></pre></div></div>';
  $("article").innerHTML = html;
}
function renderGuides() {
  ui.selected = "";
  const guides = [
    ["Native implementation status","docs/software/native_implementation_status.md","Current integrated features and limitations"],
    ["Integrated native sessions","docs/software/CE-SW-31_integrated_native_sessions.md","Session ownership, live simulation and recovery"],
    ["Reusable experiments","docs/software/CE-SW-32_reusable_native_experiments.md","Experiment definitions, trials and durable receipts"],
    ["Sustained storage","docs/software/CE-SW-33_sustained_storage.md","History, segmentation and retention"],
    ["Scientific supervision","docs/software/CE-SW-34_supervised_scientific_sessions.md","Supervised model providers and controlled execution"],
    ["Authenticated mesh","docs/software/CE-SW-35_authenticated_native_mesh.md","Enrollment, TLS, replay and verification"],
    ["Environment and calibration","docs/software/CE-SW-36_environment_and_calibration.md","Observable units, frozen calibration and holdouts"]
  ];
  $("article").innerHTML = heading("Engineering guides","DOCUMENTATION","Read source-pinned architecture notes, specifications and current implementation status.") +
    '<div class="cards">' + guides.map(([title,path,desc]) =>
      '<div class="card"><a target="_blank" rel="noopener noreferrer" href="' + safe(sourceURL(path,0)) + '">' + safe(title) +
      ' ↗</a><p>' + safe(desc) + '</p></div>').join("") + '</div>' +
    '<div class="note">These Markdown guides come from the selected source commit. They may describe design intent or limitations rather than independently validated outcomes.</div>' +
    '<p><a href="legacy.html#/guides">Browse earlier annotated guides and offline archive ↗</a></p>';
}
function renderVersions() {
  ui.selected="";
  $("article").innerHTML = heading("Versions and provenance","SOURCE REVISIONS",
    "Every published version is linked to a fixed C++ source commit, with its own immutable declaration index and source files.") +
    '<div class="note">Automatic synchronization follows the native source branch, not the main docs branch. A compiler-build and static-data check gate publication.</div>' +
    '<div class="section"><h2>Published snapshots</h2>' + ui.versions.map(v =>
      '<div class="version-row"><div><strong>' + safe(v.version) + '</strong><small>Commit ' +
      safe(v.commit) + '</small><small>' + count(v.symbols) + ' symbols, ' + count(v.headers) + ' headers, ' +
      count(v.warningCount) + ' syntax-warning headers</small></div><div><a href="' +
      safe(versionLink(v.version,"#/overview")) + '">Open ↗</a>' +
      (v.version === ui.version.version ? '' : '<button type="button" class="btn" data-compare-version="' +
      safe(v.version) + '">Compare</button>') + '</div></div>').join("") +
    '<div id="version-diff" aria-live="polite"></div></div>' +
    '<div class="section"><h2>Source identity</h2><p class="source-ref">' + safe(ui.version.sourceRepo) +
    ' · ' + safe(ui.version.sourceRef) + '</p><p><code>' + safe(ui.version.commit) + '</code></p>' +
    '<p><a target="_blank" rel="noopener noreferrer" href="' + safe(sourceURL("native/include",0)) +
    '">Browse source tree on GitHub ↗</a></p></div>';
}
async function compareVersion(previousSlug) {
  const other = ui.versions.find(v => v.version === previousSlug);
  const output = $("version-diff");
  if (!other || !output) return;
  output.innerHTML = '<p class="summary">Comparing source-pinned declaration inventories…</p>';
  const source = await fetchJSON(other.index);
  const existing = new Map(source.symbols.map(s => [s.id,s]));
  const current = new Map(ui.symbols.map(s => [s.id,s]));
  const added = ui.symbols.filter(s => !existing.has(s.id));
  const removed = source.symbols.filter(s => !current.has(s.id));
  const outputList = (items, available) => items.slice(0,75).map(s =>
    '<div class="member">' + (available ? '<a href="' + routeFor(s) + '">' : '<span>') +
    safe(s.qualified) + (available ? '</a>' : '</span>') +
    '<small>' + safe(s.kind) + '</small></div>').join("") +
    (items.length > 75 ? '<p class="summary">Showing first 75 entries.</p>' : '');
  output.innerHTML = '<div class="section"><h2>Declaration diff: ' + safe(previousSlug) + ' → ' +
    safe(ui.version.version) + '</h2><div class="pills">' + badge(added.length+" added/changed") +
    badge(removed.length+" removed/changed") + '</div>' +
    '<p class="summary">Identity-based syntactic comparison. Modified signatures may appear as one removal and one addition; this is not a source compatibility verdict.</p>' +
    '<h3>New or changed in selected version</h3>' + outputList(added,true) +
    '<h3>Absent from selected version</h3>' + outputList(removed,false) + '</div>';
}
async function readRoute() {
  let hash; try { hash = decodeURIComponent(location.hash || "#/overview"); } catch (_) { hash = "#/overview"; }
  const pair = hash.replace(/^#\/?/,"").split("/");
  const token = ++ui.routeToken;
  try {
    if (pair[0] === "symbol" && pair[1]) await openSymbol(pair[1], token);
    else if (pair[0] === "file" && pair[1]) { ui.kind="files"; await openFile(pair[1], token); }
    else if (pair[0] === "guides") renderGuides();
    else if (pair[0] === "versions") renderVersions();
    else renderIntro();
  } catch (err) {
    if (token !== ui.routeToken) return;
    $("article").innerHTML = heading("Documentation could not load","REFERENCE ERROR",err.message) +
      '<p><a href="legacy.html">Open the legacy reference ↗</a></p>';
    console.error(err);
  }
}
function bind() {
  $("search").addEventListener("input", event => {
    ui.query = event.target.value.trim();
    if (ui.query && ui.kind === "types") ui.kind = "all";
    renderResults();
  });
  $("search").addEventListener("keydown", event => {
    if (event.key === "ArrowDown" || event.key === "ArrowUp") {
      event.preventDefault();
      const results = Array.from(document.querySelectorAll(".result"));
      if (!results.length) return;
      ui.cursor = Math.max(0, Math.min(results.length-1,ui.cursor + (event.key==="ArrowDown" ? 1 : -1)));
      results[ui.cursor].focus();
    } else if (event.key === "Enter") {
      const item = document.querySelector(".result"); if (item) location.hash = item.getAttribute("href");
    }
  });
  $("internal").addEventListener("change", e => {ui.internal = e.target.checked;renderResults();});
  document.querySelectorAll(".facet").forEach(b => b.addEventListener("click",()=>{
    ui.kind=b.dataset.kind;ui.cursor=0;renderResults();
  }));
  document.addEventListener("click", event => {
    const b = event.target.closest("[data-layer]");
    if (!b) return;
    ui.layer=b.dataset.layer;ui.query="";$("search").value="";
    ui.kind="types";location.hash="#/overview";renderResults();
  });
  $("version").addEventListener("change", e => {location.assign(versionLink(e.target.value,location.hash));});
  $("theme").addEventListener("click", () => {
    const next = document.documentElement.dataset.theme === "light" ? "dark" : "light";
    document.documentElement.dataset.theme = next;
    try {localStorage.setItem("anaiero-theme",next)} catch (_) {}
  });
  $("menu").addEventListener("click",()=> {
    document.body.classList.toggle("menu-open");
    $("menu").setAttribute("aria-expanded", String(document.body.classList.contains("menu-open")));
  });
  document.addEventListener("keydown",e=>{
    const active = document.activeElement;
    if (((e.metaKey || e.ctrlKey) && e.key.toLowerCase()==="k") ||
        (e.key==="/" && !["INPUT","TEXTAREA"].includes(active.tagName))) {
      e.preventDefault(); $("search").focus();$("search").select();
    }
    if (e.key === "Escape") {
      ui.query="";$("search").value="";renderResults();$("search").blur();
      document.body.classList.remove("menu-open");
    }
  });
  document.addEventListener("click", async e => {
    const copy = e.target.closest("[data-copy-link]");
    if (copy) {
      try { await navigator.clipboard.writeText(location.href);toast("Permalink copied");}
      catch (_) {toast("Copy the page URL from your address bar");}
    }
    const preview = e.target.closest("[data-show-source]");
    if (preview) {
      preview.disabled=true;
      try {await showSource(preview.dataset.showSource, Number(preview.dataset.line));}
      catch(err) {toast("Could not load source: " + err.message);}
      finally {preview.disabled=false;}
    }
    const compare = e.target.closest("[data-compare-version]");
    if (compare) {
      compare.disabled = true;
      try {await compareVersion(compare.dataset.compareVersion);}
      catch (err) {toast("Comparison unavailable: " + err.message);}
      finally {compare.disabled = false;}
    }
    if (e.target.closest(".navlink")) document.body.classList.remove("menu-open");
  });
  window.addEventListener("hashchange", readRoute);
}
async function main() {
  bind();
  try {
    const versions = await fetchJSON("versions.json");
    if (versions.schema !== 2 || !Array.isArray(versions.versions) || !versions.versions.length)
      throw new Error("No supported source snapshots");
    ui.versions = versions.versions;
    const requested = uiURL.searchParams.get("v") || versions.current;
    ui.version = ui.versions.find(v => v.version === requested);
    if (!ui.version) throw new Error("Requested source version does not exist: " + requested);
    const data = await fetchJSON(ui.version.index);
    if (!Array.isArray(data.symbols) || !Array.isArray(data.files))
      throw new Error("Invalid API index structure");
    ui.symbols = data.symbols;
    ui.files = data.files;
    ui.symbolById = new Map(ui.symbols.map(e => [e.id,e]));
    ui.fileById = new Map(ui.files.map(e => [e.id,e]));
    renderVersionControl();renderLayers();renderResults();
    await readRoute();
  } catch(err) {
    console.error(err);
    $("status").textContent = "Reference unavailable";
    $("article").innerHTML = heading("Native reference unavailable","LOAD ERROR", err.message) +
      '<p><a class="btn" href="legacy.html">Open the previous reference ↗</a></p>';
  }
}
main();
})();
