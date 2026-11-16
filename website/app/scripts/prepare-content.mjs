/**
 * Source-driven Anaiero static content preparation.
 *
 * Reads ONLY explicit records from website/planning/content-registry.json,
 * validates their path against the repository root and approved text suffixes,
 * and emits a read-only, build-time static corpus. Never reads private logs,
 * patent files, .env files, binary archives, or unknown site source paths.
 */
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';

const HERE=path.dirname(fileURLToPath(import.meta.url));
const APP=path.resolve(HERE,'..');
const ROOT=path.resolve(APP,'../..');
const PUBLIC=path.join(APP,'public','content');
const SOURCES=JSON.parse(fs.readFileSync(path.join(ROOT,'website/planning/content-registry.json'),'utf8'));
const EXTENSIONS=new Set(['.md','.json','.py','.txt','.yml','.yaml']);
const RESTRICTED=/(^|\/)(private|secret|personal|patent|claims|logs|archive-private|source_documents)(\/|$)/i;
const deniedFilename=/(^|\/)[^.]*patent[^/]*\.(pdf|docx|md|txt|json)$/i;

if(!Array.isArray(SOURCES.sources)||SOURCES.sources.length<300) throw new Error('Source registry absent or unexpectedly small');
fs.rmSync(PUBLIC,{recursive:true,force:true});
fs.mkdirSync(path.join(PUBLIC,'documents'),{recursive:true});
fs.mkdirSync(path.join(PUBLIC,'media'),{recursive:true});
const sha=(text)=>crypto.createHash('sha256').update(text).digest('hex');
const clean=(text)=>text
  .replace(/^---\s*\n[\s\S]*?\n---\s*\n/,'')
  .replace(/<script[\s\S]*?<\/script>/gi,'[Script omitted from searchable document]')
  .trim();
