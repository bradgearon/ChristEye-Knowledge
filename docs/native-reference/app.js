/* Anaiero Native Reference — a dependency-free, offline-capable source browser. */
(() => {
  'use strict';
  const D = window.ANAIERO_DOCS || {meta:{}, entries:[], files:[], guides:[], layers:[]};
  const M = D.meta || {};
  const rawEntries = D.entries || [];
  let entries = rawEntries;
  const files = D.files || [];
  const guides = D.guides || [];
  const rawById = new Map(rawEntries.map(e => [String(e.id), e]));
  let byId = new Map(entries.map(e => [String(e.id), e]));
  const fileById = new Map(files.map(f => [String(f.id), f]));
  const fileByPath = new Map(files.map(f => [f.path, f]));
  const guideById = new Map(guides.map(g => [String(g.id), g]));
  let byQualified = new Map(entries.map(e => [e.qualified, e]));
  const $ = id => document.getElementById(id);
  const esc = value => String(value == null ? '' : value).replace(/[&<>"']/g, ch => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch]));
  const inlineCode = value => esc(value).replace(/`([^`]+)`/g,'<code class="inline-code">$1</code>');
  const number = value => Number(value || 0).toLocaleString('en-US');
  const defaultLayers = [
    {id:'application',title:'Application',description:'Composes engines, sessions, experiments, and mesh workflows into usable native runtimes.'},
    {id:'services',title:'Services',description:'Owns domain decisions: simulation, optimization, evidence, and state transitions.'},
    {id:'contracts',title:'Contracts',description:'Typed interfaces and passive records that connect replaceable implementations.'},
    {id:'infrastructure',title:'Infrastructure',description:'Storage, queues, processes, transport, and host-specific adapters.'},
    {id:'models',title:'Models',description:'Shared data structures and model values.'},
    {id:'living',title:'Living core',description:'The original digital simulation, virtual machine, and durable state implementation.'},
    {id:'wind',title:'Wind core',description:'Packet codecs, bounded scheduling, and media routing.'},
    {id:'sources',title:'Native sources',description:'Compiled implementation files and native entry points.'}
  ];
  const layerMap = new Map(defaultLayers.map(l=>[l.id,l]));
  (D.layers || []).forEach(l=>layerMap.set(l.id,{...(layerMap.get(l.id)||{}),...l}));
  const layers = [...layerMap.values()].filter(l => entries.some(e=>e.layer===l.id) || files.some(f=>f.layer===l.id));
  const typeKinds = new Set(['class','struct','enum','union','alias','typedef','concept']);
  const functionKinds = new Set(['function','method','constructor','destructor','operator']);
  const valueKinds = new Set(['field','constant','variable','enumerator','macro']);
  const state = {layer:'all',kind:'classes',query:'',internal:false,route:'overview',selected:null,tab:'reference',limit:120,activeIndex:-1};
  const expandedTypes = new Set();
  const expandedFamilies = new Set();
  const collapsedTypes = new Set();
  const collapsedFamilies = new Set();
  const collapsedNamespaces = new Set();
  // Keep all source declarations and legacy IDs, but browse logical C++ entities.
  function declarationKey(e){
    const q=e.qualified||e.name;
    if(typeKinds.has(e.kind))return (['class','struct'].includes(e.kind)?'nominal':e.kind)+'|'+q;
    if(functionKinds.has(e.kind)){
      const parameters=(e.parameters||[]).map(p=>String(p.type||p.declaration||'').replace(/\s+/g,'')).join(',');
      const signature=String(e.signature||'');
      const qualifiers=(signature.match(/\)\s*((?:(?:const|volatile|noexcept)\b\s*|&&?\s*)*)/)||[])[1]||'';
      const cv=qualifiers.replace(/\bnoexcept\b/g,'').replace(/\s+/g,'');
      const template=(signature.match(/^\s*template\s*<([\s\S]*?)>\s*/) || [])[1]||'';
      return 'call|'+q+'('+parameters+')'+cv+'|'+template.replace(/\s+/g,'');
    }
    return e.kind+'|'+q;
  }
  function declarationWeight(e){return (typeKinds.has(e.kind)?(e.memberIds||[]).length*100:0)+(e.hasBody?10000:0)+Math.min(String(e.source||'').length,9000)+(e.descriptionSource==='reviewed'?100:0);}
  const declarationGroups=new Map();
  rawEntries.forEach(e=>{const key=declarationKey(e);if(!declarationGroups.has(key))declarationGroups.set(key,[]);declarationGroups.get(key).push(e);});
  entries=[...declarationGroups.values()].map(group=>{
    const ordered=group.slice().sort((a,b)=>declarationWeight(b)-declarationWeight(a));
    const result={...ordered[0],_declarations:group,aliasIds:group.map(e=>e.id)};
    result.memberIds=[...new Set(group.flatMap(e=>e.memberIds||[]))];
    const reviewed=ordered.find(e=>e.descriptionSource==='reviewed');
    if(reviewed){for(const key of ['description','details','returns','descriptionSource','evidence'])if(reviewed[key])result[key]=reviewed[key];}
    result.internal=group.every(e=>e.internal||['private','protected'].includes(e.access));
    if(!result.internal)result.access='public';
    return result;
  });
  byId=new Map();entries.forEach(e=>e.aliasIds.forEach(id=>byId.set(String(id),e)));
  const typesByName=new Map(entries.filter(e=>typeKinds.has(e.kind)).map(e=>[e.qualified,e]));
  entries.forEach(e=>{
    let parent=byId.get(String(e.parentId));
    if(!parent&&e.parent)parent=typesByName.get(e.parent);
    if(!parent){const q=String(e.qualified||'');const pos=q.lastIndexOf('::');if(pos>=0)parent=typesByName.get(q.slice(0,pos));}
    if(parent){e.parentId=parent.id;e.parent=parent.qualified;if(e.kind==='function')e.kind='method';if(e.kind==='method')e.name=String(e.qualified).slice(String(parent.qualified).length+2);}
  });
  byQualified=new Map(entries.map(e=>[e.qualified,e]));
  let currentResults = [];
  let toastTimer;
  const symbolsByParent = new Map();
  entries.forEach(e => { if(e.parentId){const k=String(e.parentId);if(!symbolsByParent.has(k))symbolsByParent.set(k,[]);symbolsByParent.get(k).push(e);} });
  const publicEntry = e => state.internal || (!e.internal && !['private','protected'].includes(e.access));
  const sourceURL = (path,line,endLine) => 'https://github.com/'+(M.repository||'bradgearon/ChristEye-Knowledge')+'/blob/'+(M.commit||M.branch||'HEAD')+'/'+String(path||'').split('/').map(encodeURIComponent).join('/')+(line?'#L'+line+(endLine&&endLine!==line?'-L'+endLine:''):'');
  const repoURL = 'https://github.com/'+(M.repository||'bradgearon/ChristEye-Knowledge');
  const layerTitle = id => (layerMap.get(id)||{}).title || id || 'Native';
  const iconLetter = kind => ({class:'C',struct:'S',enum:'E',union:'U',alias:'T',typedef:'T',function:'ƒ',method:'ƒ',constructor:'+',destructor:'~',operator:'ƒ',constant:'k',variable:'v',field:'·',enumerator:'e',macro:'#',file:'◇',guide:'▤'}[kind]||'T');
  const kindIcon = kind => '<span class="kind-icon kind-'+esc(kind)+'" aria-hidden="true">'+iconLetter(kind)+'</span>';
  const symbolHref = e => '#symbol/'+encodeURIComponent(e.id);
  const fileHref = f => '#file/'+encodeURIComponent(f.id);
  const guideHref = g => '#guide/'+encodeURIComponent(g.id);
  const typeLabel = kind => kind==='alias'?'type alias':kind || 'symbol';
  const qualified = e => e.qualified || e.name || '';
  const ownerOf = e => byId.get(String(e.parentId));
  function rootOf(e){let current=e,parent=ownerOf(e),seen=new Set();while(parent&&!seen.has(parent.id)){seen.add(parent.id);current=parent;parent=ownerOf(parent);}return current;}
  function callableType(e){return e.kind==='class'||e.kind==='struct'&&(symbolsByParent.get(String(e.id))||[]).some(m=>functionKinds.has(m.kind));}
  function categoryOf(e){if(callableType(e))return 'classes';if(typeKinds.has(e.kind))return 'data';if(functionKinds.has(e.kind))return 'functions';return 'constants';}
  function rootEntries(){return entries.filter(e=>!ownerOf(e)&&!['constructor','destructor'].includes(e.kind));}
  const categoryTitles={classes:'Classes & interfaces',data:'Data types',functions:'Free functions',constants:'Constants'};
  const countLayer = id => rootEntries().filter(e=>e.layer===id&&publicEntry(e)&&typeKinds.has(e.kind)).length;
  function lifecycleName(e){
    if(e.kind==='destructor')return 'Destructor';
    if(e.kind!=='constructor')return e.name;
    const p=(e.parameters||[]),owner=ownerOf(e),type=String(p[0]?.type||'');
    let label=!p.length?'Default constructor':p.length===1&&owner&&type.includes(owner.name)&&type.includes('&')?(type.includes('&&')?'Move constructor':'Copy constructor'):'Constructor';
    if(/=\s*delete\b/.test(e.signature||''))label+=' (deleted)';
    return label;
  }
  function familyKey(e){return ['constructor','destructor'].includes(e.kind)?e.kind+'|'+e.parentId:qualified(e);}
  function groupMembers(list){const map=new Map();list.forEach(e=>{const key=familyKey(e);if(!map.has(key))map.set(key,[]);map.get(key).push(e);});return [...map.entries()].map(([key,items])=>({key,items:items.sort((a,b)=>(a.parameters||[]).length-(b.parameters||[]).length||String(a.signature).localeCompare(String(b.signature))),entry:items[0],label:items[0].kind==='constructor'?'Constructors':items[0].kind==='destructor'?'Destructor':items[0].name}));}
  function toast(message){$('toast').textContent=message;$('toast').classList.add('visible');clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('toast').classList.remove('visible'),2200);}
  async function copy(value){try{await navigator.clipboard.writeText(value);toast('Copied to clipboard');}catch(_){const t=document.createElement('textarea');t.value=value;t.style.position='fixed';t.style.left='-9999px';document.body.appendChild(t);t.select();const ok=document.execCommand('copy');t.remove();toast(ok?'Copied to clipboard':'Copy unavailable in this browser');}}
  function closePanels(){document.body.classList.remove('nav-open','browse-open');$('menu-toggle').setAttribute('aria-expanded','false');}
  function navigate(route){closePanels();if(location.hash==='#'+route)readRoute();else location.hash=route;}
  function highlight(code){
    const rx=/(\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|^\s*#\s*\w+|\b(?:alignas|auto|bool|break|case|catch|char|class|const|constexpr|consteval|constinit|continue|decltype|default|delete|do|double|else|enum|explicit|extern|false|final|float|for|friend|if|inline|int|long|mutable|namespace|new|noexcept|nullptr|operator|override|private|protected|public|return|short|signed|sizeof|static|static_assert|struct|switch|template|this|throw|true|try|typedef|typename|union|unsigned|using|virtual|void|volatile|while)\b|\b(?:0x[0-9a-fA-F]+|\d+(?:\.\d+)?)[uUlLfF]*\b)/gm;
    let out='',cursor=0,match;
    while((match=rx.exec(code))!==null){out+=esc(code.slice(cursor,match.index));const token=match[0];const type=token.startsWith('//')||token.startsWith('/*')?'comment':token.startsWith('"')||token.startsWith("'")?'string':/^\s*#/.test(token)?'directive':/^\d/.test(token)?'number':'keyword';out+='<span class="tok-'+type+'">'+esc(token)+'</span>';cursor=rx.lastIndex;}
    return out+esc(code.slice(cursor));
  }
  function codeBlock(code,title,options={}){
    const id='copy-'+Math.random().toString(36).slice(2);
    const source=options.lines?'<pre class="source-code"><code>'+String(code).split('\n').map((line,i)=>'<span class="code-line"><span class="line-number">'+(Number(options.start||1)+i)+'</span><span class="line-content">'+highlight(line)+'</span></span>').join('')+'</code></pre>':'<pre class="wrap"><code>'+highlight(String(code))+'</code></pre>';
    return '<div class="code-block"><div class="code-bar"><span>'+esc(title)+'</span><span class="code-actions">'+(options.url?'<a href="'+esc(options.url)+'" target="_blank" rel="noopener noreferrer">GitHub ↗</a>':'')+'<button class="text-button" data-copy-target="'+id+'">Copy</button></span></div><template id="'+id+'">'+esc(code)+'</template>'+source+'</div>';
  }
  function breadcrumb(parts){return '<div class="breadcrumb"><a href="#overview">Native reference</a>'+parts.map((p,i)=>'<span class="sep">/</span>'+(p.href?'<a href="'+esc(p.href)+'">'+esc(p.title)+'</a>':'<span class="crumb-end">'+esc(p.title)+'</span>')).join('')+'</div>';}
  function footer(){return '<footer class="doc-footer"><span>Source snapshot <a href="'+repoURL+'/commit/'+esc(M.commit||'HEAD')+'" target="_blank" rel="noopener noreferrer">'+esc(String(M.commit||'').slice(0,7))+'</a> · '+esc(M.repository||'Anaiero native')+'</span><a href="'+repoURL+'/pull/'+(M.pr||44)+'" target="_blank" rel="noopener noreferrer">PR #'+esc(M.pr||44)+' ↗</a></footer>';}
  function mount(html,keepScroll=false){const y=$('main-content').scrollTop;$('document').innerHTML='<article class="doc-inner">'+html+footer()+'</article>';if(!keepScroll)$('main-content').scrollTop=0;else $('main-content').scrollTop=y;}
  function updateNav(){
    document.querySelectorAll('.nav-link').forEach(button=>{
      const route=button.dataset.route,layer=button.dataset.layer;
      const active=route?(route==='overview'?state.route==='overview':route==='guides'?['guides','guide'].includes(state.route):['files','file'].includes(state.route)):(state.layer===layer&&['layer','symbol','browse'].includes(state.route));
      button.classList.toggle('active',active);button.setAttribute('aria-current',active?'page':'false');
    });
    $('all-count').textContent=number(catalogGroups('classes','all').length+catalogGroups('data','all').length);
    document.querySelectorAll('[data-layer-count]').forEach(e=>e.textContent=number(countLayer(e.dataset.layerCount)));
  }
  function browserLink(e,label,context,extraClass=''){
    const active=byId.get(String(state.selected))?.id===e.id;
    return '<a class="symbol-item '+extraClass+(active?' active':'')+'" href="'+symbolHref(e)+'" title="'+esc(qualified(e))+'">'+kindIcon(e.kind)+'<span class="symbol-text"><span class="symbol-name">'+esc(label||lifecycleName(e))+'</span>'+(context?'<span class="symbol-context">'+esc(context)+'</span>':'')+'</span></a>';
  }
  function familyRows(family,nested=false,search=false){
    const items=family.items,first=items[0],isOverload=items.length>1;
    if(!isOverload){const context=functionKinds.has(first.kind)?compactSignature(first):typeLabel(first.kind);return browserLink(first,lifecycleName(first),context,nested?'member-item':'');}
    const key=family.key,open=!collapsedFamilies.has(key)&&(expandedFamilies.has(key)||search);
    const label=first.kind==='constructor'?'Constructors':family.label;
    return '<div class="overload-family"><button class="family-toggle" data-family-toggle="'+esc(key)+'" aria-expanded="'+open+'"><span class="chevron'+(open?' open':'')+'">›</span>'+kindIcon(first.kind)+'<span class="family-label">'+esc(label)+'</span><span class="overload-count">'+items.length+' overloads</span></button>'+(open?'<div class="overload-children">'+items.map(e=>browserLink(e,e.kind==='constructor'?lifecycleName(e):e.name,compactSignature(e),'member-item overload-item')).join('')+'</div>':'')+'</div>';
  }
  function compactSignature(e){
    const args=(e.parameters||[]).map(p=>p.type+(p.default!==undefined&&p.default!==null&&p.default!==''?' = '+p.default:'')).join(', ');
    if(functionKinds.has(e.kind))return '('+args+')'+(/\)\s*const\b/.test(e.signature||'')?' const':'')+(e.returnType?' → '+e.returnType:'');
    return e.signature||typeLabel(e.kind);
  }
  function memberSections(type,hits=null){
    const members=hits?hits.filter(e=>e.id!==type.id):membersOf(type);
    if(!members.length)return '<div class="tree-empty">No '+(state.internal?'':'public ')+'members in this view.</div>';
    if(hits)return '<div class="tree-section-label">MATCHING MEMBERS <span>'+members.length+'</span></div>'+groupMembers(members).map(f=>familyRows(f,true,true)).join('');
    const groups=[['Construction',members.filter(e=>['constructor','destructor'].includes(e.kind))],['Methods',members.filter(e=>functionKinds.has(e.kind)&&!['constructor','destructor'].includes(e.kind))],['Fields & values',members.filter(e=>valueKinds.has(e.kind))],['Nested types',members.filter(e=>typeKinds.has(e.kind))]];
    return groups.filter(g=>g[1].length).map(([title,list])=>'<div class="tree-section-label">'+esc(title)+' <span>'+groupMembers(list).length+'</span></div>'+groupMembers(list).map(f=>typeKinds.has(f.entry.kind)?typeBrowserRow(f.entry,null,false,true):familyRows(f,true,false)).join('')).join('');
  }
  function typeBrowserRow(e,hits=null,search=false,nested=false){
    const visibleMembers=membersOf(e),matching=hits?.filter(h=>h.id!==e.id)||[];
    const open=!collapsedTypes.has(String(e.id))&&(expandedTypes.has(String(e.id))||(search&&matching.length>0));
    const count=groupMembers(visibleMembers).length;
    const isActive=byId.get(String(state.selected))?.id===e.id;
    return '<div class="type-tree'+(nested?' nested-type':'')+'"><div class="type-tree-row'+(isActive?' current':'')+'"><button class="tree-toggle" data-type-toggle="'+esc(e.id)+'" aria-label="'+(open?'Collapse ':'Expand ')+esc(qualified(e))+'" aria-expanded="'+open+'"'+(!visibleMembers.length?' disabled':'')+'><span class="chevron'+(open?' open':'')+'">›</span></button>'+browserLink(e,e.name,typeLabel(e.kind)+(callableType(e)&&e.kind==='struct'?' · interface / behavior':'')+' · '+count+' member group'+(count===1?'':'s'),'type-link')+'</div>'+(open?'<div class="type-children">'+memberSections(e,search&&matching.length?matching:null)+'</div>':'')+'</div>';
  }
  function searchGroups(){
    const terms=state.query.toLowerCase().trim().split(/\s+/).filter(Boolean);
    const eligible=entries.filter(e=>publicEntry(e)&&(state.layer==='all'||e.layer===state.layer));
    const exactTypes=eligible.filter(e=>typeKinds.has(e.kind)&&(String(e.name).toLowerCase()===state.query.toLowerCase().trim()||qualified(e).toLowerCase()===state.query.toLowerCase().trim()));
    const matches=exactTypes.length?exactTypes:eligible.filter(e=>{
      const text=(qualified(e)+' '+(e.signature||'')+' '+(e.description||'')+' '+(e.path||'')+' '+(e.subsystem||'')).toLowerCase();return terms.every(t=>text.includes(t));
    });
    const groups=new Map();
    matches.forEach(e=>{
      const root=rootOf(e);if(!publicEntry(root))return;
      const key=typeKinds.has(root.kind)?root.id:familyKey(root);
      if(!groups.has(key))groups.set(key,{entry:root,hits:[],score:0});
      const g=groups.get(key);g.hits.push(e);
      const n=String(e.name).toLowerCase(),q=qualified(e).toLowerCase();
      const score=terms.reduce((sum,t)=>sum+(n===t?120:n.startsWith(t)?60:n.includes(t)?40:q.includes(t)?15:3),0);
      g.score=Math.max(g.score,score+(typeKinds.has(e.kind)&&exactTypes.length?20:0));
    });
    return [...groups.values()].sort((a,b)=>b.score-a.score||qualified(a.entry).localeCompare(qualified(b.entry)));
  }
  function catalogGroups(kind,layer=state.layer){
    const list=rootEntries().filter(e=>publicEntry(e)&&(layer==='all'||e.layer===layer)&&categoryOf(e)===kind);
    return groupMembers(list).map(f=>({entry:f.entry,hits:f.items,score:0,family:f})).sort((a,b)=>qualified(a.entry).localeCompare(qualified(b.entry)));
  }
  function indexRenderedLinks(){
    const links=[...$('symbol-list').querySelectorAll('a.symbol-item')].filter(a=>!a.closest('details:not([open])'));
    currentResults=links.map((a,i)=>{a.dataset.resultIndex=i;return {_href:a.getAttribute('href')};});
  }
  function renderList(){
    let mode=['guides','guide'].includes(state.route)?'guides':['files','file'].includes(state.route)?'files':'symbols';
    if(state.query)mode='symbols';
    document.querySelector('.symbol-filters').style.display=mode==='symbols'?'grid':'none';
    document.querySelector('.symbol-options label').style.display=mode==='symbols'?'flex':'none';
    $('scope-eyebrow').textContent=state.query?'GROUPED SEARCH':mode==='guides'?'READ THE SYSTEM':mode==='files'?'SOURCE BROWSER':'TYPE BROWSER';
    $('scope-title').textContent=state.query?'Search results':mode==='guides'?'Guides & architecture':mode==='files'?'Source files':state.layer==='all'?'Native API':layerTitle(state.layer);
    $('search-all-layers').hidden=!(state.query&&state.layer!=='all');
    const search=!!state.query.trim();
    document.querySelectorAll('[data-kind]').forEach(button=>{
      const active=!search&&button.dataset.kind===state.kind;button.classList.toggle('active',active);button.setAttribute('aria-pressed',String(active));
      const count=button.querySelector('.category-count');if(count)count.textContent=number(catalogGroups(button.dataset.kind).length);
    });
    if(mode!=='symbols'){
      const list=mode==='guides'?guides.map(g=>({...g,kind:'guide',_href:guideHref(g),_context:g.path||'',_name:g.title})):files.filter(f=>state.layer==='all'||f.layer===state.layer).sort((a,b)=>a.path.localeCompare(b.path)).map(f=>({...f,kind:'file',_href:fileHref(f),_context:f.path,_name:f.name||f.path.split('/').pop()}));
      $('result-count').textContent=number(list.length)+' '+mode;
      $('symbol-list').innerHTML=list.map(e=>'<a class="symbol-item" href="'+esc(e._href)+'">'+kindIcon(e.kind)+'<span class="symbol-text"><span class="symbol-name">'+esc(e._name)+'</span><span class="symbol-context">'+esc(e._context)+'</span></span></a>').join('');indexRenderedLinks();return;
    }
    const groups=search?searchGroups():catalogGroups(state.kind);
    const hitCount=search?new Set(groups.flatMap(g=>g.hits.map(e=>e.id))).size:0;
    const resultLabel=state.kind==='classes'?(groups.length===1?'class':'classes'):state.kind==='data'?(groups.length===1?'data type':'data types'):state.kind==='functions'?(groups.length===1?'function group':'function groups'):(groups.length===1?'constant':'constants');
    $('result-count').textContent=search?number(groups.length)+(groups.length===1?' group · ':' groups · ')+number(hitCount)+(hitCount===1?' match':' matches'):number(groups.length)+' '+resultLabel;
    if(!groups.length){$('symbol-list').innerHTML='<div class="list-empty"><strong>No matching '+(search?'symbols':categoryTitles[state.kind].toLowerCase())+'.</strong>'+(search?'Try a class or method name. Search includes members and all four categories.':'Choose another category or layer. Internal declarations are available with the Internals toggle.')+'</div>';currentResults=[];return;}
    const displayed=groups.slice(0,state.limit),namespaces=new Map();
    displayed.forEach(g=>{const ns=g.entry.namespace||'Global namespace';if(!namespaces.has(ns))namespaces.set(ns,[]);namespaces.get(ns).push(g);});
    let html=search?'<div class="search-context-note">All categories · '+esc(state.layer==='all'?'all layers':layerTitle(state.layer))+'</div>':'';
    for(const [namespace,items] of namespaces){
      const open=search||!collapsedNamespaces.has(namespace);
      html+='<details class="namespace-group" data-namespace="'+esc(namespace)+'"'+(open?' open':'')+'><summary><span class="chevron">›</span><span class="namespace-name" title="'+esc(namespace)+'">'+esc(namespace)+'</span><span class="namespace-count">'+items.length+'</span></summary><div class="namespace-content">';
      html+=items.map(g=>typeKinds.has(g.entry.kind)?typeBrowserRow(g.entry,search?g.hits:null,search):familyRows(g.family||{key:familyKey(g.entry),items:g.hits,entry:g.entry,label:g.entry.name},false,search)).join('');
      html+='</div></details>';
    }
    if(groups.length>state.limit)html+='<button class="load-more" id="load-more">Show '+number(Math.min(120,groups.length-state.limit))+' more · '+number(groups.length-state.limit)+' remaining</button>';
    $('symbol-list').innerHTML=html;
    $('load-more')?.addEventListener('click',()=>{const y=$('symbol-list').scrollTop;state.limit+=120;renderList();$('symbol-list').scrollTop=y;});
    $('symbol-list').querySelectorAll('.namespace-group').forEach(details=>details.addEventListener('toggle',()=>{const ns=details.dataset.namespace;if(details.open)collapsedNamespaces.delete(ns);else collapsedNamespaces.add(ns);indexRenderedLinks();}));
    indexRenderedLinks();
  }
  function overview(){
    const visible=entries.filter(e=>!e.internal&&!['private','protected'].includes(e.access));
    const functions=catalogGroups('classes','all').length;
    const types=catalogGroups('data','all').length;
    const freeGroups=catalogGroups('functions','all').length;
    const headerCount=files.filter(f=>/\.(h|hh|hpp|hxx)$/.test(f.path||'')).length;
    const find=(name)=>entries.find(e=>e.name===name&&typeKinds.has(e.kind));
    const cards=[
      {title:'Follow a native session',description:'Start with the application layer and see how engines, durable evidence, and operator state fit together.',entry:find('NativeSessionApplication'),layer:'application'},
      {title:'Explore the engine interface',description:'Inspect lifecycle methods, capability records, checkpointing, and the contracts an engine implements.',entry:find('ISimulationEngine'),layer:'contracts'},
      {title:'Find the domain logic',description:'Browse the services responsible for simulation, experiments, projection, optimization, and authorization.',layer:'services'},
      {title:'Inspect the authenticated mesh',description:'Trace the latest mesh work through enrollment, bounded transport, replay, and receipt handling.',entry:find('NativeMeshApplication')||find('MeshIngressService'),layer:'infrastructure'}
    ];
    mount(breadcrumb([{title:'Overview'}])+'<div class="hero-top"><div class="page-kicker"><span class="rule"></span> CHRISTEYE ANAIERO / C++20</div><h1>Anaiero native reference.</h1><p class="lead">Explore the functions, types, and services behind Anaiero. Search by name or behavior, read the interface, and follow it straight to the implementation.</p><div class="snapshot-bar"><span class="chip accent">SOURCE SNAPSHOT</span><code>'+esc(M.branch||'latest native branch')+'</code><a href="'+repoURL+'/commit/'+esc(M.commit||'HEAD')+'" target="_blank" rel="noopener noreferrer">'+esc(String(M.commit||'').slice(0,7))+' ↗</a></div></div><div class="stats"><div><div class="stat-value">'+number(headerCount)+'</div><div class="stat-label">Library headers</div></div><div><div class="stat-value">'+number(functions)+'</div><div class="stat-label">Classes & interfaces</div></div><div><div class="stat-value">'+number(types)+'</div><div class="stat-label">Data types</div></div><div><div class="stat-value">'+number(freeGroups)+'</div><div class="stat-label">Free-function groups</div></div></div><p class="index-coverage">Organized from <strong>'+number(rawEntries.length)+' source declarations</strong> across '+number(layers.length)+' layers. Constructors, methods, fields, and overloads live under their owning type.</p><div class="section-heading"><h2>A few places to begin</h2><a class="small-link" href="#guides">Read the guides ↗</a></div><div class="starting-grid">'+cards.map((c,i)=>'<a class="start-card" href="'+(c.entry?symbolHref(c.entry):'#layer/'+c.layer)+'"><span class="card-number">0'+(i+1)+' / EXPLORE</span><span class="arrow">↗</span><h3>'+c.title+'</h3><p>'+c.description+'</p></a>').join('')+'</div><div class="section-heading"><h2>Navigate the architecture</h2><span class="small-link">Types by source layer</span></div><div class="layer-table">'+layers.map(l=>'<div class="layer-row"><a href="#layer/'+esc(l.id)+'">'+esc(l.title)+' ↗</a><p>'+esc(l.description||'')+'</p><span>'+number(countLayer(l.id))+'</span></div>').join('')+'</div><div class="notice"><strong>Read the implementation with its context.</strong> Signatures and source come from this pinned commit. Reviewed explanations are labeled; declaration-only entries remain linked to their exact source. Anaiero’s native runtime includes digital research models and optional scientific integrations, with their current boundaries documented in the guides.</div>');
    document.title='Anaiero · Native Reference';
  }
  function membersOf(entry){
    const found=[...(entry.memberIds||[]).map(id=>byId.get(String(id))).filter(Boolean),...(symbolsByParent.get(String(entry.id))||[])];
    return [...new Map(found.map(e=>[e.id,e])).values()].filter(publicEntry).sort((a,b)=>String(a.name).localeCompare(String(b.name)));
  }
  function memberEntry(e,showNamespace=false){
    return '<div class="detail-member"><div class="detail-member-heading">'+kindIcon(e.kind)+'<a class="member-name" href="'+symbolHref(e)+'">'+esc(lifecycleName(e))+'</a>'+(['private','protected'].includes(e.access)?'<span class="chip">'+esc(e.access)+'</span>':'')+(e._declarations.length>1?'<span class="chip">'+e._declarations.length+' source locations</span>':'')+'</div>'+(showNamespace?'<div class="member-namespace">'+esc(e.namespace||'')+'</div>':'')+'<div class="member-sig">'+esc(e.signature||qualified(e))+'</div>'+(e.description?'<p class="member-desc">'+inlineCode(e.description)+'</p>':'')+'</div>';
  }
  function memberTable(members){
    const unique=[...new Map(members.map(e=>[e.id,e])).values()];
    return '<div class="member-collection">'+groupMembers(unique).map(f=>{
      if(f.items.length===1)return memberEntry(f.items[0],!ownerOf(f.items[0]));
      return '<details class="detail-overload"><summary>'+kindIcon(f.entry.kind)+'<span>'+esc(f.label)+'</span><span class="chip">'+f.items.length+' overloads</span><span class="detail-expand">Show signatures <span class="chevron">›</span></span></summary><div class="detail-overload-items">'+f.items.map(e=>memberEntry(e)).join('')+'</div></details>';
    }).join('')+'</div>';
  }
  function relatedType(name){const cleaned=String(name).replace(/^(public|protected|private)\s+/,'').trim();return byQualified.get(cleaned)||entries.find(e=>e.name===cleaned&&typeKinds.has(e.kind));}
  function renderSymbol(entry,keepScroll=false){
    if(!entry){missing('This symbol could not be found.');return;}
    document.title=(entry.qualified||entry.name)+' · Anaiero Reference';
    entry=byId.get(String(entry.id))||entry;
    const parent=ownerOf(entry);
    const peers=entries.filter(e=>e.id!==entry.id&&familyKey(e)===familyKey(entry)&&publicEntry(e));
    const f=fileByPath.get(entry.path);
    const members=membersOf(entry);
    const params=entry.parameters||[];
    const description=entry.description||entry.comment||'';
    const details=Array.isArray(entry.details)?entry.details:entry.details?[entry.details]:[];
    const parts=[{title:layerTitle(entry.layer),href:'#layer/'+entry.layer}];
    if(parent)parts.push({title:parent.name,href:symbolHref(parent)});
    if(entry.namespace)parts.splice(1,0,{title:entry.namespace});
    parts.push({title:lifecycleName(entry)});
    let body=breadcrumb(parts)+'<div class="symbol-title-row">'+kindIcon(entry.kind)+'<div><h1>'+esc(lifecycleName(entry))+'</h1><div class="qualified-name">'+esc(qualified(entry))+'</div></div></div><div class="symbol-badges"><span class="chip accent">'+esc(typeLabel(entry.kind))+'</span><span class="chip">'+esc(entry.access||'public')+'</span>'+(entry.modifiers||[]).map(v=>'<span class="chip">'+esc(v)+'</span>').join('')+'<span class="chip">'+(entry.descriptionSource==='reviewed'?'Reviewed explanation':entry.descriptionSource==='implementation'?'From implementation':entry.descriptionSource==='comment'||entry.comment?'Source comment':'Source declaration')+'</span></div>'+(description?'<p class="symbol-description">'+inlineCode(description)+'</p>':'<p class="muted-copy">The declaration and implementation for this symbol are included below. No separate explanatory comment was provided in the source.</p>')+'<div class="doc-tabs"><button class="doc-tab'+(state.tab==='reference'?' active':'')+'" data-tab="reference">Reference'+(members.length?'<span>'+groupMembers(members).length+' member group'+(groupMembers(members).length===1?'':'s')+'</span>':'')+'</button><button class="doc-tab'+(state.tab==='source'?' active':'')+'" data-tab="source">Implementation</button><button class="doc-tab" data-copy-link="true">Copy link ↗</button></div>';
    if(state.tab==='source'){
      const variants=entry._declarations||[entry];
      body+=variants.map((variant,i)=>'<h2 class="source-variant-title">'+(variants.length>1?'Source location '+(i+1):'Implementation')+'</h2>'+codeBlock(variant.source||variant.signature||'No source captured.',variant.path+':'+variant.line,{lines:true,start:variant.line||1,url:sourceURL(variant.path,variant.line,variant.endLine)})).join('');
      if(f)body+='<p class="muted-copy"><a href="'+fileHref(f)+'">Browse complete source file →</a></p>';
    }else{
      body+=codeBlock(entry.signature||qualified(entry),'C++ DECLARATION',{url:sourceURL(entry.path,entry.line)});
      if(details.length)body+='<h2>Behavior</h2><ul class="details-list">'+details.map(d=>'<li>'+inlineCode(typeof d==='string'?d:JSON.stringify(d))+'</li>').join('')+'</ul>';
      if((entry.baseTypes||[]).length)body+='<h2>Inheritance</h2><div class="inheritance">'+entry.baseTypes.map(b=>{const text=typeof b==='string'?b:b.name||b.type||JSON.stringify(b);const target=relatedType(text);return target?'<a class="chip" href="'+symbolHref(target)+'">'+esc(text)+' ↗</a>':'<code class="chip">'+esc(text)+'</code>';}).join('')+'</div>';
      if(params.length)body+='<h2>Parameters</h2><div class="table-scroll"><table class="doc-table"><thead><tr><th>Name</th><th>Type</th><th>Notes / default</th></tr></thead><tbody>'+params.map((p,i)=>'<tr><td><code class="param-name">'+esc(p.name||'argument '+(i+1))+'</code></td><td><code>'+esc(p.type||p.declaration||'—')+'</code></td><td>'+inlineCode(p.description||p.meaning||'')+(p.default!==undefined&&p.default!==null&&p.default!==''?((p.description||p.meaning)?'<br>':'')+'Default: <code>'+esc(p.default)+'</code>':'')+(!(p.description||p.meaning)&&!(p.default!==undefined&&p.default!==null&&p.default!=='')?'<span aria-label="No additional parameter comment">—</span>':'')+'</td></tr>').join('')+'</tbody></table></div>';
      if(entry.returnType||entry.returns)body+='<h2>Returns</h2><div class="return-row">'+(entry.returnType?'<code>'+esc(entry.returnType)+'</code>':'')+inlineCode(typeof entry.returns==='string'?entry.returns:'')+'</div>';
      if(members.length){
        const groups=[['Construction & lifecycle',members.filter(e=>['constructor','destructor'].includes(e.kind))],['Methods',members.filter(e=>functionKinds.has(e.kind)&&!['constructor','destructor'].includes(e.kind))],['Data members & values',members.filter(e=>valueKinds.has(e.kind))],['Nested types',members.filter(e=>typeKinds.has(e.kind))]];
        const grouped=new Set(groups.flatMap(g=>g[1].map(e=>e.id)));
        const other=members.filter(e=>!grouped.has(e.id));if(other.length)groups.push(['Members',other]);
        body+=groups.filter(g=>g[1].length).map(([title,items])=>'<h2 class="section-label">'+title+'<span class="chip">'+groupMembers(items).length+(groupMembers(items).length===1?' group':' groups')+'</span></h2>'+memberTable(items)).join('');
      }
      const implementations=(entry.implementedByIds||[]).map(id=>byId.get(String(id))).filter(Boolean).filter(publicEntry);
      const related=(entry.relatedIds||[]).map(id=>byId.get(String(id))).filter(Boolean).filter(publicEntry);
      if(implementations.length)body+='<h2>Implementations</h2>'+memberTable(implementations);
      if(related.length)body+='<h2>Related symbols</h2>'+memberTable(related);
      if(peers.length)body+='<h2>Other overloads</h2>'+memberTable(peers);
      if(entry._declarations.length>1)body+='<h2>Source locations</h2><ul class="source-locations">'+entry._declarations.map(v=>'<li><a href="'+sourceURL(v.path,v.line,v.endLine)+'" target="_blank" rel="noopener noreferrer">'+esc(v.path)+':'+v.line+' ↗</a><code>'+esc(v.signature||'')+'</code></li>').join('')+'</ul>';
      body+='<h2>Defined in</h2><div class="source-note"><a href="'+sourceURL(entry.path,entry.line,entry.endLine)+'" target="_blank" rel="noopener noreferrer">'+esc(entry.path)+':'+esc(entry.line||1)+' ↗</a>'+(f?'<a href="'+fileHref(f)+'">Browse source file →</a>':'')+'</div>';
      if(!members.length&&entry.source&&entry.source.trim()!==(entry.signature||'').trim())body+=codeBlock(entry.source,'IMPLEMENTATION',{lines:true,start:entry.line||1});
      if(parent)body+='<p class="muted-copy">Member of <a href="'+symbolHref(parent)+'">'+esc(qualified(parent))+'</a>.</p>';
    }
    mount(body,keepScroll);
  }
  function renderBrowse(){
    const cards=[['classes','Classes & interfaces','Services, engines, adapters, and callable types. Expand a class to browse its members.'],['data','Data types','Passive records, enums, aliases, and the values carried across interfaces.'],['functions','Free functions','Namespace-level helpers, grouped by function name and overload signature.'],['constants','Constants','Namespace-level constants and configuration values.']];
    mount(breadcrumb([{title:'Browse native API'}])+'<div class="page-kicker"><span class="rule"></span> ORGANIZED BY OWNERSHIP</div><h1>Find the class. Follow its members.</h1><p class="lead">The browser groups the library by namespace and owning type. Open a class for its complete reference, or use the chevron to reveal construction, methods, fields, and nested types.</p><div class="browse-category-grid">'+cards.map(([id,title,description])=>'<button class="browse-category-card" data-browse-category="'+id+'"><span class="stat-value">'+number(catalogGroups(id,'all').length)+'</span><h3>'+title+'</h3><p>'+description+'</p></button>').join('')+'</div><div class="notice"><strong>One entry per entity.</strong> Constructors and destructors stay inside their type. Overloads expand into signatures, and repeated declarations share one reference with each source location preserved. Search still reaches every member.</div>');
    document.title='Browse native API · Anaiero Reference';
  }
  function renderLayer(id){
    const layer=layerMap.get(id)||{title:id,description:''};
    const layerFiles=files.filter(f=>f.layer===id);
    const all=['classes','data','functions','constants'].map(kind=>[kind,catalogGroups(kind,id)]);
    const rootCount=all.reduce((sum,[,groups])=>sum+groups.length,0);
    mount(breadcrumb([{title:layer.title}])+'<div class="page-kicker"><span class="rule"></span> LIBRARY LAYER</div><h1>'+esc(layer.title)+'</h1><p class="lead">'+esc(layer.description||'Explore the declarations and implementations in this layer.')+'</p><div class="snapshot-bar"><span class="chip">'+number(rootCount)+' entries</span><span class="chip">'+number(layerFiles.length)+' files</span></div>'+all.filter(([,g])=>g.length).map(([kind,groups])=>'<h2 class="section-label">'+categoryTitles[kind]+'<span class="chip">'+groups.length+' groups</span></h2>'+memberTable(groups.flatMap(g=>g.family.items))).join(''));
    document.title=layer.title+' · Anaiero Reference';
  }
  function fileRows(list){return '<div class="file-grid">'+list.map(f=>'<a class="file-row" href="'+fileHref(f)+'">'+kindIcon('file')+'<div><div class="file-name">'+esc(f.name||f.path.split('/').pop())+'</div>'+(f.description?'<div class="file-desc">'+inlineCode(f.description)+'</div>':'')+'<div class="file-path">'+esc(f.path)+'</div></div><span class="file-counter">'+number((f.symbolIds||[]).length)+' symbols ↗</span></a>').join('')+'</div>';}
  function renderFiles(){mount(breadcrumb([{title:'Source files'}])+'<div class="page-kicker"><span class="rule"></span> PINNED IMPLEMENTATION</div><h1>Browse the source.</h1><p class="lead">Every file shown here belongs to the same native source snapshot. Open a file to inspect its code and jump into the symbols it defines.</p>'+fileRows(files.slice().sort((a,b)=>a.path.localeCompare(b.path))));document.title='Source files · Anaiero Reference';}
  function renderFile(file,keepScroll=false){
    if(!file){missing('This source file could not be found.');return;}
    const symbols=[...new Map((file.symbolIds||[]).map(id=>byId.get(String(id))).filter(Boolean).map(e=>[e.id,e])).values()].filter(publicEntry);
    const top=symbols.filter(e=>!ownerOf(e));
    let html=breadcrumb([{title:'Source files',href:'#files'},{title:file.name||file.path.split('/').pop()}])+'<div class="page-kicker"><span class="rule"></span> SOURCE FILE</div><h1 class="mono" style="font-size:25px;overflow-wrap:anywhere">'+esc(file.name||file.path.split('/').pop())+'</h1><p class="qualified-name">'+esc(file.path)+'</p>'+(file.description?'<p class="muted-copy">'+inlineCode(file.description)+'</p>':'')+'<div class="doc-tabs"><button class="doc-tab'+(state.tab==='reference'?' active':'')+'" data-tab="reference">Symbols <span>'+symbols.length+'</span></button><button class="doc-tab'+(state.tab==='source'?' active':'')+'" data-tab="source">Complete source</button><a class="doc-tab" href="'+sourceURL(file.path)+'" target="_blank" rel="noopener noreferrer">GitHub ↗</a></div>';
    if(state.tab==='source'||!top.length)html+=codeBlock(file.source||'No source text available.',file.path,{lines:true,start:1,url:sourceURL(file.path)});
    else html+=memberTable(top)+'<p class="muted-copy">Nested members are grouped under their owning type. Use <button class="text-button" data-tab="source" style="font-size:inherit;color:var(--accent)">Complete source</button> to read the entire file.</p>';
    mount(html,keepScroll);document.title=(file.name||file.path)+' · Anaiero Reference';
  }
  function renderGuides(){mount(breadcrumb([{title:'Guides & architecture'}])+'<div class="page-kicker"><span class="rule"></span> THE SYSTEM IN CONTEXT</div><h1>Guides & architecture.</h1><p class="lead">Read the native implementation notes alongside the API. These guides come from the same branch and commit as the reference.</p><div class="guide-grid">'+guides.map((g,i)=>'<a class="guide-card" href="'+guideHref(g)+'"><span class="eyebrow">'+String(i+1).padStart(2,'0')+' / GUIDE</span><h3>'+esc(g.title)+'</h3><p>'+inlineCode(g.description||g.path||'')+'</p></a>').join('')+'</div>');document.title='Guides · Anaiero Reference';}
  function safeGuide(html,path){
    const t=document.createElement('template');t.innerHTML=html;
    t.content.querySelectorAll('script,iframe,object,embed,form,input,button,style,link,meta,base').forEach(el=>el.remove());
    t.content.querySelectorAll('*').forEach(el=>{[...el.attributes].forEach(a=>{if(a.name.startsWith('on')||a.name==='srcdoc'||(a.name==='href'||a.name==='src')&&/^\s*(javascript|data):/i.test(a.value))el.removeAttribute(a.name);});});
    t.content.querySelectorAll('a[href]').forEach(a=>{
      const href=a.getAttribute('href');
      if(href.startsWith('#'))return;
      if(!/^(https?:|mailto:)/.test(href)){
        const parts=(path||'').split('/');parts.pop();href.split('/').forEach(s=>{if(s==='..')parts.pop();else if(s!=='.')parts.push(s);});
        const rel=parts.join('/');const matching=guides.find(g=>g.path===rel.split('#')[0]);
        a.setAttribute('href',matching?guideHref(matching):sourceURL(rel));
      }
      if(/^https?:/.test(a.href)){a.target='_blank';a.rel='noopener noreferrer';}
    });
    return t.innerHTML;
  }
  function renderGuide(guide){if(!guide){missing('This guide could not be found.');return;}mount(breadcrumb([{title:'Guides & architecture',href:'#guides'},{title:guide.title}])+'<div class="source-note"><span class="chip accent">REPOSITORY GUIDE</span><a href="'+sourceURL(guide.path)+'" target="_blank" rel="noopener noreferrer">Read on GitHub ↗</a></div><div class="prose">'+(guide.html?safeGuide(guide.html,guide.path):'<h1>'+esc(guide.title)+'</h1><p>'+inlineCode(guide.description||'')+'</p>')+'</div>');document.title=guide.title+' · Anaiero Reference';}
  function missing(message){mount('<div class="empty-page"><span class="eyebrow">REFERENCE LOOKUP</span><h1>'+esc(message)+'</h1><p>The link may belong to another source snapshot. Use the search field or <a href="#overview">return to the overview</a>.</p></div>');}
  function renderCurrent(keep=false){if(state.route==='overview')overview();else if(state.route==='browse')renderBrowse();else if(state.route==='symbol')renderSymbol(byId.get(String(state.selected)),keep);else if(state.route==='layer')renderLayer(state.layer);else if(state.route==='files')renderFiles();else if(state.route==='file')renderFile(fileById.get(String(state.selected)),keep);else if(state.route==='guides')renderGuides();else if(state.route==='guide')renderGuide(guideById.get(String(state.selected)));else missing('This page could not be found.');}
  function readRoute(){
    const raw=location.hash.replace(/^#/,'')||'overview';const [route,...rest]=raw.split('/');let id;try{id=decodeURIComponent(rest.join('/'));}catch(_){id=rest.join('/');}
    const last=state.selected;state.route=route;state.selected=id||null;if(last!==state.selected)state.tab='reference';
    if(route==='symbol'){
      const e=byId.get(String(id));if(e){state.selected=e.id;const root=rootOf(e);collapsedNamespaces.delete(root.namespace||'Global namespace');if(!state.query){state.layer=e.layer;state.kind=categoryOf(root);}if(ownerOf(e)){let ancestor=ownerOf(e);while(ancestor){expandedTypes.add(String(ancestor.id));collapsedTypes.delete(String(ancestor.id));ancestor=ownerOf(ancestor);}expandedFamilies.add(familyKey(e));collapsedFamilies.delete(familyKey(e));}}
    }else if(route==='layer'){state.layer=id;state.query='';$('search').value='';state.kind=['classes','data','functions','constants'].find(kind=>catalogGroups(kind,id).length)||'classes';state.limit=120;}
    else if(['overview','browse','guides','files'].includes(route)){state.layer='all';state.query='';$('search').value='';state.kind='classes';state.limit=120;}
    else if(route==='file')state.layer='all';
    else if(byId.has(raw)){state.route='symbol';state.selected=raw;}
    closePanels();updateNav();renderList();renderCurrent();
  }
  $('head-sha').textContent=String(M.commit||'').slice(0,7)||'snapshot';
  $('branch-name').textContent=M.branch||'Native implementation';
  $('github-top').href=repoURL+'/tree/'+(M.commit||M.branch||'HEAD');
  $('file-count').textContent=number(files.length);
  const symbols=['◇','ƒ','⇄','▦','◫','◎','≋','⌁'];
  $('layer-nav').innerHTML=layers.map((l,i)=>'<button class="nav-link" data-layer="'+esc(l.id)+'"><span class="nav-icon">'+symbols[i%symbols.length]+'</span>'+esc(l.title)+'<span class="nav-count" data-layer-count="'+esc(l.id)+'" title="Classes, interfaces, and data types"></span></button>').join('');
  document.querySelectorAll('[data-route]').forEach(button=>button.addEventListener('click',()=>navigate(button.dataset.route)));
  document.querySelectorAll('[data-layer]').forEach(button=>button.addEventListener('click',()=>navigate(button.dataset.layer==='all'?'browse':'layer/'+button.dataset.layer)));
  document.querySelectorAll('[data-kind]').forEach(button=>button.addEventListener('click',()=>{state.kind=button.dataset.kind;state.query='';$('search').value='';state.limit=120;renderList();$('symbol-list').scrollTop=0;}));
  $('search-all-layers').addEventListener('click',()=>{state.layer='all';state.limit=120;renderList();updateNav();$('symbol-list').scrollTop=0;});
  $('show-internal').addEventListener('change',e=>{state.internal=e.target.checked;state.limit=120;renderList();updateNav();if(['symbol','layer','file'].includes(state.route))renderCurrent(true);});
  $('search').addEventListener('input',e=>{const hadQuery=!!state.query;state.query=e.target.value;if(state.query&&!hadQuery)state.layer='all';state.limit=120;state.activeIndex=-1;renderList();updateNav();$('symbol-list').scrollTop=0;if(state.query&&matchMedia('(max-width:740px)').matches){document.body.classList.remove('nav-open');document.body.classList.add('browse-open');}});
  $('search').addEventListener('keydown',e=>{
    if(['ArrowDown','ArrowUp'].includes(e.key)){e.preventDefault();state.activeIndex=Math.max(0,Math.min(currentResults.length-1,state.activeIndex+(e.key==='ArrowDown'?1:-1)));const item=document.querySelector('[data-result-index="'+state.activeIndex+'"]');if(item){item.focus();item.scrollIntoView({block:'nearest'});}}
    else if(e.key==='Enter'&&currentResults.length){e.preventDefault();navigate(currentResults[Math.max(0,state.activeIndex)]._href.slice(1));$('search').blur();}
  });
  $('symbol-list').addEventListener('keydown',e=>{if(['ArrowDown','ArrowUp'].includes(e.key)){const i=Number(e.target.closest('[data-result-index]')?.dataset.resultIndex);if(!Number.isFinite(i))return;e.preventDefault();const next=Math.max(0,Math.min(currentResults.length-1,i+(e.key==='ArrowDown'?1:-1)));document.querySelector('[data-result-index="'+next+'"]')?.focus();state.activeIndex=next;}});
  document.addEventListener('keydown',e=>{
    if((e.key==='/'&&!['INPUT','TEXTAREA'].includes(document.activeElement.tagName))||((e.metaKey||e.ctrlKey)&&e.key.toLowerCase()==='k')){e.preventDefault();$('search').focus();$('search').select();}
    if(e.key==='Escape'){if(state.query){state.query='';$('search').value='';renderList();}closePanels();$('search').blur();}
  });
  document.addEventListener('click',e=>{
    const category=e.target.closest('[data-browse-category]');if(category){state.kind=category.dataset.browseCategory;state.query='';$('search').value='';renderList();if(matchMedia('(max-width:740px)').matches)document.body.classList.add('browse-open');return;}
    const typeToggle=e.target.closest('[data-type-toggle]');
    if(typeToggle){const id=typeToggle.dataset.typeToggle;if(typeToggle.getAttribute('aria-expanded')==='true'){expandedTypes.delete(id);collapsedTypes.add(id);}else{expandedTypes.add(id);collapsedTypes.delete(id);}const y=$('symbol-list').scrollTop;renderList();$('symbol-list').scrollTop=y;return;}
    const familyToggle=e.target.closest('[data-family-toggle]');
    if(familyToggle){const key=familyToggle.dataset.familyToggle;if(familyToggle.getAttribute('aria-expanded')==='true'){expandedFamilies.delete(key);collapsedFamilies.add(key);}else{expandedFamilies.add(key);collapsedFamilies.delete(key);}const y=$('symbol-list').scrollTop;renderList();$('symbol-list').scrollTop=y;return;}
    const tab=e.target.closest('[data-tab]');if(tab){state.tab=tab.dataset.tab;renderCurrent(true);}
    const cp=e.target.closest('[data-copy-target]');if(cp){const template=$(cp.dataset.copyTarget);if(template)copy(template.content.textContent);}
    if(e.target.closest('[data-copy-link]'))copy(location.href);
    if(e.target.closest('.symbol-item'))closePanels();
  });
  $('theme-toggle').addEventListener('click',()=>{const theme=document.documentElement.dataset.theme==='dark'?'light':'dark';document.documentElement.dataset.theme=theme;try{localStorage.setItem('anaiero-theme',theme);}catch(_){}$('theme-toggle').setAttribute('aria-label','Switch to '+(theme==='dark'?'light':'dark')+' theme');});
  $('menu-toggle').addEventListener('click',()=>{document.body.classList.remove('browse-open');document.body.classList.toggle('nav-open');$('menu-toggle').setAttribute('aria-expanded',String(document.body.classList.contains('nav-open')));});
  $('browse-toggle').addEventListener('click',()=>{document.body.classList.remove('nav-open');document.body.classList.toggle('browse-open');});
  $('close-browser').addEventListener('click',closePanels);$('scrim').addEventListener('click',closePanels);
  window.addEventListener('hashchange',readRoute);
  readRoute();
})();
