"use strict";
(() => {
const byId=id=>document.getElementById(id),canvas=byId("scene"),ctx=canvas.getContext("2d");
const replay=window.CEVLabReplay;
if(!replay)throw Error("SIM3.1 replay validator not loaded");
const v={yaw:-.65,pitch:.43,zoom:1.45,frames:[],idx:0,colony:"all",selected:null,points:[],overlay:[],playing:false,last:0};
const color=c=>!c.alive?"#738598":c.kind==="blood_carrier"?"#f17b8d":c.kind.includes("neuron")?"#6fdce8":"#cce79b";
const demo=[0,1].map(t=>({origin:"SIMULATED_UNCALIBRATED",format:"CEVLABFRAME/1",run:"viewer-preview",tick:t,law_version:1,
  config_sha256:"0".repeat(64),snapshot_sha256:"0".repeat(64),
  colonies:[{id:"crest",food:70-t,data:0,signal:0,stress:0},{id:"blood",food:90,data:t,signal:0,stress:0}],
  cells:[{colony:"crest",id:1,parent:0,kind:"crest_progenitor",x:10,y:12,z:2,energy:40,maturity:t,learning:0,birth:0,death:0,alive:true},
         {colony:"crest",id:2,parent:1,kind:"blood_carrier",x:3+t,y:5,z:2,energy:40,maturity:t,learning:0,birth:0,death:0,alive:true},
         {colony:"blood",id:3,parent:0,kind:"neuron",x:20,y:17,z:4,energy:40,maturity:t,learning:t,birth:0,death:0,alive:true}],
  links:[{from:"crest",to:"blood",channel:"food",units:3},{from:"blood",to:"crest",channel:"data",units:2}]}));
function renderMetrics(){const f=v.frames[v.idx];if(!f)return;const cells=f.cells;
  byId("metrics").textContent="Tick "+f.tick+" · "+f.colonies.length+" colonies · "+cells.length+" cells · "+
    cells.filter(c=>c.alive).length+" viable · "+cells.filter(c=>!c.alive).length+" historical tombstones";
  const c=v.selected&&cells.find(x=>x.id===v.selected.id&&x.colony===v.selected.colony);
  byId("selected").textContent=c?c.colony+" / "+c.id+" ("+c.kind+") · parent "+c.parent+
    " · energy proxy "+c.energy+" · learning proxy "+c.learning+
    (c.alive?" · viable":" · tombstone, death tick "+c.death):"Click a cell.";
}
function setFrames(frames,title){replay.validateTimeline(frames);
  v.frames=frames;v.idx=0;v.selected=null;v.colony="all";v.playing=false;byId("play").textContent="Play";
  byId("source").textContent=title+" · "+frames.length+" frames";
  byId("time").max=String(frames.length-1);byId("time").value="0";
  const select=byId("colony");select.replaceChildren(new Option("All","all"));
  for(const col of frames[0].colonies)select.add(new Option(col.id,col.id));
  renderMetrics();draw();
}
function project(p,w,h){const cy=Math.cos(v.yaw),sy=Math.sin(v.yaw),cp=Math.cos(v.pitch),sp=Math.sin(v.pitch);
  const x=p.x*cy-p.y*sy,d=p.x*sy+p.y*cy,y=p.z*cp-d*sp;
  const scale=Math.min(8,Math.max(.2,v.zoom))*3.8*Math.min(1.2,w/850);
  return {x:w*.5+x*scale,y:h*.55-y*scale,depth:d};}
function world(c,f){return {x:c.x+(f.colonies.findIndex(x=>x.id===c.colony)*80)-45,y:c.y,z:c.z||0};}
function line(a,b,c,width=2){ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);ctx.strokeStyle=c;ctx.lineWidth=width;ctx.stroke();}
function draw(){const rect=canvas.getBoundingClientRect(),w=rect.width,h=rect.height,dpr=Math.min(2,window.devicePixelRatio||1);
  if(!w||!h)return;canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.clearRect(0,0,w,h);const f=v.frames[v.idx];if(!f)return;
  for(let n=-80;n<=140;n+=20){line(project({x:n,y:-80,z:0},w,h),project({x:n,y:120,z:0},w,h),"#244154",1);
    line(project({x:-80,y:n,z:0},w,h),project({x:140,y:n,z:0},w,h),"#244154",1);}
  const visible=f.cells.filter(c=>v.colony==="all"||v.colony===c.colony);
  if(byId("wiring").checked)for(const edge of f.links){const a=f.cells.find(c=>c.colony===edge.from),b=f.cells.find(c=>c.colony===edge.to);
    if(a&&b&&(v.colony==="all"||v.colony===edge.from||v.colony===edge.to))
      line(project(world(a,f),w,h),project(world(b,f),w,h),edge.channel==="food"?"#aaca84":"#73d4ec",2);}
  if(byId("lineage").checked)for(const c of visible){const parent=f.cells.find(x=>x.colony===c.colony&&x.id===c.parent);
    if(parent)line(project(world(parent,f),w,h),project(world(c,f),w,h),"#8da5bb",1);}
  const fieldName=byId("soil").value;
  if(fieldName!=="none" && byId("showOverlay").checked)for(const o of v.overlay){
    if(v.colony!=="all" && v.colony!==o.colony)continue;
    const layer=o.fields.layers.find(x=>x.name===fieldName);
    if(!layer)continue;
    const highest=Math.max(1,...layer.values);
    for(let i=0;i<64;i++){
      const amount=layer.values[i];
      if(amount<=0)continue;
      const at=project(world({colony:o.colony,x:(i%8)*4+1,y:Math.floor(i/8)*4+1,z:0},f),w,h);
      const intensity=Math.max(.1,Math.min(.75,amount/highest*.75));
      ctx.fillStyle=fieldName==="rna"?`rgba(190,153,248,${intensity})`:`rgba(161,226,163,${intensity})`;
      ctx.beginPath();ctx.arc(at.x,at.y,4+intensity*5,0,Math.PI*2);ctx.fill();
    }
  }
  if(byId("showOverlay").checked)for(const o of v.overlay)for(const c of o.cells){if(v.colony!=="all"&&v.colony!==o.colony)continue;
    const p=project(world({...c,colony:o.colony},f),w,h);ctx.beginPath();ctx.arc(p.x,p.y,12,0,2*Math.PI);
    ctx.setLineDash([3,3]);ctx.strokeStyle="#d7acef";ctx.stroke();ctx.setLineDash([]);}
  v.points=[];
  for(const c of visible){const p=project(world(c,f),w,h);v.points.push({p,c});
    const radial=ctx.createRadialGradient(p.x-3,p.y-4,1,p.x,p.y,13);
    radial.addColorStop(0,"#fff");radial.addColorStop(.25,color(c));radial.addColorStop(1,"#1b2a3d");
    ctx.fillStyle=radial;ctx.beginPath();ctx.arc(p.x,p.y,11,0,2*Math.PI);ctx.fill();
    if(v.selected&&v.selected.id===c.id&&v.selected.colony===c.colony){ctx.strokeStyle="#fff";ctx.lineWidth=2;ctx.stroke();}
    ctx.fillStyle="#d3e3f4";ctx.font="11px system-ui";ctx.fillText(c.colony+":"+c.id,p.x+13,p.y+3);}
}
function seek(i){v.idx=Math.max(0,Math.min(v.frames.length-1,i));byId("time").value=v.idx;renderMetrics();draw();}
let pointer=null,start=null;
canvas.onpointerdown=e=>{pointer={x:e.clientX,y:e.clientY};start={...pointer};canvas.setPointerCapture(e.pointerId);};
canvas.onpointermove=e=>{if(!pointer)return;v.yaw+=(e.clientX-pointer.x)*.008;
  v.pitch=Math.max(-1.2,Math.min(1.45,v.pitch+(e.clientY-pointer.y)*.006));
  pointer={x:e.clientX,y:e.clientY};draw();};
canvas.onpointerup=e=>{if(start&&Math.hypot(e.clientX-start.x,e.clientY-start.y)<5){
  const b=canvas.getBoundingClientRect(),x=e.clientX-b.left,y=e.clientY-b.top;
  let chosen=null,dist=20;for(const point of v.points){const d=Math.hypot(point.p.x-x,point.p.y-y);
    if(d<dist){dist=d;chosen=point.c;}}
  if(chosen){v.selected={id:chosen.id,colony:chosen.colony};renderMetrics();draw();}}
  pointer=null;start=null;};
canvas.addEventListener("wheel",e=>{e.preventDefault();v.zoom=Math.max(.2,Math.min(8,v.zoom*Math.exp(-e.deltaY*.001)));draw();},{passive:false});
byId("time").oninput=e=>seek(Number(e.target.value));
byId("colony").onchange=e=>{v.colony=e.target.value;v.selected=null;renderMetrics();draw();};
for(const key of ["lineage","wiring","showOverlay","soil"])byId(key).onchange=draw;
byId("play").onclick=()=>{v.playing=!v.playing;byId("play").textContent=v.playing?"Pause":"Play";v.last=0;};
byId("reset").onclick=()=>{v.yaw=-.65;v.pitch=.43;v.zoom=1.45;draw();};
async function readJsonLines(file,max=10000){return replay.parseNDJSON(await file.text(),max);}
byId("input").onchange=async e=>{const file=e.target.files?.[0];if(!file)return;
  try{setFrames(await readJsonLines(file),file.name);}catch(error){byId("source").textContent="Rejected: "+error.message;}};
byId("overlay").onchange=async e=>{const file=e.target.files?.[0];if(!file)return;
  try{const imported=await readJsonLines(file,32);
    replay.validateOverlays(imported);
    v.overlay=imported;draw();
  }catch(error){byId("source").textContent="Rejected overlay: "+error.message;}};
function animate(t){if(v.playing){if(!v.last)v.last=t;if(t-v.last>340){
    v.last=t;if(v.idx+1<v.frames.length)seek(v.idx+1);
    else{v.playing=false;byId("play").textContent="Play";}}}
  requestAnimationFrame(animate);}
window.addEventListener("resize",draw);
setFrames(demo,"UI-only synthetic preview; load a generated replay for actual SIM3 frames");
requestAnimationFrame(animate);
})();
