import { useEffect, useMemo, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { ArrowLeft, ArrowRight, ArrowUpRight, BookOpen, ChevronDown, Clock3, Code2, Database, ExternalLink, FileCode2, Filter, Search, ShieldCheck } from 'lucide-react';
import Markdown from '../components/Markdown';
import { catalog, deepSearch, docById, docByCode, loadDoc, sourceUrl, type DocMeta, type FullDoc } from '../lib/content';

const groups=['All','Engineering','Neuroscience','Living systems','Scientific research','Data schemas','StarChips','Foundations','Historical archive'];
export function Library(){
 const [group,setGroup]=useState('All');
 const [search,setSearch]=useState('');
 const [deep,setDeep]=useState<Set<string>|null>(null);
 const [expanded,setExpanded]=useState(false);
 const [working,setWorking]=useState(false);
 useEffect(()=>{
  const q=search.trim();
  if(q.length<3){setDeep(null);setWorking(false);return;}
  let active=true;setWorking(true);
  const timer=window.setTimeout(()=>{deepSearch(q).then(result=>{if(active){setDeep(result);setWorking(false);}}).catch(()=>{if(active){setDeep(null);setWorking(false);}});},270);
  return()=>{active=false;window.clearTimeout(timer);};
 },[search]);
 const results=useMemo(()=>{
  const q=search.trim().toLowerCase();
  return catalog.documents.filter(d=>{
   if(group!=='All'&&d.group!==group)return false;
   if(group==='All'&&d.group==='Historical archive')return false;
   if(!q)return true;
   const headline=(d.title+' '+d.group+' '+d.id+' '+d.excerpt).toLowerCase();
   return headline.includes(q)||(deep?.has(d.id)??false);
  });
 },[group,search,deep]);
 const visible=expanded?results:results.slice(0,36);
 return <div className="page-wrap">
  <div className="container page-hero docs-hero"><span className="section-kicker">ENGINEERING / RESEARCH LIBRARY</span><h1>The full system,<br/><em>open to examination.</em></h1><p>Current designs, scientific companions, process documentation, source research, original JSON schemas and specialist StarChip manifests—directly readable on the site.</p>
   <div className="library-metrics"><span><strong>24</strong> full module designs</span><span><strong>24</strong> scientific companions</span><span><strong>{catalog.documents.length}</strong> indexed source entries</span></div>
  </div>
  <section className="section docs-list-section"><div className="container">
   <div className="library-tools"><label className="library-search"><Search size={19}/><input type="search" placeholder="Search titles, molecules, documents or system terms…" value={search} onChange={e=>{setSearch(e.target.value);setExpanded(false);}} aria-label="Search the document library"/>{working&&<span className="searching-label">Searching full texts…</span>}</label>
    <div className="library-filter"><Filter size={17}/><span>Collection</span><select aria-label="Filter document collections" value={group} onChange={e=>{setGroup(e.target.value);setExpanded(false);}}>{groups.map(g=><option key={g}>{g}</option>)}</select><ChevronDown size={15}/></div></div>
   <div className="library-layout"><aside className="library-aside"><span className="aside-caption">COLLECTIONS</span>{groups.map(g=><button key={g} type="button" className={'library-category'+(group===g?' active':'')} onClick={()=>{setGroup(g);setExpanded(false);}}><span>{g}</span><span className="category-count">{g==='All'?catalog.documents.filter(x=>x.group!=='Historical archive').length:catalog.documents.filter(x=>x.group===g).length}</span></button>)}
    <div className="aside-note"><ShieldCheck size={19}/><p>Research proposals, third-party findings and implemented software are labeled separately.</p></div></aside>
    <div className="library-results"><div className="library-results-header"><span>{results.length} document{results.length===1?'':'s'} {group==='All'?'in current collections':'in '+group}</span><span>WDS source-controlled corpus</span></div>
     <div className="document-grid">{visible.map(d=><DocumentCard key={d.id} doc={d}/>)}</div>
     {!results.length&&<div className="empty-state"><BookOpen size={29}/><h3>No documents match your search.</h3><p>Try a related biological term, module number or another collection.</p><button type="button" className="text-link" onClick={()=>{setSearch('');setGroup('All');}}>Clear filters <ArrowRight size={15}/></button></div>}
     {visible.length<results.length&&<button type="button" className="button button-outline show-more" onClick={()=>setExpanded(true)}>Show all {results.length} documents <ArrowRight size={17}/></button>}
    </div>
   </div>
  </div></section>
 </div>;
}
export function DocumentCard({doc}:{doc:DocMeta}){
 const Icon=doc.kind==='schema'?Database:doc.kind==='design'?Code2:doc.kind==='companion'?BookOpen:FileCode2;
 return <Link to={doc.href} className="doc-card"><div className="doc-card-top"><span className="doc-icon"><Icon size={18}/></span><span className="doc-kind">{doc.kind.replaceAll('-',' ').toUpperCase()}</span><ArrowUpRight size={16}/></div><h3>{doc.title}</h3><p>{doc.excerpt.slice(0,180)}{doc.excerpt.length>180?'…':''}</p><div className="doc-foot"><span>{doc.revision} · {doc.readMinutes} min</span><span>Read document <ArrowRight size={13}/></span></div></Link>;
}
export function DocumentView(){
 const {id:requested}=useParams();
 const id=requested && !docById(requested) && /^bio-\d+|^sys-\d+/.test(requested)?'process-'+requested:requested;
 const entry=id?docById(id):undefined;
 const [doc,setDoc]=useState<FullDoc|null>(null);
 const [error,setError]=useState(false);
 useEffect(()=>{let alive=true;setDoc(null);setError(false);if(!id)return;
  loadDoc(id).then(d=>{if(alive)setDoc(d);}).catch(()=>{if(alive)setError(true);});
  return()=>{alive=false;};
 },[id]);
 if(!entry)return <div className="container page-hero"><h1>Document not found.</h1><p>Try the <Link to="/docs">full library</Link>.</p></div>;
 const companion=entry.code?(entry.kind==='design'?docByCode(entry.code,'companion'):entry.kind==='companion'?docByCode(entry.code,'design'):undefined):undefined;
 const same=entry.code?catalog.documents.filter(d=>d.code===entry.code&&d.id!==entry.id):[];
 return <div className="page-wrap doc-view-wrap"><div className="container breadcrumb"><Link to="/docs"><ArrowLeft size={15}/> Library</Link><span>/</span><span>{entry.group}</span><span>/</span><strong>{entry.code?.toUpperCase()||entry.kind}</strong></div>
 <div className="container doc-layout"><article className="doc-article">
  <div className="doc-intro"><span className="section-kicker">{entry.group} / {entry.status}</span><h1>{entry.title}</h1><div className="doc-metadata"><span>{entry.revision}</span><span><Clock3 size={13}/> {entry.readMinutes} min read</span><span>© 2026 We Develop Software LLC</span></div>
   <p className="doc-excerpt">{entry.excerpt}</p><div className="doc-action-row">{companion&&<Link className="button button-primary" to={companion.href}>Read {entry.kind==='design'?'scientific companion':'engineering design'} <ArrowRight size={15}/></Link>}<a href={sourceUrl(entry.source)} className="button button-outline" target="_blank" rel="noreferrer">View original source <ExternalLink size={15}/></a></div>
  </div>
  {error?<div className="doc-loading"><h2>This document could not be loaded.</h2><p>The original remains accessible through the source link above.</p></div>:!doc?<div className="doc-loading"><div className="loading-line"/><div className="loading-line shorter"/><p>Loading full source document…</p></div>:<Markdown markdown={doc.markdown} source={doc.source}/>}
 </article><aside className="doc-side"><div className="doc-side-panel"><span className="aside-caption">DOCUMENT DETAILS</span><div className="doc-detail"><small>Document class</small><strong>{entry.kind.replaceAll('-',' ')}</strong></div><div className="doc-detail"><small>Publication state</small><strong>{entry.status}</strong></div><div className="doc-detail"><small>Original source</small><a href={sourceUrl(entry.source)} target="_blank" rel="noreferrer">{entry.source.split('/').pop()} <ExternalLink size={13}/></a></div><div className="doc-detail"><small>Source checksum (SHA-256)</small><code className="hash-code">{entry.sourceHash.slice(0,20)}…</code></div><div className="doc-rights"><ShieldCheck size={18}/><p>Scientific evidence and research designs are not claims of validated living computing.</p></div></div>
   {same.length>0&&<div className="doc-side-panel"><span className="aside-caption">RELATED MODULE FILES</span>{same.map(d=><Link key={d.id} to={d.href} className="side-related"><BookOpen size={16}/>{d.kind.replaceAll('-',' ')} <ArrowRight size={14}/></Link>)}</div>}
  </aside>
 </div></div>;
}
