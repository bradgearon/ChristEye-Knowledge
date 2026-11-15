import {useEffect,useMemo,useState} from 'react';
import {Link,NavLink,Routes,Route,useLocation,useNavigate} from 'react-router-dom';
import {Activity,ArrowRight,ArrowUpRight,BookOpen,BrainCircuit,CheckCircle2,ChevronRight,CircuitBoard,Code2,Cpu,Database,Dna,ExternalLink,FileText,FlaskConical,Flower2,GitBranch,HeartHandshake,Layers3,Leaf,Menu,Microscope,Moon,Network,Play,Search,Shield,ShieldCheck,Sparkles,Sun,X,Zap} from 'lucide-react';
import Atlas from './Atlas';
import {DocReader,Library,SourceRedirect} from './Reader';
import {InteractiveLab,Studio} from './Studio';
import type {Manifest,DocMeta,ModuleProfile,Molecule} from './data';
import {readRoute,groupNames,sourceLink,useData} from './data';

const nav=[
 {to:'/',label:'Overview',icon:Flower2},
 {to:'/system',label:'System atlas',icon:Network},
 {to:'/living',label:'Living systems',icon:Leaf},
 {to:'/science',label:'Science',icon:Microscope},
 {to:'/build',label:'Build',icon:Code2},
 {to:'/studio',label:'StarChip Studio',icon:Cpu},
 {to:'/scholarship',label:'Scholarship',icon:BookOpen},
 {to:'/library',label:'Library',icon:Database}
];
const keyConcepts=[
 {term:'Soil',meaning:'The entire proposed living organoid, not merely its surrounding extracellular matrix.',source:'processes/BIO-03-soil-organoid.md'},
 {term:'Root',meaning:'A candidate spinal-derived physical subsystem or a demonstrated delayed functional relation—never assumed to be identical.',source:'processes/BIO-02-axon-spinal-root.md'},
 {term:'Crest',meaning:'Qualified biological source, lineage and cell-observation records, including uncertainty.',source:'processes/BIO-01-crest-lineage.md'},
 {term:'Lotus',meaning:'A permissioned digital coordinator; biological local-coordination hypotheses have separate evidence.',source:'specifications/v3/design/CE-A12_lotus_coordinator_Design.md'},
 {term:'Spirit',meaning:'A named functional neuronal/glial participant or digital subsystem, not a validated claim of conscious agency.',source:'processes/BIO-06-teaching.md'},
 {term:'Milk',meaning:'Task-independent qualified physiological support and sustaining care.',source:'processes/BIO-10-care-health.md'},
 {term:'Honey',meaning:'Selective outcome-contingent instruction or supportive feedback, separated from physical nourishment.',source:'processes/BIO-04-sustibus-chemibus.md'},
 {term:'Bread',meaning:'Foundational source-grounded curriculum, lessons and basic capabilities.',source:'processes/SYS-11-module-training.md'},
 {term:'Meat',meaning:'Advanced integrated curriculum and demonstration of specialist competence.',source:'processes/SYS-11-module-training.md'},
 {term:'Wind',meaning:'A typed software message or specifically qualified measured biological influence.',source:'docs/windbus.md'},
 {term:'Crystal',meaning:'A verified digital teaching pack, or a separate unvalidated material memory-carrier hypothesis.',source:'processes/BIO-08-crystals.md'},
 {term:'Heart',meaning:'The scoped biological response-forming role, distinct from the independent physical limit controller.',source:'docs/architecture.md'},
 {term:'Love',meaning:'A human-centered value of care, non-manipulation and compassionate interaction; not an inferred organoid feeling.',source:'specifications/v3/design/CE-A21_affective_support_and_honey_Design.md'},
 {term:'Power',meaning:'Measurable computation, electrical transport and resource costs; not free energy created by biological learning.',source:'docs/hardware.md'}
];
const phases=[['P01','Physical reference and source contracts','Documented'],['P02','Cell-free WindBus and teaching/evaluator','Software'],['P02B','Supervisory and scientific roles','Software'],['P02C','Commission actual development boards','Planned'],['P03–P06','Neural preparation, learning, multi-module Soil','Research'],['P07–P08','Physical Crystal and deployment','Hypothesis']];
function SourceButton({path,manifest,label='Open full specification'}:{path:string;manifest:Manifest;label?:string}){
 const doc=manifest.documents.find(x=>x.source_path===path);
 return doc?<Link className="quiet-link" to={readRoute(doc)}>{label} <ArrowUpRight size={15}/></Link>:<a className="quiet-link" href={sourceLink(path)} target="_blank" rel="noreferrer">{label} <ExternalLink size={15}/></a>;
}
function SectionHeader({eyebrow,title,description}:{eyebrow:string;title:string;description?:string}){
 return <div className="section-heading-lg"><span className="eyebrow">{eyebrow}</span><h2>{title}</h2>{description&&<p>{description}</p>}</div>;
}
function ResearchBoundary(){return <div className="boundary"><ShieldCheck size={18}/><span><strong>Research boundary.</strong> Digital examples and qualified external literature are not evidence that a complete biological computer, physical Crystal or autonomous cell-growth system exists today.</span></div>}
function Homepage({manifest}:{manifest:Manifest}){
 const modules=manifest.documents.filter(x=>x.group==='current-module-design').length;
 return <div className="home">
  <section className="hero-main">
   <div className="hero-orbit orbit-one"/><div className="hero-orbit orbit-two"/>
   <div className="hero-content"><div className="eyebrow"><span className="pulse-dot"/> WE DEVELOP SOFTWARE LLC · ANAIERO RESEARCH</div>
   <h1>A computer that learns<br/>through <em>connection.</em></h1>
   <p>ChristEye Anaiero is a framework for modular intelligence, durable knowledge, specialist capabilities and the scientific study of living neural systems.</p>
   <div className="hero-actions"><Link to="/system" className="btn btn-primary btn-large">Explore the architecture <ArrowRight size={17}/></Link><Link to="/studio" className="btn btn-outline btn-large"><Play size={16}/> Try a digital laboratory</Link></div>
   <div className="hero-proof"><span><CheckCircle2 size={15}/> Tested digital references</span><span><Microscope size={15}/> Biological research proposed</span><span><Shield size={15}/> WDS research property</span></div></div>
   <div className="hero-symbol" aria-hidden="true"><div className="hero-symbol-ring ring-a"/><div className="hero-symbol-ring ring-b"/><div className="hero-symbol-core"><BrainCircuit size={68} strokeWidth={1}/></div><span className="hero-symbol-label label-1">LOTUS</span><span className="hero-symbol-label label-2">ROOTS</span><span className="hero-symbol-label label-3">WINDS</span><span className="hero-symbol-label label-4">SOIL</span></div>
  </section>
  <div className="home-quickstats"><div><strong>{modules}</strong><span>Module engineering plans</span></div><div><strong>12</strong><span>Biological & systems processes</span></div><div><strong>{manifest.count}</strong><span>Browsable source records</span></div><div><strong>7</strong><span>Interactive digital explorations</span></div></div>
  <section className="feature-section"><SectionHeader eyebrow="01 / SYSTEM OVERVIEW" title="One architecture. Many ways to understand." description="Explore the same source-controlled engineering and science from the perspective that matters to you."/>
   <div className="lens-grid">{[
    {to:'/living',name:'Living Systems',icon:Leaf,info:'Soil, cell lineage, axons, spinal Roots, growth and care',cls:'warm'},
    {to:'/build',name:'Developer Workbench',icon:Code2,info:'Typed messages, reference code, module contracts and schemas',cls:'teal'},
    {to:'/science',name:'Scientific Atlas',icon:FlaskConical,info:'Molecules, original studies, hypotheses and evidence',cls:'violet'},
    {to:'/studio',name:'StarChip Studio',icon:Cpu,info:'Run digital simulations and inspect proposed capabilities',cls:'teal'},
    {to:'/scholarship',name:'Scholarship',icon:BookOpen,info:'Knowledge, Understanding, Wisdom and conceptual roots',cls:'warm'},
    {to:'/library',name:'Complete Library',icon:Database,info:'Every full design, companion, phase and original record',cls:'violet'}].map(x=><Link key={x.to} to={x.to} className={'lens-card '+x.cls}><div className="lens-icon"><x.icon size={22}/></div><h3>{x.name} <ArrowUpRight size={16}/></h3><p>{x.info}</p></Link>)}</div>
  </section>
  <section className="feature-section"><div className="split-heading"><SectionHeader eyebrow="02 / CONNECTED SYSTEMS" title="Inside Anaiero's architecture" description="Select a component to examine the exact difference between digital implementation, biological hypothesis and external protection."/><Link className="quiet-link" to="/system">Open full atlas <ArrowUpRight size={16}/></Link></div><Atlas compact/></section>
  <section className="feature-section"><div className="split-heading"><SectionHeader eyebrow="03 / WORKING DIGITAL REFERENCE" title="Watch a decision form." description="The Lotus test makes its reasoning visible. It can stage digital education, but it cannot authorize biological care or stimulation."/><Link className="quiet-link" to="/studio">More labs <ArrowUpRight size={16}/></Link></div><InteractiveLab initial="lotus"/></section>
  <section className="feature-section"><SectionHeader eyebrow="04 / RESEARCH ETHOS" title="Care, love, and power—defined."/><div className="values-grid"><div><HeartHandshake size={25}/><h3>Care</h3><p>Measured physiological support and dependable stewardship, independent of whether a learner succeeds.</p></div><div><Sparkles size={25}/><h3>Love</h3><p>A commitment to dignity, compassionate design and non-manipulative feedback—not a claim of measured organoid feeling.</p></div><div><Zap size={25}/><h3>Power</h3><p>Real compute, data, electrical and metabolic budgets. Capabilities are bounded by measured resources.</p></div></div></section>
  <section className="end-cta"><span className="eyebrow">ALL THE SCIENCE · ALL THE ENGINEERING</span><h2>Read every original document,<br/>not just an interpretation.</h2><p>{manifest.count} approved source records, with links back to their GitHub revisions and paired research.</p><Link className="btn btn-primary btn-large" to="/library">Enter the research library <ArrowRight size={17}/></Link></section>
  <ResearchBoundary/>
 </div>;
}
function SystemPage({manifest}:{manifest:Manifest}){
 const topics=[
  ['LOTUS','Digital governance and developmental coordination','specifications/v3/design/CE-A12_lotus_coordinator_Design.md'],
  ['WIND BUS','Typed information routing and delivery receipts','docs/windbus.md'],
  ['SOIL','Complete proposed living organoid','processes/BIO-03-soil-organoid.md'],
  ['SPINAL ROOT','Physical tissue candidate and functional memory distinction','processes/BIO-02-axon-spinal-root.md'],
  ['CREST','Qualified cell-source, lineage and identity','processes/BIO-01-crest-lineage.md'],
  ['SUSTI BUS','Physiological care that never depends on a reward','processes/BIO-10-care-health.md'],
  ['CHEMI BUS','Chemical observations, source, assay and uncertainty','processes/BIO-04-sustibus-chemibus.md'],
  ['CRYSTAL','Digital experience pack versus physical carrier hypothesis','processes/BIO-08-crystals.md']
 ];
 return <div className="page-block"><div className="section-intro"><div className="eyebrow"><Network size={15}/> INTERACTIVE ARCHITECTURE</div><h1>Many participants.<br/><em>One accountable system.</em></h1><p>Understand the modules as connected roles, not as an unverified single organism. Choose a lens, inspect a connection, then open its original technical specification.</p></div><Atlas/><div className="feature-section"><SectionHeader eyebrow="THE EIGHT CORE ROLES" title="Every name maps to an engineering responsibility."/><div className="topic-grid">{topics.map(([name,body,path])=><article key={name} className="topic-card"><div className="topic-code">{name}</div><p>{body}</p><SourceButton path={path} manifest={manifest} label="Source and process"/></article>)}</div></div><ResearchBoundary/></div>;
}
const lifeSteps=[
 ['01','Source and Crest','Qualified lineage, clone/observation IDs and human review.','processes/BIO-01-crest-lineage.md','No cell production from software alone'],
 ['02','Soil development','A living organoid with regional cells, processes, matrix and measured coverage.','processes/BIO-03-soil-organoid.md','A larger Soil is not automatically more intelligent'],
 ['03','Axons and spinal Roots','Distinguish neuronal projections, synapses and proposed Root tissue.','processes/BIO-02-axon-spinal-root.md','A structural contact does not prove a learned relation'],
 ['04','Milk and Water','SustiBus continuity of qualified care, distinct from learning feedback.','processes/BIO-10-care-health.md','Nourishment must never depend on task score'],
 ['05','Learning and Honey','Declared inputs, feedback, frozen readers and separate evaluation.','processes/BIO-06-teaching.md','A successful digital task is not proof of biology'],
 ['06','Retained Root','Delayed, content-specific performance with controls and uncertainty.','processes/BIO-05-measurements.md','Correlation cannot locate the exact engram'],
 ['07','Growth and renewal','Differentiate new cells, axons, digital tools and changed function.','processes/BIO-09-growth-control.md','Interventions require qualified human SOP'],
 ['08','Crystal questions','Digital replay is executable; physical carrier transfer is research.','processes/BIO-08-crystals.md','No proven peptide that installs arbitrary knowledge']
];
function LivingPage({manifest}:{manifest:Manifest}){
 const [step,setStep]=useState(0);
 const cur=lifeSteps[step];
 return <div className="page-block"><div className="section-intro"><div className="eyebrow"><Leaf size={15}/> LIVING SYSTEMS · RESEARCH</div><h1>Life as a measured process.<br/><em>Growth with purpose.</em></h1><p>Walk through the proposed biological architecture, from cell-source identity to a tested functional Root. Every transition is tied to observations and scientific gates.</p></div>
  <div className="journey-stepper"><div className="journey-stages">{lifeSteps.map((x,i)=><button key={x[0]} className={'journey-stage '+(i===step?'active':'')} onClick={()=>setStep(i)}><span className="step-number">{x[0]}</span><span>{x[1]}</span><ChevronRight size={16}/></button>)}</div><div className="journey-detail"><span className="eyebrow">STAGE {cur[0]} / 08</span><h2>{cur[1]}</h2><p className="journey-desc">{cur[2]}</p><div className="process-field"><span>RESEARCH BOUNDARY</span><p>{cur[4]}</p></div><SourceButton path={cur[3]} manifest={manifest} label="Read full controlled process"/><div className="row-gap journey-actions"><button className="btn btn-secondary" disabled={step===0} onClick={()=>setStep(s=>Math.max(0,s-1))}>Previous</button><button className="btn btn-primary" disabled={step===lifeSteps.length-1} onClick={()=>setStep(s=>Math.min(lifeSteps.length-1,s+1))}>Next stage <ArrowRight size={15}/></button></div></div></div>
 <div className="feature-section"><SectionHeader eyebrow="BIOLOGICAL MEANINGS" title="Growth is not one operation." description="Cell division, axon extension, synaptic remodeling and verified learning require different measurements."/><div className="four-column">{[
 ['Cell births','Progenitor lineage and observed divisions','processes/BIO-01-crest-lineage.md'],
 ['Axon development','Neuronal processes and functional connectivity','processes/BIO-02-axon-spinal-root.md'],
 ['Cell support','Astrocytic and chemical observations, qualified Milk','processes/BIO-04-sustibus-chemibus.md'],
 ['Retained relation','Delayed task performance with source and control','processes/BIO-06-teaching.md']].map(([title,info,path])=><div className="principle-card" key={title}><div className="small-symbol"><Dna size={20}/></div><h3>{title}</h3><p>{info}</p><SourceButton manifest={manifest} path={path}/></div>)}</div></div>
 <div className="feature-section"><SectionHeader eyebrow="CARE · LOVE · STEWARDSHIP" title="A system should never have to earn its essential care."/><div className="care-banner"><HeartHandshake size={37}/><div><h3>Milk maintains. Honey teaches.</h3><p>Care and learning are separate channels. Scientific oversight, qualified services and independent equipment limits govern living material; software can observe and propose but cannot administer novel biological interventions.</p><SourceButton manifest={manifest} path="processes/BIO-10-care-health.md" label="Read the care and physiology specification"/></div></div></div><ResearchBoundary/>
 </div>;
}
function SciencePage({manifest}:{manifest:Manifest}){
 const {data,error}=useData<Molecule[]>('molecules.json');
 const [query,setQuery]=useState(''),[selected,setSelected]=useState<Molecule|null>(null),[domain,setDomain]=useState('all');
 const domains=Array.from(new Set(data?.map(x=>x.domain)||[]));
 const filtered=(data||[]).filter(x=>(domain==='all'||x.domain===domain)&&(!query||[x.domain,x.name,x.role,x.sources].join(' ').toLowerCase().includes(query.toLowerCase())));
 return <div className="page-block"><div className="section-intro"><div className="eyebrow"><Microscope size={15}/> SCIENCE AND MECHANISMS</div><h1>From molecule to memory,<br/><em>only where evidence leads.</em></h1><p>Browse the published biological and structural candidates, their ordinary roles, and the exact scientific boundaries between measurements, hypotheses and Anaiero implementation.</p></div>
 <div className="science-lead"><div><span className="micro-label">METHODOLOGY</span><h2>What do we know—and how?</h2><p>Each claim needs a named preparation, an assay, a control, an alternative explanation and an evidence label. A pathway appearing in a study does not automatically explain a future Anaiero Root.</p></div><SourceButton path="research/evidence/anaiero-biology-2026-11.md" manifest={manifest} label="Open source-backed evidence register"/></div>
 <div className="section-heading"><span>MOLECULAR & STRUCTURAL ATLAS</span><span>{data?.length||0} research and assay entries</span></div>
 <div className="library-tools"><label className="search-field"><Search size={17}/><input placeholder="Search lactate, BDNF, axons, receptors…" value={query} onChange={e=>setQuery(e.target.value)}/></label><select aria-label="Biological domain" value={domain} onChange={e=>setDomain(e.target.value)}><option value="all">All biological domains</option>{domains.map(d=><option key={d} value={d}>{d}</option>)}</select></div>
 {error&&<p className="error-state">{error}</p>}
 <div className="molecule-layout"><div className="molecule-list">{filtered.map((m,i)=><button key={i} className={'molecule-row '+(selected===m?'selected':'')} onClick={()=>setSelected(m)}><div><small>{m.domain}</small><strong>{m.name}</strong><span>{m.role}</span></div><ChevronRight size={16}/></button>)}</div>
 <div className="molecule-detail">{(selected||filtered[0])?(()=>{const m=selected||filtered[0];return <><div className="eyebrow">SELECTED SCIENTIFIC ENTITY</div><h2>{m.name}</h2><span className="tag muted">{m.useClass}</span><h3>Ordinary biological role</h3><p>{m.role}</p><h3>Required evidence</h3><p>{m.evidence}</p><h3>Source scope</h3><p>{m.sources}</p><div className="process-field"><span>NOT A BIOLOGICAL RECIPE</span><p>This index identifies candidates for characterization or research. It is not an instruction to add a chemical to a culture or encode semantic knowledge in a peptide.</p></div><SourceButton path="processes/MOLECULAR_INDEX.md" manifest={manifest} label="Full source and definitions"/></>})():<p className="tiny-note">Select a biological entity to inspect its interpretation.</p>}</div></div>
 <div className="feature-section"><SectionHeader eyebrow="THE SCIENTIFIC METHOD" title="An evidence trail from observation to understanding."/><div className="evidence-steps">{[['F','Finding','Scoped result in a named published preparation'],['X','Synthesis','Cross-source interpretation; not a new experiment'],['H','Hypothesis','Prediction with a falsifiable outcome'],['D','Design','Interface or operation specified by the project'],['A','Analogy','A meaningful correspondence, not biological proof']].map(x=><div key={x[0]}><strong>{x[0]}</strong><div><h3>{x[1]}</h3><p>{x[2]}</p></div></div>)}</div></div><ResearchBoundary/>
 </div>;
}
function BuildPage({manifest}:{manifest:Manifest}){
 const {data:modules,error}=useData<ModuleProfile[]>('modules.json');
 const [filter,setFilter]=useState('');
 const visible=modules?.filter(x=>(x.id+' '+x.slug+' '+x.substrate).toLowerCase().includes(filter.toLowerCase()))||[];
 return <div className="page-block"><div className="section-intro"><div className="eyebrow"><Code2 size={15}/> DEVELOPER WORKBENCH</div><h1>Build with clarity.<br/><em>Test with evidence.</em></h1><p>Work with the real digital reference, full module contracts, typed schemas and experimental qualification gates. Code and biology have distinct permission boundaries.</p><div className="hero-actions"><SourceButton path="docs/code.md" manifest={manifest} label="Digital quickstart and Python reference"/><SourceButton path="docs/schema-reference.md" manifest={manifest} label="Full schema dictionary"/></div></div>
 <div className="dev-pipeline"><div><CircuitBoard size={22}/><h3>Source</h3><p>Permissioned events and named instrument observations</p></div><ChevronRight size={17}/><div><Network size={22}/><h3>WindBus</h3><p>Versioned routes, clocks, ACLs and receipts</p></div><ChevronRight size={17}/><div><BrainCircuit size={22}/><h3>Lotus</h3><p>Bounded digital decisions and protected skills</p></div><ChevronRight size={17}/><div><Database size={22}/><h3>Roots</h3><p>Evidence, snapshots and delayed evaluations</p></div></div>
 <div className="section-heading"><span>24 COMPLETE MODULE DESIGNS & COMPANIONS</span><label className="search-field compact"><Search size={16}/><input aria-label="Filter modules" value={filter} onChange={e=>setFilter(e.target.value)} placeholder="Filter modules…"/></label></div>
 {error&&<p className="error-state">{error}</p>}
 <div className="module-grid">{visible.map(m=>{
  const design=manifest.documents.find(x=>x.source_path.startsWith('specifications/v3/design/'+m.id+'_'));
  const companion=manifest.documents.find(x=>x.source_path.startsWith('specifications/v3/companion/'+m.id+'_'));
  return <article className="module-card" key={m.id}><div className="module-top"><span>{m.id}</span><span>v3.1</span></div><h3>{m.slug.replaceAll('_',' ')}</h3><p>{m.substrate}</p><div className="module-refs">{design&&<Link to={readRoute(design)}>Engineering design <ArrowUpRight size={14}/></Link>}{companion&&<Link to={readRoute(companion)}>Science companion <ArrowUpRight size={14}/></Link>}</div><small>{m.processes.join(' · ')}</small></article>
 })}</div>
 <div className="feature-section"><SectionHeader eyebrow="BUILD FOR THE ACTUAL PLATFORM" title="Hardware, monitoring and safe interfaces."/><div className="three-panels">{[['Microcontrollers','Supervisory data, system identity, clock domains and watchdogs.','docs/hardware.md'],['Interface contracts','Typed messages, sample conditions, delivery receipts and schema validity.','docs/windbus.md'],['Release gates','Cell-free tests first; real sensors, tissue and growth require qualification.','docs/testing.md']].map(([title,info,path])=><article key={title}><Layers3 size={21}/><h3>{title}</h3><p>{info}</p><SourceButton path={path} manifest={manifest}/></article>)}</div></div><ResearchBoundary/></div>;
}
function Scholarship({manifest}:{manifest:Manifest}){
 const [selected,setSelected]=useState(keyConcepts[0]);
 return <div className="page-block"><div className="section-intro"><div className="eyebrow"><BookOpen size={15}/> KNOWLEDGE · UNDERSTANDING · WISDOM</div><h1>Where language meets<br/><em>the structure of thought.</em></h1><p>Explore the project's underlying words through three lenses: interpretation, engineering representation and ordinary scientific referent. A meaningful metaphor does not automatically establish a biological mechanism.</p></div>
 <div className="scholar-layout"><div className="concept-selector">{keyConcepts.map(c=><button key={c.term} className={selected.term===c.term?'active':''} onClick={()=>setSelected(c)}>{c.term}<ChevronRight size={16}/></button>)}</div><div className="concept-reading"><span className="eyebrow">TERM / INTERPRETATION / IMPLEMENTATION</span><h2>{selected.term}</h2><p>{selected.meaning}</p><div className="concept-views"><div><span>01 · Conceptual lens</span><p>An organizing word used in the project's language and theory.</p></div><div><span>02 · Engineering lens</span><p>A declared role, contract, state, record or subsystem that can be specified and tested.</p></div><div><span>03 · Scientific lens</span><p>A particular physical entity or measurement when evidence supports the relationship; otherwise an open research question.</p></div></div><SourceButton path={selected.source} manifest={manifest} label="Read primary project meaning and constraints"/></div></div><ResearchBoundary/>
 </div>;
}
function Roadmap({manifest}:{manifest:Manifest}){
 return <div className="page-block"><div className="section-intro"><div className="eyebrow"><GitBranch size={15}/> DEVELOPMENT ROADMAP</div><h1>Growth is measured<br/><em>one phase at a time.</em></h1><p>The phases describe evidence gates, not release promises or dates. Passing digital software tests does not authorize experiments on living tissue.</p></div><div className="phase-list">{phases.map(([id,title,status],i)=><div className="phase-row" key={id}><div className="phase-index">{String(i+1).padStart(2,'0')}</div><div><strong>{id}</strong><h3>{title}</h3></div><span className="tag neutral">{status}</span></div>)}</div><SourceButton path="docs/roadmap.md" manifest={manifest} label="Full project release phases"/><ResearchBoundary/></div>;
}
function SearchModal({manifest,close}:{manifest:Manifest;close:()=>void}){
 const [q,setQ]=useState('');
 const matches=manifest.documents.filter(x=>(x.title+x.source_path).toLowerCase().includes(q.toLowerCase())).slice(0,14);
 return <div className="search-backdrop" role="presentation" onClick={close}><div className="search-dialog" role="dialog" aria-modal="true" aria-label="Search documents" onClick={e=>e.stopPropagation()}><div className="search-dialog-input"><Search size={20}/><input autoFocus placeholder="Find a module, study, molecule, or protocol…" value={q} onChange={e=>setQ(e.target.value)}/><button aria-label="Close search" onClick={close}><X size={19}/></button></div><div className="search-results">{(q?matches:manifest.documents.filter(x=>x.group==='current-module-design').slice(0,6)).map(d=><Link key={d.id} onClick={close} to={readRoute(d)}><FileText size={17}/><span><strong>{d.title}</strong><small>{groupNames[d.group]||d.group}</small></span><ArrowUpRight size={15}/></Link>)}{q&&!matches.length&&<p>No title matches. Try the full-text filters in <Link to="/library" onClick={close}>Library</Link>.</p>}</div><div className="search-dialog-foot">For search inside full source documents, open the Library.</div></div></div>;
}
function Shell({manifest}:{manifest:Manifest}){
 const location=useLocation(),navigate=useNavigate();
 const [menu,setMenu]=useState(false),[search,setSearch]=useState(false),[light,setLight]=useState(false);
 useEffect(()=>{window.scrollTo(0,0);setMenu(false)},[location]);
 useEffect(()=>{document.documentElement.dataset.theme=light?'light':'dark'},[light]);
 useEffect(()=>{const handler=(e:KeyboardEvent)=>{if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='k'){e.preventDefault();setSearch(true)}if(e.key==='Escape')setSearch(false)};window.addEventListener('keydown',handler);return()=>window.removeEventListener('keydown',handler)},[]);
 return <div className="site-shell">
  <aside className={'site-sidebar '+(menu?'open':'')}>
   <Link to="/" className="brand"><span className="brand-logo"><Flower2 size={26} strokeWidth={1.3}/></span><span><strong>ChristEye<span> Anaiero</span></strong><small>RESEARCH & ENGINEERING</small></span></Link>
   <div className="nav-label">DISCOVER</div><nav aria-label="Primary navigation">{nav.map(item=><NavLink end={item.to==='/'} key={item.to} to={item.to} className={({isActive})=>'sidebar-link '+(isActive?'active':'')}><item.icon size={18} strokeWidth={1.65}/><span>{item.label}</span><ChevronRight className="sidebar-chevron" size={15}/></NavLink>)}</nav>
   <div className="sidebar-bottom"><div className="sidebar-research"><div className="status-light"/><div><strong>Source-linked research</strong><span>Digital reference · biological research</span></div></div><Link to="/roadmap" className="sidebar-foot-link">Development phases <ArrowUpRight size={13}/></Link><a href="https://github.com/bradgearon/ChristEye-Knowledge" target="_blank" rel="noreferrer" className="sidebar-foot-link">GitHub source <ExternalLink size={13}/></a><span>© 2026 We Develop Software LLC</span></div>
  </aside>
  {menu&&<button className="mobile-scrim" aria-label="Close navigation" onClick={()=>setMenu(false)}/>}
  <div className="site-main">
   <header className="site-header"><div className="header-left"><button className="icon-button mobile-menu" aria-label="Open navigation" onClick={()=>setMenu(!menu)}><Menu size={20}/></button><span className="header-path">ANAIERO <ChevronRight size={13}/> {location.pathname==='/'?'RESEARCH PORTAL':location.pathname.split('/')[1].toUpperCase()}</span></div>
    <div className="header-actions"><button className="header-search" onClick={()=>setSearch(true)}><Search size={17}/><span>Search research…</span><kbd>⌘ K</kbd></button><button className="icon-button" aria-label={light?'Dark theme':'Light theme'} onClick={()=>setLight(v=>!v)}>{light?<Moon size={18}/>:<Sun size={18}/>}</button><a className="header-github" href="https://github.com/bradgearon/ChristEye-Knowledge" target="_blank" rel="noreferrer"><GitBranch size={16}/> <span>Source</span> <ArrowUpRight size={13}/></a></div></header>
   <div className="main-content"><Routes>
    <Route path="/" element={<Homepage manifest={manifest}/>}/>
    <Route path="/system" element={<SystemPage manifest={manifest}/>}/>
    <Route path="/living" element={<LivingPage manifest={manifest}/>}/>
    <Route path="/science" element={<SciencePage manifest={manifest}/>}/>
    <Route path="/build" element={<BuildPage manifest={manifest}/>}/>
    <Route path="/studio" element={<Studio manifest={manifest}/>}/>
    <Route path="/scholarship" element={<Scholarship manifest={manifest}/>}/>
    <Route path="/library" element={<Library manifest={manifest}/>}/>
    <Route path="/read/:id" element={<DocReader manifest={manifest}/>}/>
    <Route path="/read-source" element={<SourceRedirect manifest={manifest}/>}/>
    <Route path="/roadmap" element={<Roadmap manifest={manifest}/>}/>
    <Route path="*" element={<div className="empty-state"><h1>Page not found</h1><p>The research may have moved.</p><button className="btn btn-primary" onClick={()=>navigate('/library')}>Browse the library</button></div>}/>
   </Routes></div>
   <footer className="site-footer"><span>ChristEye Anaiero · WDS research property · Full technical source linked and versioned</span><span>Digital prototypes and scientific hypotheses are separately labeled.</span></footer>
  </div>
  {search&&<SearchModal manifest={manifest} close={()=>setSearch(false)}/>}
 </div>;
}
export default function App(){
 const {data,error}=useData<Manifest>('manifest.json');
 if(error)return <div className="boot-error"><Shield size={36}/><h1>The source archive could not load</h1><p>{error}</p><p>Check the static asset base path and build-time content pipeline.</p></div>;
 if(!data)return <div className="boot"><div className="boot-symbol"><Flower2 size={42}/></div><div>Loading ChristEye Anaiero</div><span>Connecting the research atlas…</span></div>;
 return <Shell manifest={data}/>;
}
