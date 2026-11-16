import { useState } from 'react';
import { Link } from 'react-router-dom';
import { ArrowUpRight, Check, CircleHelp } from 'lucide-react';
type Node = {id:string;name:string;subtitle:string;desc:string;status:string;link:string;type:'digital'|'research'|'safety'};
const nodes:Node[]=[
 {id:'lotus',name:'Lotus',subtitle:'Coordination',desc:'Schedules permitted digital learning actions, evaluates available routes, and records why a decision was made.',status:'Digital architecture',link:'/docs/ce-a12-design',type:'digital'},
 {id:'wind',name:'WindBus',subtitle:'Typed communication',desc:'Connects specialized modules with permissioned, versioned events, time boundaries, and delivery receipts.',status:'Software reference',link:'/docs/ce-a11-design',type:'digital'},
 {id:'roots',name:'Roots',subtitle:'Retained capability',desc:'A functional Root is an independently tested durable relationship. A spinal-derived physical Root is a separate neuroscience hypothesis.',status:'Measured digitally / biological research',link:'/docs/ce-a16-design',type:'research'},
 {id:'soil',name:'Soil',subtitle:'Organoid research',desc:'Soil denotes the entire proposed living organoid, including neuronal and glial cells, axons, matrix and developmental history.',status:'Biological research design',link:'/docs/ce-a15-design',type:'research'},
 {id:'crystal',name:'Crystals',subtitle:'Verified learning packs',desc:'Digital Crystals hold consented source-linked lessons and replay instructions. Transferable physical Crystals remain unproven.',status:'Digital example / physical hypothesis',link:'/docs/ce-a14-design',type:'digital'},
 {id:'guard',name:'Limits',subtitle:'Independent safety',desc:'The authority to protect physical systems remains separate from Lotus, training feedback, and other adaptive software.',status:'Hardware release gated',link:'/docs/ce-a22-design',type:'safety'}
];
export default function Atlas({compact=false}:{compact?:boolean}){
 const [selected,setSelected]=useState('lotus');
 const node=nodes.find(x=>x.id===selected)!;
 return <div className={'atlas'+(compact?' atlas-compact':'')}>
  <div className="atlas-topline"><span><span className="live-orbit"/> ANAIERO / LAYERED ARCHITECTURE</span><span>INTERACTIVE SYSTEM MAP</span></div>
  <div className="atlas-figure">
   <svg viewBox="0 0 760 430" className="atlas-lines" role="img" aria-label="Conceptual network connecting Lotus, WindBus, Roots, Soil, Crystals and Limits">
    <defs><radialGradient id="a-glow"><stop stopColor="#5fe9d7" stopOpacity=".15"/><stop offset="1" stopColor="#5fe9d7" stopOpacity="0"/></radialGradient></defs>
    <circle cx="380" cy="204" r="186" fill="url(#a-glow)"/>
    {[70,124,185].map(r=><circle key={r} cx="380" cy="204" r={r} fill="none" stroke="#5b7895" strokeOpacity=".17"/>)}
    <g strokeWidth="1.5" strokeLinecap="round" fill="none" opacity=".8">
     <path d="M380 204L185 95M380 204L580 100M380 204L150 283M380 204L608 300M380 204L390 386" stroke="#5dddc7" strokeDasharray="4 8"/>
     <path d="M185 95L150 283L390 386L608 300L580 100" stroke="#748da4" opacity=".5"/>
     <path d="M380 204L390 386" stroke="#e4b67c" strokeWidth="2"/>
    </g>
    {[['380','204','17'],['185','95','6'],['580','100','6'],['150','283','6'],['608','300','6'],['390','386','7']].map(([x,y,r],i)=>
      <circle key={i} cx={x} cy={y} r={r} fill={i===0?'#82ebd6':'#1a384c'} stroke={i===5?'#e4b67c':'#81dcd0'} strokeWidth="2"/>)}
   </svg>
   <button className={'atlas-node node-lotus'+(selected==='lotus'?' selected':'')} onClick={()=>setSelected('lotus')} type="button"><span className="node-sigil">◎</span><strong>LOTUS</strong><small>coordination</small></button>
   <button className={'atlas-node node-wind'+(selected==='wind'?' selected':'')} onClick={()=>setSelected('wind')} type="button"><strong>WINDBUS</strong><small>communication</small></button>
   <button className={'atlas-node node-crystal'+(selected==='crystal'?' selected':'')} onClick={()=>setSelected('crystal')} type="button"><strong>CRYSTAL</strong><small>digital learning</small></button>
   <button className={'atlas-node node-roots'+(selected==='roots'?' selected':'')} onClick={()=>setSelected('roots')} type="button"><strong>ROOTS</strong><small>retention</small></button>
   <button className={'atlas-node node-soil'+(selected==='soil'?' selected':'')} onClick={()=>setSelected('soil')} type="button"><strong>SOIL</strong><small>organoid research</small></button>
   <button className={'atlas-node node-guard'+(selected==='guard'?' selected':'')} onClick={()=>setSelected('guard')} type="button"><strong>LIMITS</strong><small>independent safety</small></button>
  </div>
  <div className="atlas-inspector"><div className="inspector-meta"><span className={'inspector-marker '+node.type}/>{node.status}</div>
   <div className="inspector-body"><div><h3>{node.name} <span>— {node.subtitle}</span></h3><p>{node.desc}</p></div>
   <Link className="inspector-action" to={node.link} title={'Read '+node.name+' design'}><ArrowUpRight size={18}/></Link></div>
  </div>
  <div className="atlas-legend"><span><i className="legend-teal"/><Check size={12}/> Digital design and software</span><span><i className="legend-gold"/><CircleHelp size={12}/> Biological research, not demonstrated</span></div>
 </div>;
}
