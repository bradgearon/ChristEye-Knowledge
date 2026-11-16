import { useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { Activity, ArrowRight, ArrowUpRight, BookOpen, Box, Cpu, Database, Gauge, Headphones, Network, ShieldCheck, Sparkles, Waves } from 'lucide-react';
import { catalog } from '../lib/content';
const sections=[
 {id:'all',name:'All StarChips'},
 {id:'world',name:'World & game design'},
 {id:'music',name:'Music & audio'},
 {id:'research',name:'Science & learning'},
 {id:'tools',name:'Software & knowledge'}
];
const classification=(code?:string)=>{
 const n=Number(code?.match(/\d+$/)?.[0]||'0');
 return n<=6?'world':n<=9?'music':n<=12?'research':'tools';
};
export function LotusDemo(){
 const [action,setAction]=useState('train_digital');
 const [gain,setGain]=useState(.72);
 const [cost,setCost]=useState(.24);
 const [delta,setDelta]=useState(-.02);
 const outcome = action==='dispense_medium'?'VETO':delta<-.05?'HOLD':gain-cost>0?'STAGE_DIGITAL':'UNRESOLVED';
 const reasons:Record<string,string>={
  VETO:'An unqualified physical action is never in the digital action allowlist.',
  HOLD:'This candidate violates the declared protection margin for an existing Root.',
  STAGE_DIGITAL:'A permitted digital action has positive predicted benefit after stated cost and protection checks.',
  UNRESOLVED:'The available estimate does not establish a positive net benefit.'
 };
 return <div className="demo-panel"><div className="demo-head"><div><span className="mini-kicker">DIGITAL REFERENCE / LOTUS</span><h3>Try a developmental decision</h3><p>Change the inputs to see a bounded software decision. This mirrors the published Lotus decision semantics; it does not control tissue.</p></div><Cpu size={25}/></div>
  <div className="demo-controls"><label>Proposed action<select value={action} onChange={e=>setAction(e.target.value)}><option value="train_digital">Train digital module</option><option value="replay_digital">Replay digital Crystal</option><option value="dispense_medium">Change physical medium (forbidden)</option></select></label>
   <label>Expected digital learning benefit <strong>{gain.toFixed(2)}</strong><input type="range" min="0" max="1" step=".01" value={gain} onChange={e=>setGain(Number(e.target.value))}/></label>
   <label>Computational or opportunity cost <strong>{cost.toFixed(2)}</strong><input type="range" min="0" max="1" step=".01" value={cost} onChange={e=>setCost(Number(e.target.value))}/></label>
   <label>Protected old-Root change <strong>{delta.toFixed(2)}</strong><input type="range" min="-.3" max=".2" step=".01" value={delta} onChange={e=>setDelta(Number(e.target.value))}/></label>
  </div>
  <div className={'demo-outcome '+(outcome==='VETO'?'veto':outcome==='HOLD'?'hold':'')}><small>LOTUS DECISION</small><strong>{outcome.replaceAll('_',' ')}</strong><p>{reasons[outcome]}</p></div>
  <div className="demo-footer"><span><ShieldCheck size={14}/> Browser-only illustrative computation</span><a href="https://github.com/bradgearon/ChristEye-Knowledge/blob/main/anaiero/lotus.py" target="_blank" rel="noreferrer">Open source algorithm <ArrowUpRight size={14}/></a></div>
 </div>;
}
function WindBusDemo(){
 const [mode,setMode]=useState('valid');
 const output:Record<string,{state:string,reason:string,sequence:number}> = {
  valid:{state:'DELIVERED',reason:'Message type and recipient are authorized; no repeated event ID.',sequence:17},
  expired:{state:'REJECTED',reason:'The timestamp exceeds the permitted delivery interval.',sequence:18},
  unauthorized:{state:'REJECTED',reason:'The sender has no route permission for this recipient.',sequence:19}
 };
 const v=output[mode];
 return <div className="demo-panel"><div className="demo-head"><div><span className="mini-kicker">SOFTWARE SIMULATION / WINDBUS</span><h3>Inspect a routed Wind</h3><p>A typed event can be delivered, rejected as stale or rejected because the source is unauthorized.</p></div><Network size={25}/></div>
   <div className="segmented-sim">{[['valid','Authorized'],['expired','Expired'],['unauthorized','Unauthorized']].map(([key,title])=><button type="button" key={key} className={mode===key?'on':''} onClick={()=>setMode(key)}>{title}</button>)}</div>
   <div className="example-code"><div><span>kind</span><strong>observation</strong></div><div><span>source</span><strong>{mode==='unauthorized'?'public-client':'observer'}</strong></div><div><span>recipient</span><strong>lotus</strong></div><div><span>sequence</span><strong>{v.sequence}</strong></div><div><span>status</span><strong className={v.state==='DELIVERED'?'text-mint':'text-gold'}>{v.state}</strong></div></div>
   <p className="simulation-explanation">{v.reason}</p><div className="demo-footer"><span><ShieldCheck size={14}/> Illustrative packet, not live neural activity</span><a href="https://github.com/bradgearon/ChristEye-Knowledge/blob/main/anaiero/bus.py" target="_blank" rel="noreferrer">Reference code <ArrowUpRight size={14}/></a></div>
 </div>;
}
function BandwidthDemo(){
 const [channels,setChannels]=useState(1020),[samples,setSamples]=useState(20000),[bytes,setBytes]=useState(2);
 const perSecond=channels*samples*bytes/1e6,perDay=perSecond*86400/1000;
 return <div className="demo-panel"><div className="demo-head"><div><span className="mini-kicker">ENGINEERING / ILLUSTRATIVE BANDWIDTH</span><h3>Neural acquisition budget</h3><p>Estimate raw stream throughput from hypothetical channel count, sample rate and bytes per sample.</p></div><Gauge size={25}/></div>
   <div className="demo-controls"><label>Selected recording channels <strong>{channels.toLocaleString()}</strong><input type="range" min="32" max="2048" step="16" value={channels} onChange={e=>setChannels(Number(e.target.value))}/></label>
    <label>Samples per channel per second <strong>{samples.toLocaleString()}</strong><input type="range" min="1000" max="30000" step="1000" value={samples} onChange={e=>setSamples(Number(e.target.value))}/></label>
    <label>Bytes per sample <select value={bytes} onChange={e=>setBytes(Number(e.target.value))}><option value="2">2 bytes (16-bit)</option><option value="4">4 bytes (32-bit)</option></select></label></div>
   <div className="budget-results"><div><small>RAW THROUGHPUT</small><strong>{perSecond.toFixed(2)} MB/s</strong></div><div><small>UNCOMPRESSED 24 HOURS</small><strong>{perDay.toFixed(2)} GB</strong></div></div>
   <div className="demo-footer"><span><ShieldCheck size={14}/> Arithmetic only; excludes metadata, compression and device overhead</span><Link to="/docs/guide-hardware">Hardware plan <ArrowRight size={13}/></Link></div>
  </div>;
}
export default function Studio(){
 const [active,setActive]=useState('all');
 const [demo,setDemo]=useState('lotus');
 const chips=catalog.documents.filter(d=>d.kind==='chip'&&d.code?.startsWith('ce-d'));
 const visible=useMemo(()=>active==='all'?chips:chips.filter(c=>classification(c.code)===active),[active,chips]);
 return <div className="page-wrap">
  <div className="container page-hero"><span className="section-kicker">STARCHIP STUDIO / THE CREATIVE COMPUTATION FRONTIER</span><h1>Specialize. Connect.<br/><em>Create what comes next.</em></h1><p>Explore proposed StarChip disciplines, their input and output contracts, and bounded digital demonstrations from the published software architecture. Proposed chip capabilities are labeled as designs, not existing trained biological systems.</p></div>
  <section className="section studio-labs"><div className="container"><div className="heading-row"><div><span className="section-kicker">INTERACTIVE / SOFTWARE LAB</span><h2>Inspect the <em>moving parts.</em></h2></div><span className="site-status"><span className="status-dot"/> Browser-side digital simulations</span></div>
   <div className="lab-tabs">{[['lotus','Lotus decisions'],['windbus','WindBus traffic'],['power','Power & bandwidth']].map(([id,title])=><button type="button" key={id} onClick={()=>setDemo(id)} className={demo===id?'active':''}>{title}</button>)}</div>
   {demo==='lotus'?<LotusDemo/>:demo==='windbus'?<WindBusDemo/>:<BandwidthDemo/>}
  </div></section>
  <section className="section"><div className="container"><div className="heading-row"><div><span className="section-kicker">THE SPECIALIST LIBRARY</span><h2>Discover the <em>StarChips.</em></h2><p>The proposal manifests below define capabilities and integration points. Full world-building, AI music and researcher outputs require additional implementation.</p></div></div>
   <div className="chip-tabs">{sections.map(s=><button type="button" key={s.id} className={active===s.id?'active':''} onClick={()=>setActive(s.id)}>{s.name}</button>)}</div>
   <div className="chip-grid">{visible.map((c,i)=><Link to={c.href} className="chip-card" key={c.id}><div className="chip-symbol">{classification(c.code)==='music'?<Headphones/>:classification(c.code)==='world'?<Sparkles/>:classification(c.code)==='research'?<BookOpen/>:<Box/>}</div><span className="chip-code">{c.code?.toUpperCase()} · PROPOSED</span><h3>{c.title}</h3><p>Explore this specialist's architecture, expected interfaces, inputs and future output capabilities.</p><span className="chip-action">Inspect specification <ArrowUpRight size={16}/></span></Link>)}</div>
  </div></section>
  <section className="section"><div className="container statement-callout"><div><span className="section-kicker">FROM SPECIFICATION TO TESTED OUTPUT</span><h2>Designs become capabilities<br/><em>through evidence.</em></h2><p>The next studio stages will produce reproducible, rights-cleared demonstrations for procedural Metroidvania maps, original MIDI/score structures, and source-based research hypotheses.</p></div><Link className="button button-outline" to="/docs">Read the technical plans <ArrowRight size={16}/></Link></div></section>
 </div>;
}
