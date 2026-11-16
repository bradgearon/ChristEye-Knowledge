import { useState } from 'react';
import { Link } from 'react-router-dom';
import { Activity, ArrowUpRight, BrainCircuit, Cpu, Dna, FlaskConical, HeartHandshake, Leaf, Network, ShieldCheck, Sparkles } from 'lucide-react';

type NodeId='lotus'|'wind'|'soil'|'root'|'crest'|'chemibus'|'sustibus'|'crystal'|'limits';
type Lens='system'|'living'|'digital';
type SystemNode={id:NodeId;label:string;sub:string;kind:'digital'|'living'|'research'|'safety';description:string;source:string;x:number;y:number};
const nodes:SystemNode[]=[
 {id:'lotus',label:'LOTUS',sub:'Coordination',kind:'digital',x:477,y:90,
  description:'A host scheduler that routes education and validates actions. The biological Lotus is a separate proposed role and requires evidence.',source:'specifications/v3/design/CE-A12_lotus_coordinator_Design.md'},
 {id:'wind',label:'WIND BUS',sub:'Typed information',kind:'digital',x:477,y:226,
  description:'A typed, provenance-aware software event bus. It carries curriculum, measurements and receipts—not nourishment or physical stimulation.',source:'processes/BIO-07-joining.md'},
 {id:'soil',label:'SOIL',sub:'Living organoid',kind:'research',x:185,y:374,
  description:'The complete proposed organoid: neurons, glia, dendrites, axons and local extracellular structures. This is not a demonstrated integrated Anaiero computer.',source:'processes/BIO-03-soil-organoid.md'},
 {id:'root',label:'ROOTS',sub:'Spinal + functional',kind:'research',x:477,y:408,
  description:'Separate the proposed spinal-derived Root tissue from a functional Root: a tested, delayed learned relation. One does not prove the other.',source:'processes/BIO-02-axon-spinal-root.md'},
 {id:'crest',label:'CREST',sub:'Source and lineage',kind:'research',x:185,y:228,
  description:'Cell-source records and production qualification. A lineage ID can be shared by many descendants; unique observed cell IDs are different.',source:'processes/BIO-01-crest-lineage.md'},
 {id:'chemibus',label:'CHEMI BUS',sub:'Molecular observations',kind:'research',x:774,y:228,
  description:'Assay-qualified metabolite, RNA/protein and candidate molecular measurements. These data do not by themselves identify a stored semantic memory.',source:'processes/BIO-04-sustibus-chemibus.md'},
 {id:'sustibus',label:'SUSTI BUS',sub:'Care and support',kind:'living',x:774,y:374,
  description:'Read-only reports of approved culture support and source-qualified care. Essential care stays independent of training success.',source:'processes/BIO-10-care-health.md'},
 {id:'crystal',label:'CRYSTAL',sub:'Digital replay',kind:'digital',x:477,y:537,
  description:'A digitally reproducible experience package and curriculum. Physical transferable memory Crystals are an unproven, separately gated research hypothesis.',source:'processes/BIO-08-crystals.md'},
 {id:'limits',label:'LIMITS',sub:'Independent veto',kind:'safety',x:774,y:90,
  description:'The independent limit controller and qualified human review veto physical actions that Lotus and StarChips must not authorize.',source:'specifications/v3/design/CE-A22_independent_limits_and_governance_Design.md'}
];
const links:[NodeId,NodeId][]=[
 ['lotus','wind'],['lotus','limits'],['wind','root'],['wind','crest'],['wind','chemibus'],['crest','soil'],
 ['soil','root'],['chemibus','sustibus'],['sustibus','soil'],['root','crystal'],['sustibus','limits'],['wind','crystal']
];
const kinds:Record<SystemNode['kind'],string>={digital:'Software reference',living:'Physiological support',research:'Living research',safety:'Independent protection'};
const iconFor=(id:NodeId)=>id==='lotus'?BrainCircuit:id==='wind'?Network:id==='soil'?Leaf:id==='root'?Activity:id==='crest'?Dna:id==='chemibus'?FlaskConical:id==='sustibus'?HeartHandshake:id==='limits'?ShieldCheck:Sparkles;
export default function Atlas({compact=false}:{compact?:boolean}){
 const [active,setActive]=useState<NodeId>('lotus');
 const [lens,setLens]=useState<Lens>('system');
 const selected=nodes.find(n=>n.id===active)!;
 const shown=(n:SystemNode)=>lens==='system'||(lens==='digital'?(n.kind==='digital'||n.kind==='safety'):(n.kind==='living'||n.kind==='research'||n.kind==='safety'));
 return <section className={'atlas-wrap '+(compact?'atlas-compact':'')} aria-label="Interactive architecture atlas">
  <div className="atlas-toolbar">
   <div className="atlas-caption"><span className="eyebrow mini">A SYSTEM OF RELATIONSHIPS</span><span className="atlas-caption-sub">Select a component to trace its purpose, boundaries and source.</span></div>
   <div className="segmented" role="group" aria-label="Choose architecture lens">
    {(['system','digital','living'] as const).map(v=><button className={lens===v?'selected':''} onClick={()=>setLens(v)} type="button" key={v}>{v==='system'?'All layers':v==='digital'?'Software':'Living research'}</button>)}
   </div>
  </div>
  <div className="atlas-body">
   <div className="atlas-map">
    <svg className="atlas-svg" viewBox="0 0 970 620" role="img" aria-label="System components connected through coordination and material/measurement buses">
     <defs>
      <radialGradient id="atlas-pool"><stop offset="0" stopColor="#173e48" stopOpacity=".70"/><stop offset="1" stopColor="#081723" stopOpacity="0"/></radialGradient>
      <filter id="atlas-soft"><feGaussianBlur stdDeviation="7"/></filter>
     </defs>
     <circle cx="477" cy="295" r="340" fill="url(#atlas-pool)"/>
     <g opacity=".20">{Array.from({length:14},(_,i)=><path key={i} d={'M0 '+(42*i)+'H970'} stroke="#36616f" strokeWidth=".6"/>)}</g>
     {links.map(([a,b],i)=>{const aa=nodes.find(n=>n.id===a)!, bb=nodes.find(n=>n.id===b)!;
      const dim=!(shown(aa)&&shown(bb));
      return <path key={i} d={'M'+aa.x+' '+aa.y+' L'+bb.x+' '+bb.y} className={'atlas-connection '+(dim?'dim':'')} strokeWidth={active===a||active===b?2.8:1.35}/>;
     })}
     {nodes.map(n=>{
       const Icon=iconFor(n.id);
       return <g key={n.id} className={'atlas-node '+(active===n.id?'current ':'')+(shown(n)?'':'muted')} role="button" tabIndex={0} aria-label={'Inspect '+n.label}
        onClick={()=>setActive(n.id)} onKeyDown={e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();setActive(n.id)}}}>
        <circle cx={n.x} cy={n.y} r={active===n.id?60:53} className={'atlas-node-ring kind-'+n.kind}/>
        <circle cx={n.x} cy={n.y} r="44" className="atlas-node-bg"/>
        <foreignObject x={n.x-15} y={n.y-30} width="30" height="30"><div className="node-foreign"><Icon size={20} strokeWidth={1.7}/></div></foreignObject>
        <text x={n.x} y={n.y+9} textAnchor="middle" className="node-main">{n.label}</text>
        <text x={n.x} y={n.y+25} textAnchor="middle" className="node-sub">{n.sub}</text>
       </g>;
     })}
    </svg>
    <div className="atlas-legend"><span><i className="legend-dot teal"/> Software</span><span><i className="legend-dot gold"/> Living / care</span><span><i className="legend-dot violet"/> Research hypothesis</span></div>
   </div>
   <div className="atlas-info">
    <div className="info-overline">SELECTED SYSTEM · {selected.id.toUpperCase()}</div>
    <div className="info-icon"><Cpu size={19}/></div>
    <h3>{selected.label.toLowerCase().replace(/^./,a=>a.toUpperCase())}</h3>
    <div className="info-status">{kinds[selected.kind]}</div>
    <p>{selected.description}</p>
    <Link to={'/read-source?path='+encodeURIComponent(selected.source)} className="quiet-link">Read source specification <ArrowUpRight size={15}/></Link>
    {!compact&&<div className="info-note">Diagram is a conceptual system model, not a live neural measurement.</div>}
   </div>
  </div>
 </section>
}
