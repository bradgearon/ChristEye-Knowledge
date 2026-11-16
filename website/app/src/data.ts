import { useEffect, useState } from 'react';
export const BASE=import.meta.env.BASE_URL;
export const REPO='https://github.com/bradgearon/ChristEye-Knowledge';
export type ContentGroup='current-module-design'|'current-module-companion'|'scientific-process'|'research-source'|'schema'|'starchip-manifest'|string;
export interface DocMeta {
 id:string; source_path:string;source_sha:string;title:string;summary:string;
 group:ContentGroup;kind:string;status:string;audiences:string[];
 phase:string|null;planned_route:string;size:number;ext:string;
}
export interface DocBody extends DocMeta {content:string}
export interface Manifest {version:string;copyright:string;generated_from:string;count:number;documents:DocMeta[]}
export interface ModuleProfile {
 id:string;slug:string;substrate:string;axon:string;molecular_questions:string;
 training:string;growth:string;acceptance:string;failure:string;processes:string[];
 status:string;
}
export interface Chip {
 module_id:string;name:string;version:string;stage:string;capabilities:string[];
 integration_refs?:string[]; ports?:Record<string,string[]>;evidence_state?:string
}
export interface Molecule {
 domain:string;name:string;role:string;useClass:string;evidence:string;sources:string
}
export async function getData<T>(file:string):Promise<T>{
 const res=await fetch(BASE+'content/'+file,{cache:'force-cache'});
 if(!res.ok)throw Error('Source is unavailable: '+file+' ('+res.status+')');
 return await res.json() as T;
}
export function useData<T>(url:string){
 const [data,setData]=useState<T|null>(null);
 const [error,setError]=useState<string|null>(null);
 useEffect(()=>{
  let active=true;
  getData<T>(url).then(x=>{if(active){setData(x);setError(null)}})
    .catch(e=>{if(active)setError(String(e))});
  return()=>{active=false};
 },[url]);
 return {data,error,loading:data===null&&!error};
}
export function sourceLink(path:string){return REPO+'/blob/main/'+path.split('/').map(encodeURIComponent).join('/')}
export function formatCount(n:number){return new Intl.NumberFormat('en-US').format(n)}
export function resolveRelative(source:string,href:string){
 const withoutHash=href.split('#')[0].split('?')[0];
 if(!withoutHash)return source;
 const base=source.split('/').slice(0,-1);
 for(const part of withoutHash.split('/')){
  if(part==='..')base.pop();
  else if(part&&part!=='.')base.push(decodeURIComponent(part));
 }
 return base.join('/');
}
export function readRoute(doc:DocMeta){return '/read/'+doc.id}
export const statusLabels:Record<string,string>={
 design:'Design',proposal:'Proposed','source-interpretation':'Research edition',
 'digital-prototype':'Software prototype',reference:'Reference',
 'historical-superseded':'Historical',superseded:'Historical',
 'research hypothesis':'Hypothesis','unvalidated':'Unvalidated'
};
export const groupNames:Record<string,string>={
 'current-module-design':'Engineering design',
 'current-module-companion':'Scientific companion',
 'scientific-process':'Controlled process',
 'scientific-process-index':'Science process guide',
 'research-source':'Research source',
 'research-evidence':'Evidence register',
 schema:'Schema',
 'starchip-manifest':'StarChip manifest',
 historical:'Archive',
 'phase-specification':'Phase specification',
 'phase-hub':'Development phase',
 'software-reference':'Source code',
 'digital-demo':'Runnable example',
 'docs-source':'Reference library',
 governance:'Governance',
 'site-overview':'Overview'
};
