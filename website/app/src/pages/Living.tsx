import { useState } from 'react';
import { Link } from 'react-router-dom';
import { ArrowRight, Heart, Leaf, Sprout, Activity, FlaskConical, GitBranch, ShieldCheck, Waves, Network, CheckCircle2 } from 'lucide-react';
const steps=[
 {id:'crest',n:'01',title:'Crest & lineage',Icon:GitBranch,desc:'Source identity, differentiation history and cell tracking are qualified by the cell-production scientist. Clone lineage and individual cell observations must not be confused.',doc:'/docs/bio-01-crest-lineage'},
 {id:'soil',n:'02',title:'The living Soil',Icon:Sprout,desc:'Soil is the organoid as a whole: its cells, extracellular structures, regional heterogeneity, axons, glia and actual developmental observations.',doc:'/docs/bio-03-soil-organoid'},
 {id:'roots',n:'03',title:'Axons & spinal Roots',Icon:Network,desc:'Spinal-derived Root tissue is a proposed physical subsystem. Functional Roots are separately tested retained relationships, never assigned by an image alone.',doc:'/docs/bio-02-axon-spinal-root'},
 {id:'care',n:'04',title:'Sustaining the system',Icon:Heart,desc:'Milk, Water and qualified environmental support continue regardless of task success. ChemiBus observations do not authorize chemical or peptide changes.',doc:'/docs/bio-04-sustibus-chemibus'},
 {id:'learn',n:'05',title:'Learning & evaluation',Icon:Activity,desc:'Training needs reproducible inputs, actual delivery receipts, independent evaluation and delayed test outcomes under a fixed reader.',doc:'/docs/bio-06-teaching'},
 {id:'grow',n:'06',title:'Growth & review',Icon:Leaf,desc:'New cells, axonal extension, digital capacity and meaningful acquired function are different types of growth, each with a different evidence gate.',doc:'/docs/bio-09-growth-control'}
];
export default function Living(){
 const [active,setActive]=useState('soil');
 const selected=steps.find(s=>s.id===active)!;
 return <div className="page-wrap">
  <div className="container page-hero"><span className="section-kicker">LIVING SYSTEMS / AN EVIDENCE-GATED FRONTIER</span><h1>Growth is a process.<br/><em>Care is a principle.</em></h1><p>Follow the proposed living substrate from qualified cell sources through organoid development, axonal structure, neural learning and independent evaluation—while preserving physiological support.</p></div>
  <div className="container living-split"><div className="living-timeline"><div className="section-kicker">THE DEVELOPMENTAL LIFECYCLE</div>{steps.map(({id,n,title,Icon})=><button type="button" onClick={()=>setActive(id)} key={id} className={'timeline-step'+(active===id?' active':'')}><span className="step-number">{n}</span><Icon size={18}/><span>{title}</span><ArrowRight size={15}/></button>)}</div>
   <div className="living-detail"><span className="detail-eyebrow">STEP {selected.n} / SYSTEM DEVELOPMENT</span><div className="living-illustration"><selected.Icon size={76} strokeWidth={1}/><span className="orbit orbit-one"/><span className="orbit orbit-two"/><i className="orbit-node orbit-a"/><i className="orbit-node orbit-b"/></div><h2>{selected.title}</h2><p>{selected.desc}</p><Link className="button button-outline" to={selected.doc}>Open scientific process <ArrowRight size={16}/></Link><div className="detail-foot"><ShieldCheck size={15}/> Proposed biological pathways require qualified laboratory and scientific review.</div></div>
  </div>
  <section className="section living-values"><div className="container"><div className="heading-row"><div><span className="section-kicker">CARE · LOVE · POWER</span><h2>Stewardship belongs<br/><em>inside the architecture.</em></h2></div><p>These terms have human and engineering meaning. They do not establish that biological tissue experiences human emotion or generates energy on its own.</p></div>
  <div className="value-grid"><div className="value-card"><Heart size={27}/><h3>Care</h3><p>Reliable physiological maintenance and laboratory oversight, independent of reward or performance. Living systems must never be deprived for making a wrong prediction.</p><Link to="/docs/bio-10-care-health">Care specification <ArrowRight size={14}/></Link></div>
   <div className="value-card"><Leaf size={27}/><h3>Love</h3><p>Human commitment to dignity, consent, transparent communication and thoughtful support. This is a research and design ethic, not a measurement of a culture's feelings.</p><Link to="/about">Our guiding values <ArrowRight size={14}/></Link></div>
   <div className="value-card"><Waves size={27}/><h3>Power</h3><p>Meaningful computational capacity requires measurable power, bandwidth, energy and physiological support—not an unverified source of biological free energy.</p><Link to="/studio">Explore the systems lab <ArrowRight size={14}/></Link></div>
  </div></div></section>
  <section className="section"><div className="container callout-strip"><div><span className="section-kicker">SCIENTIST WORKFLOW</span><h2>Twelve stages. Independent evidence. Clear decisions.</h2><p>Every growth, care and research proposal has its own owner, qualification step, controls and hold criteria.</p></div><Link className="button button-primary" to="/docs/process-scientist-workflow">Read the scientific workflow <ArrowRight size={16}/></Link></div></section>
 </div>;
}
