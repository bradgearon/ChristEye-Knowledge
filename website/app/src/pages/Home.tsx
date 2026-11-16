import { Link } from 'react-router-dom';
import { ArrowRight, ArrowUpRight, Atom, BookOpen, BrainCircuit, Code2, Cpu, FlaskConical, Heart, Layers3, MoveUpRight, Network, PlayCircle, ShieldCheck, Sparkles, Sprout } from 'lucide-react';
import Atlas from '../components/Atlas';
import InterestForm from '../components/InterestForm';
import { catalog } from '../lib/content';
const entrances=[
 {title:'Explore the system',meta:'SYSTEM ARCHITECTURE',detail:'Lotus, WindBus, Roots, Crystals and the layers connecting them.',to:'/system',Icon:Network},
 {title:'Living systems',meta:'DEVELOPMENT & CARE',detail:'The Soil organoid, axons, Crest lineage, metabolism and meaningful growth.',to:'/living',Icon:Sprout},
 {title:'Scientific research',meta:'EVIDENCE & NEUROSCIENCE',detail:'Molecular pathways, experiment design, published findings and open hypotheses.',to:'/science',Icon:FlaskConical},
 {title:'Developer library',meta:'DESIGNS & SCHEMAS',detail:'The complete technical specifications, interfaces, code and scientific companions.',to:'/docs',Icon:Code2},
 {title:'StarChip Studio',meta:'APPLICATIONS & OUTPUT',detail:'World-building, music, knowledge, and digital learning demonstrations.',to:'/studio',Icon:Cpu},
 {title:'Scholarship',meta:'MEANING & THEORY',detail:'Knowledge, Understanding, Wisdom, Word, Wind, Care and the history of the ideas.',to:'/journal',Icon:BookOpen}
];
export default function Home(){
 return <>
  <section className="hero-section"><div className="container hero-grid">
   <div className="hero-copy">
    <div className="eyebrow"><span className="eyebrow-rule"/> CHRिSTEYE ANAIERO · A RESEARCH INITIATIVE</div>
    <h1>Intelligence<br/>has <span className="serif-accent">architecture.</span></h1>
    <p className="hero-lead">An ambitious, evidence-led exploration of computing systems that can learn, retain knowledge, connect specialist capabilities and evolve through well-defined processes.</p>
    <div className="hero-actions"><Link className="button button-primary" to="/system">Explore the architecture <ArrowRight size={18}/></Link><Link className="button button-outline" to="/studio">Visit StarChip Studio <PlayCircle size={17}/></Link></div>
    <div className="hero-verification"><ShieldCheck size={18}/><span>Working digital software. A distinct, carefully documented living-computing research program.</span></div>
   </div>
   <div className="hero-visual"><Atlas compact/></div>
  </div>
  <div className="container hero-bottomline"><span>BUILT ON SCIENTIFIC INQUIRY</span><span className="hero-bottom-center"><span className="signal-line"/>LEARNING · STRUCTURE · UNDERSTANDING</span><Link to="/about">WE DEVELOP SOFTWARE LLC <MoveUpRight size={15}/></Link></div>
  </section>
  <section className="number-band"><div className="container numbers-grid">
   <div><strong>24</strong><span>documented system modules</span></div><div><strong>12</strong><span>controlled scientific processes</span></div><div><strong>33</strong><span>specialist chip manifests</span></div><div><strong>{catalog.documents.length}+</strong><span>source-linked research entries</span></div>
  </div></section>
  <section className="section section-entrances"><div className="container">
   <div className="section-heading heading-row"><div><span className="section-kicker">01 / SIX POINTS OF ENTRY</span><h2>One system. <em>Many disciplines.</em></h2><p>Discover Anaiero from the perspective you know—and trace every idea back to its engineering or scientific source.</p></div></div>
   <div className="entrance-grid">{entrances.map(({title,meta,detail,to,Icon},i)=><Link to={to} className={'entrance-card entrance-'+i} key={to}><div className="entrance-head"><div className="entrance-icon"><Icon size={23}/></div><ArrowUpRight size={19}/></div><div><span className="card-overline">{meta}</span><h3>{title}</h3><p>{detail}</p></div></Link>)}</div>
  </div></section>
  <section className="section section-statement"><div className="container statement-grid">
   <div><span className="section-kicker">02 / THE SYSTEM</span><h2>Not a single machine.<br/><em>A growing ecosystem.</em></h2></div>
   <div className="statement-text"><p>At its core, Anaiero is a proposed architecture for specialized computational communities: each one capable of handling a discipline, communicating through defined channels, and being evaluated for useful learning.</p><p>The digital prototypes are real software. The proposed biological substrate is research with distinct requirements, measurements and independent care.</p><Link to="/system" className="text-link">Understand the full system <ArrowRight size={16}/></Link></div>
  </div>
  <div className="container story-grid">
   <div className="story-panel story-care"><Heart size={26}/><span className="card-overline">CARE</span><h3>The conditions for growth.</h3><p>In the biological research program, essential care stays independent of reward and task performance. Monitoring, safety, and scientific judgment are part of the architecture.</p><Link to="/living">The care framework <ArrowRight size={15}/></Link></div>
   <div className="story-panel story-understand"><BrainCircuit size={26}/><span className="card-overline">UNDERSTANDING</span><h3>Knowledge must prove useful.</h3><p>Roots are retained functional relationships, not just stored text. We distinguish remembered data, independently demonstrated capability and proposed living mechanisms.</p><Link to="/docs/ce-a16-design">Explore the Roots <ArrowRight size={15}/></Link></div>
   <div className="story-panel story-power"><Sparkles size={26}/><span className="card-overline">POTENTIAL</span><h3>Specialists that connect.</h3><p>Imagine modules for composing music, constructing worlds, reading a library or analyzing a scientific discipline—each with its own sources, skills and limits.</p><Link to="/studio">Browse StarChips <ArrowRight size={15}/></Link></div>
  </div></section>
  <section className="section showcase-section"><div className="container showcase-grid"><div className="showcase-left">
   <span className="section-kicker">03 / A PLATFORM FOR CREATORS & SCIENTISTS</span><h2>Ideas become<br/><em>systems you can test.</em></h2>
   <p>Follow a software event from a task through WindBus, into a versioned digital Crystal, and back out through a verified reader. Explore how a scientist would separately investigate neuronal circuits, axons and long-term retention.</p>
   <div className="showcase-links"><Link className="button button-primary" to="/studio">Open the digital lab <ArrowRight size={17}/></Link><Link className="text-link" to="/science">Read the science <ArrowRight size={15}/></Link></div>
   </div><div className="showcase-right"><div className="showcase-terminal"><div className="terminal-bar"><span className="terminal-dots"><i/><i/><i/></span><span>ANAIERO / DIGITAL REFERENCE</span><span className="terminal-state">DESIGN + CODE</span></div>
     <div className="terminal-content"><div className="term-row"><span>01</span><strong>SOURCE EVENT</strong><small>Consent-bound learning episode</small></div><div className="term-path"/><div className="term-row"><span>02</span><strong>WINDBUS</strong><small>Typed, attributable messages</small></div><div className="term-path"/><div className="term-row"><span>03</span><strong>DIGITAL CRYSTAL</strong><small>Versioned sources and replay</small></div><div className="term-path"/><div className="term-row"><span>04</span><strong>INDEPENDENT PROBE</strong><small>Retention and capability assessment</small></div></div>
     <div className="terminal-foot"><span className="status-dot"/> Software workflow illustration · No living data</div>
   </div></div></div></section>
  <section className="section section-research"><div className="container"><div className="heading-row"><div><span className="section-kicker">04 / SCIENTIFIC TRANSPARENCY</span><h2>Ambition meets <em>evidence.</em></h2></div><Link to="/science" className="text-link">Visit the research atlas <ArrowRight size={16}/></Link></div><div className="evidence-grid">
   <div className="evidence-tile"><span className="evidence-index">01 / IMPLEMENTED</span><Layers3 size={25}/><h3>Digital systems</h3><p>Reference software for messaging, data persistence, digital Crystal replay and synthetic supervisory records.</p><span className="tile-tag digital-tag">Software prototypes</span></div>
   <div className="evidence-tile"><span className="evidence-index">02 / RESEARCH</span><Atom size={25}/><h3>Living network science</h3><p>Organoid Soil, spinal Root candidates, synaptic structures, astrocytic metabolism, axons and neural I/O.</p><span className="tile-tag study-tag">Evidence-gated proposals</span></div>
   <div className="evidence-tile"><span className="evidence-index">03 / OPEN QUESTION</span><FlaskConical size={25}/><h3>Physical Crystals</h3><p>Investigating whether a bounded material state could help a compatible recipient reacquire a specific learned relation.</p><span className="tile-tag hypothesis-tag">Not demonstrated</span></div>
  </div></div></section>
  <section className="section newsletter-section"><div className="container newsletter-grid"><div><span className="section-kicker">FOLLOW THE DEVELOPMENT</span><h2>Be part of what<br/><em>comes next.</em></h2><p>Research papers, engineering milestones, new StarChip demonstrations, and thoughtful updates from We Develop Software LLC.</p></div><InterestForm compact/></div></section>
 </>;
}
