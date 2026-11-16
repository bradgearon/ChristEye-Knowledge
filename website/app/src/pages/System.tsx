import { Link } from 'react-router-dom';
import { ArrowRight, BrainCircuit, Workflow, Layers3, Database, ShieldCheck } from 'lucide-react';
import Atlas from '../components/Atlas';
const layers=[
 {n:'01',title:'Experience & sources',desc:'The user, application or experiment introduces a permissioned observation or lesson.',Icon:Database},
 {n:'02',title:'WindBus & coordination',desc:'Typed messages connect specialists; Lotus selects allowed digital actions and records the reasoning.',Icon:Workflow},
 {n:'03',title:'Roots & memory',desc:'Independent assessment separates immediate access, delayed retention and tested functional capability.',Icon:BrainCircuit},
 {n:'04',title:'Qualification & care',desc:'Safety and qualified biological care exist outside the adaptive learning reward pathway.',Icon:ShieldCheck}
];
export default function System(){return <div className="page-wrap">
 <div className="container page-hero"><span className="section-kicker">EXPLORE / SYSTEM ARCHITECTURE</span><h1>An anatomy of<br/><em>connected intelligence.</em></h1><p>A modular architecture with explicit responsibilities, boundaries and evidence. Select a component to follow its sources into the full research library.</p></div>
 <div className="container wide-atlas"><Atlas/></div>
 <section className="section"><div className="container"><span className="section-kicker">HOW A SYSTEM LEARNS</span><h2>From source to <em>demonstrated capability.</em></h2><div className="steps-grid">{layers.map(({n,title,desc,Icon})=><div className="step-card" key={n}><div className="step-num">{n}</div><Icon size={25}/><h3>{title}</h3><p>{desc}</p></div>)}</div></div></section>
 <section className="section soft-section"><div className="container split-section"><div><span className="section-kicker">A CRITICAL DISTINCTION</span><h2>Two forms<br/><em>of memory.</em></h2></div><div className="split-panels">
  <article><span className="tile-tag digital-tag">DIGITAL — IMPLEMENTABLE</span><h3>Content-addressed knowledge</h3><p>Source-hashed files, retrieval indexes and versioned digital Crystals can be stored, cloned and replayed through software-defined interfaces.</p><Link to="/docs/ce-a02-design">Long-term storage design <ArrowRight size={15}/></Link></article>
  <article><span className="tile-tag hypothesis-tag">BIOLOGICAL — RESEARCH</span><h3>Living retention and Roots</h3><p>Spinal-derived circuits, axons and synaptic/molecular candidates require their own preparation-specific observations and independent learning tests.</p><Link to="/docs/bio-02-axon-spinal-root">Axon and spinal Root science <ArrowRight size={15}/></Link></article>
 </div></div></section>
 <section className="section"><div className="container centered-cta"><Layers3 size={33}/><h2>Every component has a technical home.</h2><p>Read 24 current engineering designs, 24 scientific companions, the process library and typed schemas in one linked technical library.</p><Link className="button button-primary" to="/docs">Explore technical documents <ArrowRight size={17}/></Link></div></section>
 </div>;}