const normalize=(x)=>x.replace(/[\r\n\t]+/g,' ').replace(/\s+/g,' ').trim();
const titleFor=(raw,source)=>{
  const front=raw.match(/^---[\s\S]*?\n(?:title|name):\s*["']?([^\n"']+)/im);
  const heading=clean(raw).match(/^#\s+(.+)/m);
  let title=(front?.[1]||heading?.[1]||path.basename(source).replace(/\.[^.]+$/,'').replace(/_/g,' ')).trim();
  return title.length>140?title.slice(0,140):title;
};
const safePath=(source)=>{
 if(typeof source!=='string'||source.startsWith('/')||source.includes('\\')||source.split('/').includes('..')) throw Error('Unsafe source '+source);
 if(RESTRICTED.test(source)||deniedFilename.test(source)) throw Error('Non-public or patent file in registry: '+source);
 if(!EXTENSIONS.has(path.extname(source).toLowerCase())) throw Error('Unapproved source type: '+source);
 const absolute=path.resolve(ROOT,source);
 if(!absolute.startsWith(ROOT+path.sep)||!fs.statSync(absolute).isFile()) throw Error('Missing/unsafe file: '+source);
 return absolute;
};
const assets=new Set();
const imageExt=new Set(['.png','.jpg','.jpeg','.webp','.gif','.svg']);
function safeMedia(source,ref,alt='Source illustration'){
 if(/^https?:\/\//i.test(ref))return '!['+alt+']('+ref+')';
 const local=decodeURIComponent(ref.split('#')[0].split('?')[0]);
 if(local.startsWith('/')||local.includes('\\')||local.split('/').includes('..')&&local.startsWith('..'))return '[Figure link withheld: unsafe path]';
 const absolute=path.resolve(ROOT,path.dirname(source),local);
 if(!absolute.startsWith(ROOT+path.sep)||!fs.existsSync(absolute)||!fs.statSync(absolute).isFile())return '[Figure source unavailable]';
 const ext=path.extname(absolute).toLowerCase();
 if(!imageExt.has(ext)||fs.statSync(absolute).size>10_000_000)return '[Figure format unavailable]';
 if(RESTRICTED.test(absolute.slice(ROOT.length+1))||deniedFilename.test(absolute))return '[Figure withheld from publication]';
 const digest=sha(fs.readFileSync(absolute));
 const name=digest.slice(0,24)+ext;
 const dest=path.join(PUBLIC,'media',name);
 if(!assets.has(name)){fs.copyFileSync(absolute,dest);assets.add(name)}
 return '!['+alt.replaceAll(']','')+'](content/media/'+name+')';
}
const records=[],searchIndex=[],seenSrc=new Set(),seenRoute=new Set();
for(const r of SOURCES.sources){
 const source=r.source_path;
 if(seenSrc.has(source)) throw Error('Duplicate source '+source);
 if(seenRoute.has(r.planned_route)) throw Error('Duplicate planned route '+r.planned_route);
 seenSrc.add(source);seenRoute.add(r.planned_route);
 const src=safePath(source);
 const raw=fs.readFileSync(src,'utf8');
 if(raw.length>4_000_000) throw Error('Source too large for static site: '+source);
 let content=clean(raw);
 // Pandoc DOCX conversion preserved figures as HTML <img> tags.
 content=content.replace(/<img\b[^>]*>/gi,tag=>{
  const src=tag.match(/\bsrc=["']([^"']+)["']/i)?.[1];
  const alt=tag.match(/\balt=["']([^"']*)["']/i)?.[1]||'Source scientific figure';
  return src?safeMedia(source,src,alt):'[Source figure without image path]';
 });
 // Original Markdown image syntax is also normalized when locally accessible.
 content=content.replace(/!\[([^\]]*)\]\(([^)]+)\)/g,(all,alt,ref)=>
   ref.startsWith('content/media/')?all:safeMedia(source,ref,alt));
 const id=sha(source).slice(0,16);
 const ext=path.extname(source).toLowerCase();
 const title=titleFor(raw,source);
 const summary=normalize(content.replace(/[\`*_#>|]/g,' ')).slice(0,270);
 const record={
  id,source_path:source,source_sha:r.source_blob_sha,
  title,summary,group:r.group,kind:r.kind,
  status:r.source_status,audiences:r.audiences||[],
  phase:r.phase||null,planned_route:r.planned_route,
  size:Buffer.byteLength(raw,'utf8'),ext
 };
 fs.writeFileSync(path.join(PUBLIC,'documents',id+'.json'),JSON.stringify({...record,content}));
 records.push(record);
 const plain=normalize(content.replace(/(\`{3}|~{3})[\s\S]*?\1/g,' '));
 searchIndex.push({id,title,search:(title+' '+source+' '+plain).toLowerCase().slice(0,25000)});
}
records.sort((a,b)=>a.title.localeCompare(b.title));
const manifest={version:'v1-static',copyright:'© 2026 We Develop Software LLC',generated_from:process.env.GITHUB_SHA||SOURCES.source_commit||'local',count:records.length,documents:records};
fs.writeFileSync(path.join(PUBLIC,'manifest.json'),JSON.stringify(manifest));
fs.writeFileSync(path.join(PUBLIC,'search-index.json'),JSON.stringify(searchIndex));

const readJSON=(p)=>JSON.parse(fs.readFileSync(path.join(ROOT,p),'utf8'));
const modules=readJSON('specifications/v3/rigorous-module-registry.json').modules;
const chips=fs.readdirSync(path.join(ROOT,'registries/v6/module-manifests'))
  .filter(x=>/^CE-[DPR]\d+\.json$/.test(x)).sort()
  .map(f=>readJSON('registries/v6/module-manifests/'+f));
fs.writeFileSync(path.join(PUBLIC,'modules.json'),JSON.stringify(modules));
fs.writeFileSync(path.join(PUBLIC,'chips.json'),JSON.stringify(chips));

const molecularText=fs.readFileSync(path.join(ROOT,'processes/MOLECULAR_INDEX.md'),'utf8');
const molecules=[];
for(const line of molecularText.split(/\r?\n/)){
 if(!line.startsWith('|')||line.startsWith('|---'))continue;
 const fields=line.split('|').slice(1,-1).map(x=>x.trim());
 if(fields.length!==6||fields[0]==='Biological scale'||/^[-:\s]+$/.test(fields[0]))continue;
 molecules.push({domain:fields[0],name:fields[1],role:fields[2],useClass:fields[3],evidence:fields[4],sources:fields[5]});
}
if(molecules.length<20)throw Error('Molecular inventory was not correctly imported');
fs.writeFileSync(path.join(PUBLIC,'molecules.json'),JSON.stringify(molecules));

const groups=Object.fromEntries([...new Set(records.map(x=>x.group))].map(g=>[g,records.filter(x=>x.group===g).length]));
fs.writeFileSync(path.join(PUBLIC,'release.json'),JSON.stringify({source_commit:manifest.generated_from,count:records.length,groups,modules:modules.length,chips:chips.length,molecules:molecules.length,media_assets:assets.size,physical_actuation:false,patent_included:false}));
console.log('Anaiero static corpus prepared:',JSON.stringify({documents:records.length,groups,modules:modules.length,chips:chips.length,molecules:molecules.length,media_assets:assets.size}));
