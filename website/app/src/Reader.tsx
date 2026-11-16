import {useState,useEffect,useMemo} from 'react';
import type {ReactNode} from 'react';
import {Link,useParams,useSearchParams,Navigate} from 'react-router-dom';
import {ArrowLeft,ArrowUpRight,BookOpen,ChevronRight,Clock3,Database,FileCode2,GitBranch,Search,ShieldCheck} from 'lucide-react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import remarkMath from 'remark-math';
import rehypeKatex from 'rehype-katex';
import rehypeSlug from 'rehype-slug';
import type {DocBody,Manifest} from './data';
import {getData,readRoute,resolveRelative,sourceLink,groupNames} from './data';

const slug=(s:string)=>s.toLowerCase().replace(/<[^>]+>/g,'').replace(/[^\w\s-]/g,'').replace(/\s+/g,'-');
const clean=(s:string)=>s.replace(/^---\s*\n[\s\S]*?\n---\s*\n/,'').replace(/<details><summary>([^<]*)<\/summary>/gi,'\n\n---\n\n### $1\n\n').replace(/<\/details>/gi,'');
export function SourceRedirect({manifest}:{manifest:Manifest}){
 const [params]=useSearchParams();
 const source=params.get('path')||'';
 const record=manifest.documents.find(d=>d.source_path===source);
 return record?<Navigate to={readRoute(record)} replace/>:<div className="empty-state">Unmapped source: {source}. <Link to="/library">Browse the library.</Link></div>;
}
export function DocReader({manifest}:{manifest:Manifest}){
 const {id}=useParams();
 const meta=manifest.documents.find(x=>x.id===id);
 const [doc,setDoc]=useState<DocBody|null>(null),[error,setError]=useState('');
 const [tab,setTab]=useState<'read'|'source'>('read');
 useEffect(()=>{
  let active=true;setDoc(null);setError('');setTab('read');
  if(meta)getData<DocBody>('documents/'+meta.id+'.json').then(x=>{if(active)setDoc(x)}).catch(e=>{if(active)setError(String(e))});
  return()=>{active=false};
 },[meta?.id]);
 const headings=useMemo(()=>doc?.content.match(/^#{2,3}\s+.+$/gm)?.slice(0,60).map(x=>({title:x.replace(/^#+\s+/,'').replace(/[*_]/g,''),level:(x.match(/^#+/)||['##'])[0].length}))||[],[doc]);
 const peer=useMemo(()=>{
  if(!meta)return undefined;
  const p=meta.source_path,other=p.includes('/design/')?p.replace('/design/','/companion/').replace('_Design.md','_Companion.md'):
    p.includes('/companion/')?p.replace('/companion/','/design/').replace('_Companion.md','_Design.md'):'';
  return manifest.documents.find(x=>x.source_path===other);
 },[meta,manifest]);
 if(!meta)return <div className="empty-state"><h1>Source not found</h1><Link to="/library">Browse all documents</Link></div>;
 const renderLink=({href,children}:{href?:string;children?:ReactNode})=>{
  if(!href)return <span>{children}</span>;
  if(href.startsWith('#'))return <a href={href} onClick={e=>{const el=document.getElementById(href.slice(1));if(el){e.preventDefault();el.scrollIntoView({behavior:'smooth'})}}}>{children}</a>;
  if(/^(https?:|mailto:)/i.test(href))return <a href={href} target="_blank" rel="noreferrer noopener">{children}</a>;
  const targetPath=resolveRelative(meta.source_path,href);
  const target=manifest.documents.find(x=>x.source_path===targetPath);
  return target?<Link to={readRoute(target)}>{children}</Link>:<a href={sourceLink(targetPath)} target="_blank" rel="noreferrer noopener">{children}</a>;
 };
 return <div className="reader-layout">
   <main className="reader-main">
    <div className="reader-breadcrumb"><Link to="/library"><ArrowLeft size={15}/> Library</Link><ChevronRight size={13}/><span>{groupNames[meta.group]||meta.group}</span></div>
    <div className="doc-heading">
     <div className="eyebrow">CANONICAL PROJECT RECORD · {meta.phase||meta.kind}</div>
     <h1>{meta.title}</h1><p>{meta.summary}</p>
     <div className="doc-meta"><span className="tag neutral">{groupNames[meta.group]||meta.group}</span><span className="tag muted">{meta.status.replace(/[-_]/g,' ')}</span><span><GitBranch size={13}/>{meta.source_sha.slice(0,9)}</span><span><FileCode2 size={13}/>{Math.round(meta.size/1024)} KB</span></div>
     <div className="doc-actions">
      <button className={'btn btn-tab '+(tab==='read'?'active':'')} onClick={()=>setTab('read')}><BookOpen size={15}/> Readable document</button>
      <button className={'btn btn-tab '+(tab==='source'?'active':'')} onClick={()=>setTab('source')}><FileCode2 size={15}/> Raw source</button>
      <a className="btn btn-ghost" href={sourceLink(meta.source_path)} target="_blank" rel="noreferrer">GitHub original <ArrowUpRight size={14}/></a>
     </div>
    </div>
    {!doc&&!error&&<div className="reader-loading">Loading the complete technical record…</div>}
    {error&&<div className="error-state">{error}</div>}
    {doc&&tab==='source'&&<div className="raw-source"><div className="code-header"><span>Canonical source · {meta.ext.toUpperCase()}</span><button className="subtle-btn" onClick={()=>void navigator.clipboard.writeText(doc.content)}>Copy source</button></div><pre>{doc.content}</pre></div>}
    {doc&&tab==='read'&&<article className="technical-article">
      {meta.ext==='.md'?
       <ReactMarkdown remarkPlugins={[remarkGfm,remarkMath]} rehypePlugins={[rehypeSlug,rehypeKatex]} components={{a:renderLink}}>{clean(doc.content)}</ReactMarkdown>:
       <pre className="code-json">{meta.ext==='.json'?(()=>{try{return JSON.stringify(JSON.parse(doc.content),null,2)}catch{return doc.content}})():doc.content}</pre>}
    </article>}
    <div className="source-footer"><ShieldCheck size={18}/><span>© 2026 We Develop Software LLC · WDS Public Research Reference License. Source-linked research; biological interventions require independent qualification.</span></div>
    {peer&&<Link className="related-peer" to={readRoute(peer)}><BookOpen size={20}/><div><strong>Continue with the paired document</strong><span>{peer.title}</span></div><ArrowUpRight size={17}/></Link>}
   </main>
   <aside className="reader-right">
    <div className="side-inspector"><div className="side-title"><BookOpen size={15}/> In this document</div>
     <nav>{headings.length?headings.map((h,i)=><a key={i} className={h.level===3?'indent':''} href={'#'+slug(h.title)} onClick={e=>{const el=document.getElementById(slug(h.title));if(el){e.preventDefault();el.scrollIntoView({behavior:'smooth'})}}}>{h.title}</a>):<span className="muted-text">Source sections and records</span>}</nav>
    </div>
    <div className="side-inspector"><div className="side-title"><Database size={15}/> Provenance</div>
     <p className="tiny-note">{meta.source_path}</p><p className="tiny-note">Git source revision:<br/>{meta.source_sha.slice(0,16)}</p>
     <a className="quiet-link" href={sourceLink(meta.source_path)} target="_blank" rel="noreferrer">View original <ArrowUpRight size={14}/></a>
    </div>
   </aside>
 </div>;
}
export function Library({manifest}:{manifest:Manifest}){
 const [q,setQ]=useState(''),[group,setGroup]=useState('all'),[history,setHistory]=useState(false);
 const [search,setSearch]=useState<{id:string;search:string}[]|null>(null);
 const groups=useMemo(()=>Array.from(new Set(manifest.documents.map(x=>x.group))).sort(),[manifest]);
 useEffect(()=>{
  if(q.trim().length<3||search)return;
  let active=true;getData<{id:string;search:string}[]>('search-index.json').then(x=>{if(active)setSearch(x)});
  return()=>{active=false};
 },[q,search]);
 const found=useMemo(()=>{
  const query=q.trim().toLowerCase();
  const matching=query.length>=3&&search?new Set(search.filter(x=>x.search.includes(query)).map(x=>x.id)):null;
  return manifest.documents.filter(x=>(group==='all'||x.group===group)&&(history||x.group!=='historical')&&(!query||x.title.toLowerCase().includes(query)||x.source_path.toLowerCase().includes(query)||(matching?.has(x.id)??false)));
 },[manifest,group,history,q,search]);
 return <div className="page-block">
  <div className="section-intro"><div className="eyebrow"><BookOpen size={15}/> COMPLETE DOCUMENT LIBRARY</div><h1>Every design. Every companion.<br/><em>The full technical record.</em></h1><p>All approved source files are readable without downloads. Search, filter, open the full text, and follow scientific connections to original records.</p></div>
  <div className="library-tools"><label className="search-field"><Search size={18}/><input value={q} onChange={e=>setQ(e.target.value)} placeholder="Search modules, terms, molecules and text…" aria-label="Search documentation"/></label>
  <select value={group} onChange={e=>setGroup(e.target.value)} aria-label="Collection"><option value="all">All collections</option>{groups.map(g=><option key={g} value={g}>{groupNames[g]||g}</option>)}</select>
  <label className="toggle-check"><input type="checkbox" checked={history} onChange={e=>setHistory(e.target.checked)}/> Historical versions</label></div>
  <div className="library-summary"><span><strong>{found.length}</strong> documents shown · {manifest.count} source items indexed</span><span><Clock3 size={14}/> Search indexes complete source text</span></div>
  <div className="library-grid">{found.slice(0,180).map(d=><Link key={d.id} to={readRoute(d)} className="library-item"><div className="row-space"><span className="micro-label">{groupNames[d.group]||d.group}</span><ChevronRight size={16}/></div><strong>{d.title}</strong><p>{d.summary}</p><div className="library-foot"><span>{d.phase||d.kind}</span><span>{Math.round(d.size/1024)} KB</span></div></Link>)}</div>
  {found.length>180&&<p className="helper">Showing 180 matches. Search or filter to find other source documents.</p>}
  {!found.length&&<div className="empty-state"><Search size={26}/><h2>No matching documents</h2><button className="btn btn-secondary" onClick={()=>{setQ('');setGroup('all');setHistory(false)}}>Reset filters</button></div>}
 </div>;
}
