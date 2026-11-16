import { Link } from 'react-router-dom';
import { ArrowRight, ArrowUpRight, Atom, BookOpen, FlaskConical, Microscope, ShieldCheck } from 'lucide-react';
import { catalog } from '../lib/content';
const evidence=[
 {class:'F',title:'Published finding',description:'A result measured in a named external study and preparation. It does not automatically transfer to Anaiero.'},
 {class:'X',title:'Scientific synthesis',description:'An interpretation across results, with assumptions and conflicting evidence stated.'},
 {class:'H',title:'Hypothesis',description:'A testable proposed mechanism; predictions and potential falsifiers must be explicit.'},
 {class:'D',title:'Engineering design',description:'A specified architecture, data contract or method that may or may not be implemented.'},
 {class:'A',title:'Analogy & philosophy',description:'An interpretive framework such as Word, Milk, Root or Spirit, mapped carefully to ordinary entities.'}
];
export default function Science(){
 const research=catalog.documents.filter(d=>d.kind==='research'||d.kind==='evidence').slice(0,9);
 return <div className="page-wrap">
  <div className="container page-hero"><span className="section-kicker">SCIENCE / UNDERSTANDING THROUGH EVIDENCE</span><h1>Questions worth asking.<br/><em>Evidence worth following.</em></h1><p>A source-linked research atlas spanning neural and glial networks, molecular processes, bioelectronics, time-dependent learning, and the boundaries between established findings and new hypotheses.</p></div>
  <section className="section scientific-lenses"><div className="container"><span className="section-kicker">HOW TO READ OUR CLAIMS</span><h2>Science has a <em>grammar of certainty.</em></h2><div className="evidence-classes">{evidence.map(c=><div className="evidence-class" key={c.class}><span className="evidence-letter">{c.class}</span><div><h3>{c.title}</h3><p>{c.description}</p></div></div>)}</div></div></section>
  <section className="section"><div className="container"><div className="heading-row"><div><span className="section-kicker">THE RESEARCH ATLAS</span><h2>Molecules, structure<br/><em>and learning.</em></h2></div><Link className="text-link" to="/docs/process-molecular-index">Molecular index <ArrowRight size={16}/></Link></div>
   <div className="science-topic-grid">
    <Link to="/docs/bio-02-axon-spinal-root" className="topic-card"><Microscope size={27}/><span>01 / STRUCTURE</span><h3>Axons, dendrites & synapses</h3><p>What is a physical connection, what is a statistical relationship, and what could count as a functional Root?</p><ArrowUpRight size={18}/></Link>
    <Link to="/docs/bio-04-sustibus-chemibus" className="topic-card"><Atom size={27}/><span>02 / CHEMISTRY</span><h3>Metabolism & molecular signals</h3><p>Glucose, lactate, receptor state and candidate plasticity pathways, with measured support separate from feedback.</p><ArrowUpRight size={18}/></Link>
    <Link to="/docs/bio-06-teaching" className="topic-card"><FlaskConical size={27}/><span>03 / EXPERIMENTS</span><h3>Training and retained understanding</h3><p>Source identity, baseline, independent evaluator, delayed tests and alternative interpretations.</p><ArrowUpRight size={18}/></Link>
   </div>
  </div></section>
  <section className="section soft-section"><div className="container"><div className="heading-row"><div><span className="section-kicker">OPEN LIBRARY / SCIENTIFIC SOURCES</span><h2>Go all the way<br/><em>to the original work.</em></h2></div><Link className="text-link" to="/docs">Browse full corpus <ArrowRight size={16}/></Link></div>
   <div className="paper-list">{research.map((d,i)=><Link to={d.href} className="paper-entry" key={d.id}><span className="paper-index">{String(i+1).padStart(2,'0')}</span><div><small>{d.group} / {d.status}</small><h3>{d.title}</h3><p>{d.excerpt.slice(0,175)}…</p></div><ArrowUpRight size={20}/></Link>)}</div>
  </div></section>
  <section className="section"><div className="container science-note"><ShieldCheck size={25}/><div><h3>Our scientific boundary matters.</h3><p>A biological model that produces electrical activity is not automatically a trained mind. A molecule associated with memory in a published animal experiment is not a portable software upgrade. Anaiero documents what would need to be measured before making either claim.</p></div><Link to="/docs/guide-testing">Testing principles <ArrowRight size={15}/></Link></div></section>
 </div>;
}
