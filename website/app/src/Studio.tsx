import {useEffect,useMemo,useState} from 'react';
import {Link} from 'react-router-dom';
import {Activity,ArrowUpRight,BookOpen,BrainCircuit,Check,ChevronRight,Cpu,Database,FileText,FlaskConical,Gamepad2,HeartPulse,LockKeyhole,Music2,Play,RotateCcw,ShieldCheck,SlidersHorizontal,Split,Waveform} from 'lucide-react';
import type {Chip,Manifest} from './data';
import {readRoute,sourceLink,useData} from './data';

type LabId='lotus'|'wind'|'crystal'|'care'|'power'|'world'|'music';
type LabDef={id:LabId;label:string;description:string;kind:string;source:string};
const labs:LabDef[]=[
 {id:'lotus',label:'Lotus Decision Engine',description:'Balance learning gains with protected old skills and a hard physical veto.',kind:'DIGITAL SIMULATION',source:'anaiero/lotus.py'},
 {id:'wind',label:'WindBus Route Inspector',description:'Follow an authorized event, rejected recipient, expiration and duplicate message.',kind:'DIGITAL SIMULATION',source:'anaiero/bus.py'},
 {id:'crystal',label:'Digital Crystal Replay',description:'Package a consented episode by content hash and simulate verified retrieval.',kind:'BROWSER MODEL OF DIGITAL REFERENCE',source:'anaiero/crystal.py'},
 {id:'care',label:'SustiBus Supervisor',description:'Inspect synthetic environment telemetry and see how stale readings latch faults.',kind:'SYNTHETIC SENSOR DATA',source:'anaiero_p02b/telemetry.py'},
 {id:'power',label:'Data and Power Budget',description:'Calculate uncompressed channel bandwidth before designing a real neural data path.',kind:'ENGINEERING CALCULATOR',source:'docs/hardware.md'},
 {id:'world',label:'Metroidvania World Director',description:'Explore an original, tiny ability-gated world graph. Not a trained game chip.',kind:'ILLUSTRATIVE GAME PROTOTYPE',source:'registries/v6/module-manifests/CE-D01.json'},
 {id:'music',label:'Harmonic / Rhythmic Divergence',description:'A deterministic original note-grid example with a tension and meter control.',kind:'ILLUSTRATIVE MUSIC PROTOTYPE',source:'registries/v6/module-manifests/CE-D08.json'},
];
function Output({title,children}:{title:string;children:React.ReactNode}){
 return <div className="lab-output"><div className="lab-output-head"><span className="monitor-dot"/> {title}<span className="lab-live">LOCAL · NO EXTERNAL CONNECTION</span></div><div className="lab-output-body">{children}</div></div>;
}
function LotusLab(){
 const [gain,setGain]=useState(.72),[cost,setCost]=useState(.18),[delta,setDelta]=useState(-.03),[action,setAction]=useState('train_digital');
 const veto=action==='change_medium';
 const state=veto?'VETO':delta<-.05?'HOLD':gain-cost>0?'STAGE_DIGITAL':'UNRESOLVED';
 const explain=veto?'No laboratory or physical-care command exists in the digital controller.':
  delta<-.05?'A protected earlier competence would regress beyond the declared 0.05 margin.':
  gain-cost>0?'This digital educational action has a positive net score under the toy model.':'Net gain is not positive; more evidence is required.';
 return <div className="lab-two-col"><div className="lab-controls">
  <h3>Propose a digital learning action</h3><p>The model mirrors the reference policy categories; all values are illustrative.</p>
  <label className="field">Requested action<select value={action} onChange={e=>setAction(e.target.value)}><option value="train_digital">Train digital StarChip</option><option value="refresh_index">Refresh digital index</option><option value="change_medium">Alter living culture medium (forbidden)</option></select></label>
  {([['Expected capability gain',gain,setGain,0,1],['Compute cost',cost,setCost,0,1],['Protected old-skill change',delta,setDelta,-.3,.2]] as const).map(([label,val,change,min,max])=><label className="range-field" key={label}><span>{label}<strong>{val.toFixed(2)}</strong></span><input type="range" min={min} max={max} step=".01" value={val} onChange={e=>change(Number(e.target.value))}/></label>)}
 </div><Output title="LOTUS / DECISION RECEIPT"><div className={'outcome outcome-'+state.toLowerCase()}><span className="tiny-caps">DECISION</span><strong>{state}</strong></div><p>{explain}</p><dl className="receipt-lines"><div><dt>Physical authorization</dt><dd>NONE</dd></div><div><dt>Protected-skill margin</dt><dd>−0.05</dd></div><div><dt>Digital net benefit</dt><dd>{(gain-cost).toFixed(2)}</dd></div></dl><p className="tiny-note">Synthetic controls · source behavior: Lotus digital policy · no culture or stimulation actuation.</p></Output></div>;
}
function WindLab(){
 const [scenario,setScenario]=useState<'valid'|'expired'|'duplicate'|'forbidden'>('valid');
 const [log,setLog]=useState<string[]>([]);
 const [counter,setCounter]=useState(0);
 const run=()=>{
  const event='wind-'+String(counter+1).padStart(3,'0');
  const reason=scenario==='valid'?'accepted → queued → delivered to lotus':
   scenario==='expired'?'rejected → message TTL elapsed':
   scenario==='forbidden'?'rejected → route ACL denies physical-care recipient':
   'duplicate → event ID already in idempotency set';
  setLog(prev=>[event+' | '+reason,...prev].slice(0,8));
  setCounter(v=>v+1);
 };
 return <div className="lab-two-col"><div className="lab-controls"><h3>Inject a typed event</h3><p>One route, four outcomes. Watch the validation contract rather than a fake neural signal.</p><label className="field">Test input<select value={scenario} onChange={e=>setScenario(e.target.value)}><option value="valid">Valid observation → Lotus</option><option value="expired">Expired event</option><option value="duplicate">Repeated event identifier</option><option value="forbidden">Unauthorized physical-care route</option></select></label><button className="btn btn-primary" type="button" onClick={run}><Play size={15}/> Deliver simulated event</button></div>
 <Output title="WIND BUS / EVENT STREAM"><div className="terminal"><div className="term-dim">SOURCE observer.digital</div><div className="term-dim">ROUTE observation → lotus.host</div><div className="term-dim">AUTH synthetic · schema v1.0</div><div className="term-sep"/>{log.length?log.map((x,i)=><div className={i===0?'term-new':''} key={x}>{x}</div>):<div className="term-dim">[idle] No events delivered. Run a scenario.</div>}</div></Output></div>;
}
function CrystalLab(){
 const [text,setText]=useState('The Last Cat: a world gate opens only after the Star of Light restores a broken route.');
 const [consent,setConsent]=useState(true);
 const [digest,setDigest]=useState(''),[result,setResult]=useState('');
 async function run(){
  if(!consent||!text.trim()){setResult('REJECTED: source text or permission is missing.');return}
  const bytes=new TextEncoder().encode(text);
  const buf=await crypto.subtle.digest('SHA-256',bytes);
  setDigest(Array.from(new Uint8Array(buf)).map(v=>v.toString(16).padStart(2,'0')).join(''));
  setResult('Digital replay staged. Byte integrity check passed; learning and retention need separate tests.');
 }
 const terms=useMemo(()=>text.toLowerCase().match(/[a-z]{4,}/g)?.slice(0,8)||[],[text]);
 return <div className="lab-two-col"><div className="lab-controls"><h3>Compile a permissioned experience</h3><p>This browser model hashes actual input bytes, like the digital reference; it does not program a biological engram.</p><label className="field">Illustrative experience<textarea rows={5} value={text} onChange={e=>{setText(e.target.value);setDigest('');setResult('')}}/></label><label className="toggle-check"><input type="checkbox" checked={consent} onChange={e=>setConsent(e.target.checked)}/> Include this example in the local demonstration</label><button className="btn btn-primary" type="button" onClick={()=>void run()}><Play size={15}/> Compile digital Crystal</button></div><Output title="CRYSTAL / VERIFICATION RECEIPT">
  <div className="artifact-stack"><span className="tiny-caps">SOURCE · LOCAL ONLY</span><strong>{text?text.slice(0,92):'No source'}</strong><span className="artifact-arrow">↓</span><span className="tiny-caps">DIGEST / CONTENT-ADDRESSED STORAGE</span><code>{digest?digest.slice(0,32)+'…':'Not generated'}</code><span className="artifact-arrow">↓</span><span className="tiny-caps">REPLAY / SOURCE-BOUND RETRIEVAL</span><p>{result||'No package produced yet.'}</p><div className="chips">{terms.slice(0,5).map((w,i)=><span key={i} className="chip">{w}</span>)}</div></div><p className="tiny-note">Hashing is real browser computation. Retrieval terms are an illustrative index. There is no neural memory transfer or retained learned capability claim.</p>
 </Output></div>;
}
function CareLab(){
 const [age,setAge]=useState(12),[temp,setTemp]=useState(36.8),[valid,setValid]=useState(true),[fault,setFault]=useState(false),[checked,setChecked]=useState(false);
 const inspect=()=>{setChecked(true);if(!valid||age>120||temp<33||temp>41)setFault(true)};
 const status=fault?'FAULT_LATCHED':checked?'OBSERVE_ONLY':'NO SAMPLE';
 return <div className="lab-two-col"><div className="lab-controls"><h3>Synthetic incubation monitor</h3><p>Illustrative sensor values only. The real source requires OEM qualification and an independent care system.</p><label className="range-field"><span>Temperature reading <strong>{temp.toFixed(1)} °C</strong></span><input type="range" min="31" max="43" step=".1" value={temp} onChange={e=>setTemp(Number(e.target.value))}/></label><label className="range-field"><span>Sample age <strong>{age} seconds</strong></span><input type="range" min="0" max="300" value={age} onChange={e=>setAge(Number(e.target.value))}/></label><label className="toggle-check"><input type="checkbox" checked={valid} onChange={e=>setValid(e.target.checked)}/> Simulated authentication valid</label><div className="row-gap"><button className="btn btn-primary" onClick={inspect}><Activity size={15}/> Inspect sample</button><button className="btn btn-secondary" onClick={()=>{setFault(false);setChecked(false)}}><RotateCcw size={14}/> Reset test</button></div></div><Output title="SUSTI BUS / READ-ONLY"><div className={'outcome '+(fault?'outcome-veto':'outcome-stage_digital')}><span className="tiny-caps">SUPERVISOR STATE</span><strong>{status}</strong></div><dl className="receipt-lines"><div><dt>Sample validity</dt><dd>{valid?'Synthetic auth valid':'AUTH FAILED'}</dd></div><div><dt>Age</dt><dd>{age>120?'STALE · '+age+'s':age+'s'}</dd></div><div><dt>Physical actions</dt><dd>NOT IMPLEMENTED</dd></div><div><dt>Review</dt><dd>{fault?'HUMAN REVIEW REQUIRED':'NO ACTUATION'}</dd></div></dl><p className="tiny-note">Source: read-only supervisor digital twin. No actual incubator, cell preparation or sensor is connected.</p></Output></div>;
}
function PowerLab(){
 const [channels,setChannels]=useState(1020),[rate,setRate]=useState(20000),[bytes,setBytes]=useState(2);
 const bps=channels*rate*bytes;
 return <div className="lab-two-col"><div className="lab-controls"><h3>Neural acquisition bandwidth</h3><p>Hypothetical raw stream arithmetic. It is not a real instrument, compressed pipeline or measured power consumption.</p><label className="field">Channels<input type="number" min="1" max="100000" value={channels} onChange={e=>setChannels(Math.max(1,Number(e.target.value)||1))}/></label><label className="field">Samples per second<input type="number" min="1" max="500000" value={rate} onChange={e=>setRate(Math.max(1,Number(e.target.value)||1))}/></label><label className="field">Bytes per sample<select value={bytes} onChange={e=>setBytes(Number(e.target.value))}><option value="2">2 bytes (16-bit)</option><option value="3">3 bytes (24-bit)</option><option value="4">4 bytes (32-bit)</option></select></label></div><Output title="POWER / DATA TRANSPORT"><div className="power-big"><strong>{(bps/1e6).toFixed(2)}</strong><span>MB/s raw stream</span></div><div className="power-bars"><span style={{width:Math.min(100,bps/8e7*100)+'%'}}/></div><dl className="receipt-lines"><div><dt>Hourly storage</dt><dd>{(bps*3600/1e9).toFixed(2)} GB</dd></div><div><dt>Daily storage</dt><dd>{(bps*86400/1e12).toFixed(2)} TB</dd></div><div><dt>Bluetooth suitability</dt><dd>NOT FOR RAW ARRAY STREAM</dd></div></dl><p className="tiny-note">These are uncompressed decimal bytes and exclude framing, metadata and processing. Electrical watts require measured voltage/current and efficiency.</p></Output></div>;
}
const rooms=[
 {id:0,name:'The Sanctuary',need:null,reward:null,x:55,y:154},
 {id:1,name:'Starlit Hall',need:null,reward:'LANTERN',x:168,y:100},
 {id:2,name:'Ancient Shaft',need:null,reward:'JUMP',x:168,y:214},
 {id:3,name:'Crystal Vault',need:'LANTERN',reward:'CRYSTAL',x:300,y:70},
 {id:4,name:'Broken Bridge',need:'JUMP',reward:null,x:300,y:230},
 {id:5,name:'Echo Cathedral',need:'CRYSTAL',reward:'ECHO',x:435,y:94},
 {id:6,name:'Silent Garden',need:'JUMP',reward:null,x:445,y:235},
 {id:7,name:'Leviathan Gate',need:'ECHO',reward:null,x:585,y:160}
];
const edges=[[0,1],[0,2],[1,3],[2,4],[3,5],[4,6],[5,7],[6,7]];
function WorldLab(){
 const [current,setCurrent]=useState(0),[inventory,setInventory]=useState<string[]>([]);
 const reachable=edges.filter(e=>e.includes(current)).map(e=>e[0]===current?e[1]:e[0]);
 const move=(id:number)=>{
  const room=rooms[id];
  if(room.need&&!inventory.includes(room.need))return;
  setCurrent(id);if(room.reward&&!inventory.includes(room.reward))setInventory(x=>[...x,room.reward!]);
 };
 return <div className="lab-two-col wide-lab"><div className="lab-controls"><h3>Explore an ability-gated world</h3><p>A miniature original graph with progression constraints. It is a React demonstration, not the proposed trained Metroidvania chip.</p><div className="micro-label">CURRENT ROOM</div><strong>{rooms[current].name}</strong><div className="micro-label spaced">ACQUIRED ABILITIES</div><div className="chips">{inventory.length?inventory.map(x=><span className="chip" key={x}>{x}</span>):<span className="tiny-note">None yet</span>}</div><div className="micro-label spaced">AVAILABLE MOVES</div>{reachable.map(id=><button key={id} type="button" className="room-choice" disabled={Boolean(rooms[id].need&&!inventory.includes(rooms[id].need))} onClick={()=>move(id)}><span>{rooms[id].name}</span><span>{rooms[id].need&&!inventory.includes(rooms[id].need)?<LockKeyhole size={14}/>:<ChevronRight size={15}/>}</span></button>)}<button className="btn btn-secondary" type="button" onClick={()=>{setCurrent(0);setInventory([])}}><RotateCcw size={14}/> Restart graph</button></div><Output title="STAR CHIP / WORLD GRAPH"><svg className="room-svg" viewBox="0 0 650 310" role="img" aria-label="Illustrative platform game room graph">{edges.map(([a,b],i)=><line key={i} x1={rooms[a].x+18} y1={rooms[a].y+12} x2={rooms[b].x+18} y2={rooms[b].y+12} stroke="#456b7c" strokeDasharray="4 4" strokeWidth="2"/>)}{rooms.map(r=><g key={r.id} onClick={()=>{if(reachable.includes(r.id))move(r.id)}} style={{cursor:reachable.includes(r.id)?'pointer':'default'}}><rect x={r.x} y={r.y} width="54" height="36" rx="7" fill={current===r.id?'#56d4bc':r.need&&!inventory.includes(r.need)?'#182433':'#204655'} stroke={current===r.id?'#9affeb':'#6293a0'}/><text x={r.x+27} y={r.y+23} textAnchor="middle" fill={current===r.id?'#102233':'#e2f5f4'} fontSize="13" fontWeight="bold">{r.id+1}</text></g>)}</svg><div className="tiny-note">1 Sanctuary · 8 Leviathan Gate. A locked route must be unlocked before entry. Edges represent authored sample rules.</div>{current===7&&<div className="mini-success"><Check size={17}/> Sample progression solved. A production generator would require automated graph validation.</div>}</Output></div>;
}
const NOTE_LABELS=['C','D','E','F','G','A','B'];
function MusicLab(){
 const [tension,setTension]=useState(4),[tempo,setTempo]=useState(128),[seed,setSeed]=useState(3);
 const events=useMemo(()=>Array.from({length:24},(_,i)=>({step:i,pitch:(i*5+seed+(i%4===0?tension:0))%7,active:(i*7+seed)%9<7})),[tension,seed]);
 return <div className="lab-two-col"><div className="lab-controls"><h3>Original note-event sketch</h3><p>Explore harmonic position and rhythmic omissions in a simple original, deterministic exercise—not a finished AHD/ARD trained composer.</p><label className="range-field"><span>Divergence depth <strong>{tension}</strong></span><input type="range" min="0" max="7" value={tension} onChange={e=>setTension(Number(e.target.value))}/></label><label className="range-field"><span>Tempo <strong>{tempo} BPM</strong></span><input type="range" min="70" max="180" value={tempo} onChange={e=>setTempo(Number(e.target.value))}/></label><button className="btn btn-secondary" onClick={()=>setSeed(v=>v+1)}><RotateCcw size={15}/> New deterministic seed</button></div><Output title="MUSIC / NOTE-STATE MATRIX"><div className="note-matrix">{NOTE_LABELS.map((n,idx)=><div className="note-row" key={n}><span>{n}</span><div>{events.map(e=><i key={e.step} className={e.active&&e.pitch===6-idx?'note-on':''}/>)}</div></div>)}</div><p className="tiny-note">Seed {seed} · original note data · no existing song is played or copied. AHD and ARD documents remain the formal research source.</p></Output></div>;
}
export function InteractiveLab({initial='lotus'}:{initial?:LabId}){
 const [active,setActive]=useState<LabId>(initial);
 const selected=labs.find(x=>x.id===active)!;
 return <section className="lab-center">
  <div className="lab-list">{labs.map((l,i)=><button key={l.id} type="button" className={'lab-list-item '+(active===l.id?'active':'')} onClick={()=>setActive(l.id)}><span className="lab-list-num">{String(i+1).padStart(2,'0')}</span><div><strong>{l.label}</strong><small>{l.kind}</small></div><ChevronRight size={16}/></button>)}</div>
  <div className="lab-workspace"><div className="lab-topbar"><div><div className="eyebrow">{selected.kind}</div><h2>{selected.label}</h2><p>{selected.description}</p></div><a href={sourceLink(selected.source)} target="_blank" rel="noreferrer" className="quiet-link">View verified source <ArrowUpRight size={15}/></a></div>
   {active==='lotus'&&<LotusLab/>}{active==='wind'&&<WindLab/>}{active==='crystal'&&<CrystalLab/>}{active==='care'&&<CareLab/>}{active==='power'&&<PowerLab/>}{active==='world'&&<WorldLab/>}{active==='music'&&<MusicLab/>}
  </div>
 </section>;
}
export function Studio({manifest}:{manifest:Manifest}){
 const {data:chips,error}=useData<Chip[]>('chips.json');
 const [filter,setFilter]=useState<'all'|'digital'|'physical'|'research'>('all');
 const list=chips?.filter(c=>filter==='all'||filter==='digital'&&c.module_id.startsWith('CE-D')||filter==='physical'&&c.module_id.startsWith('CE-P')||filter==='research'&&c.module_id.startsWith('CE-R'))||[];
 return <div className="page-block"><div className="section-intro"><div className="eyebrow"><Cpu size={15}/> STAR CHIP STUDIO</div><h1>Give a system<br/><em>another way to understand.</em></h1><p>Explore proposed specialist chips—and test real, bounded digital models right here. Every output describes whether it is source-tested software, an illustrative browser algorithm, or future research.</p></div>
 <div className="section-heading"><span>INTERACTIVE DIGITAL LABORATORY</span><span className="subtle">Seven browser experiments · no live biological equipment</span></div><InteractiveLab/>
 <div className="section-heading"><span>CHIP CAPABILITY CATALOGUE</span><span className="subtle">Versioned research and implementation manifests</span></div>
 <div className="tabs-row">{([['all','All chips'],['digital','Specialist digital'],['physical','Physical candidates'],['research','Research services']] as const).map(([id,name])=><button key={id} className={'tab-pill '+(filter===id?'active':'')} onClick={()=>setFilter(id)}>{name}</button>)}</div>
 {error&&<p className="error-state">{error}</p>}
 <div className="chip-grid">{list.map(c=>{
 const meta=manifest.documents.find(d=>d.source_path==='registries/v6/module-manifests/'+c.module_id+'.json');
 return <article className="chip-card" key={c.module_id}><div className="chip-icon"><Cpu size={21}/></div><div className="chip-status">{c.stage||'PROPOSED'}</div><h3>{c.name}</h3><p>{c.capabilities?.join(', ').replaceAll('-',' ')}</p><div className="chip-bottom"><span>{c.module_id}</span>{meta&&<Link to={readRoute(meta)}>View manifest <ArrowUpRight size={14}/></Link>}</div></article>
 })}</div>
 {!chips&&!error&&<p className="helper">Loading the public chip manifests…</p>}
 </div>;
}
