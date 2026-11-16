import { readFile, readdir, mkdir, writeFile, rm } from 'node:fs/promises';
import { resolve, join, dirname, basename, relative } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
const current=dirname(fileURLToPath(import.meta.url));
const repo=resolve(current,'../../..');
const app=resolve(current,'..');
const out=resolve(app,'public/content');
const catalogPath=resolve(app,'src/generated/catalog.json');
const slug=v=>v.toLowerCase().replace(/\.[a-z0-9]+$/,'').replace(/[^a-z0-9]+/g,'-').replace(/^-|-$/g,'');
const sha=v=>createHash('sha256').update(v).digest('hex');
const clean=v=>v.replace(/^---\s*\n[\s\S]*?\n---\s*\n/,'').trim();
const titleOf=(v,f)=>(clean(v).match(/^#\s+(.+)$/m)?.[1]??f).replace(/[*#]/g,'').trim();
const excerptOf=v=>clean(v).replace(/~~~[\s\S]*?~~~/g,' ').replace(/\[[^\]]+\]\([^)]+\)/g,' ').replace(/[#*|_~<>]/g,' ').replace(/\s+/g,' ').trim().slice(0,270);
async function walk(folder){
 const entries=await readdir(resolve(repo,folder),{withFileTypes:true});
 const found=[];
 for(const item of entries){
  const p=join(folder,item.name).replaceAll('\\','/');
  if(item.isDirectory())found.push(...await walk(p));
  else found.push(p);
 }
 return found;
}
const groups=[
 ['specifications/v3/design',/^CE-A\d+.*_Design\.md$/,'Engineering','design','module-design'],
 ['specifications/v3/companion',/^CE-A\d+.*_Companion\.md$/,'Neuroscience','companion','module-science'],
 ['processes',/\.md$/,'Living systems','process','process'],
 ['research/full-markdown',/^R\d+.*\.md$/,'Scientific research','research','research'],
 ['research/evidence',/\.md$/,'Scientific research','evidence','evidence'],
 ['phases/P01/docs',/\.md$/,'Engineering','phase','p01'],
 ['phases/P02/docs',/\.md$/,'Engineering','phase','p02'],
 ['phases/P02B/docs',/\.md$/,'Engineering','phase','p02b'],
 ['schemas/biology',/\.schema\.json$/,'Data schemas','schema','schema'],
 ['schemas/v6',/\.schema\.json$/,'Data schemas','schema','schema-v6'],
 ['registries/v6/module-manifests',/^CE-[DP]\d+\.json$/,'StarChips','chip','chip'],
 ['docs',/\.md$/,'Foundations','guide','guide'],
 ['archive/v2',/\.md$/,'Historical archive','archive','archive']
];
const documents=[],searchIndex=[],ids=new Set();
await rm(out,{recursive:true,force:true});await mkdir(out,{recursive:true});
for(const [dir,pattern,group,kind,prefix] of groups){
 const paths=await walk(dir);
 for(const source of paths.filter(p=>pattern.test(basename(p)))){
  if(/patent.*\.(docx|pdf)$/i.test(source))continue;
  const raw=await readFile(resolve(repo,source),'utf8');
  const body=kind==='schema'||kind==='chip'?'~~~json\n'+JSON.stringify(JSON.parse(raw),null,2)+'\n~~~':raw;
  const code=source.match(/CE-[A-Z]\d+/)?.[0]?.toLowerCase();
  const id=kind==='design'?code+'-design':kind==='companion'?code+'-companion':kind==='chip'?'chip-'+code:prefix+'-'+slug(relative(dir,source));
  if(ids.has(id))throw new Error('Duplicate content id '+id);
  ids.add(id);
  const title=kind==='schema'?basename(source).replace('.schema.json','').replaceAll('_',' '):kind==='chip'?(JSON.parse(raw).name||code):titleOf(body,basename(source));
  const record={id,title,group,kind,source,code,href:'/docs/'+id,excerpt:excerptOf(body),
   status:kind==='archive'?'Historical':kind==='chip'?'Design proposal':kind==='research'?'Research synthesis':kind==='design'?'Engineering design':'Documentation',
   revision:kind==='design'||kind==='companion'?'v3.1':'2026-11',sourceHash:sha(raw),length:raw.length,
   readMinutes:Math.max(1,Math.ceil(raw.split(/\s+/).length/225))};
  documents.push(record);
  searchIndex.push({id,search:(title+'\n'+group+'\n'+clean(body).replace(/[^\w\s+-]/g,' ').slice(0,12000)).toLowerCase()});
  await writeFile(join(out,id+'.json'),JSON.stringify({...record,markdown:body}));
 }
}
documents.sort((a,b)=>a.group===b.group?a.title.localeCompare(b.title):a.group.localeCompare(b.group));
const posts=[],postDir=join(app,'content/posts');
function parsePost(raw){
 const match=raw.match(/^---\s*\n([\s\S]*?)\n---\s*\n/);
 if(!match)throw new Error('Post requires frontmatter');
 const metadata={};
 for(const line of match[1].split('\n')){
  const m=line.match(/^([a-z_]+):\s*(.*)$/);
  if(m)metadata[m[1]]=m[2].replace(/^["']|["']$/g,'');
 }
 return {metadata,markdown:raw.slice(match[0].length)};
}
for(const file of (await readdir(postDir)).filter(x=>x.endsWith('.md')).sort()){
 const raw=await readFile(join(postDir,file),'utf8'),{metadata,markdown}=parsePost(raw);
 for(const field of ['title','date','summary'])if(!metadata[field])throw new Error(file+' missing '+field);
 const id=slug(file),record={id,title:metadata.title,date:metadata.date,summary:metadata.summary,
  category:metadata.category||'Research update',readMinutes:Math.max(1,Math.ceil(markdown.split(/\s+/).length/220)),
  href:'/journal/'+id,source:'website/app/content/posts/'+file,sourceHash:sha(raw)};
 posts.push(record);await writeFile(join(out,'post-'+id+'.json'),JSON.stringify({...record,markdown}));
}
posts.sort((a,b)=>b.date.localeCompare(a.date));
await mkdir(dirname(catalogPath),{recursive:true});
await writeFile(catalogPath,JSON.stringify({version:'2026-11-15',rights:'© 2026 We Develop Software LLC',documents,posts},null,2)+'\n');
await writeFile(join(out,'search-index.json'),JSON.stringify(searchIndex));
const routes=['/','/system','/living','/science','/docs','/studio','/journal','/about','/interest','/privacy',...documents.map(x=>x.href),...posts.map(x=>x.href)];
const xml='<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'+routes.map(p=>'<url><loc>https://anaiero.io'+p+'</loc></url>').join('\n')+'\n</urlset>\n';
await writeFile(join(app,'public/sitemap.xml'),xml);
console.log('Prepared '+documents.length+' source documents, '+posts.length+' posts and '+routes.length+' routes.');
