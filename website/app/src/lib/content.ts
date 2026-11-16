import raw from '../generated/catalog.json';
export type DocMeta = {
 id:string;title:string;group:string;kind:string;source:string;code?:string;
 href:string;excerpt:string;status:string;revision:string;sourceHash:string;length:number;readMinutes:number;
};
export type PostMeta = {id:string;title:string;date:string;summary:string;category:string;readMinutes:number;href:string;source:string;sourceHash:string;};
export type FullDoc = DocMeta & { markdown:string };
export type FullPost = PostMeta & { markdown:string };
export const catalog = raw as {version:string;rights:string;documents:DocMeta[];posts:PostMeta[]};
const paths = new Map(catalog.documents.map(d=>[d.source,d]));
export const docById = (id:string) => catalog.documents.find(d=>d.id===id);
export const docByCode = (code:string,kind?:string) => catalog.documents.find(d=>d.code?.toLowerCase()===code.toLowerCase() && (!kind||d.kind===kind));
export const groupDocs = (group:string) => catalog.documents.filter(d=>d.group===group);
export const sourceUrl = (source:string) => 'https://github.com/bradgearon/ChristEye-Knowledge/blob/main/'+source;
export const rawUrl = (source:string) => 'https://raw.githubusercontent.com/bradgearon/ChristEye-Knowledge/main/'+source;
export const mdRef = (source:string,href:string):{url:string;external:boolean} => {
 if(!href || href.startsWith('#'))return {url:href,external:false};
 if(/^https?:\/\//i.test(href))return {url:href,external:true};
 if(/^(mailto:|tel:)/i.test(href))return {url:href,external:true};
 if(href.startsWith('/'))return {url:href,external:false};
 const [path,anchor]=href.split('#');
 const bits=source.split('/').slice(0,-1);
 for(const part of (path||'').split('/')){
  if(!part||part==='.')continue;
  if(part==='..')bits.pop();
  else bits.push(part);
 }
 const joined=bits.join('/');
 const match=paths.get(joined);
 return {url:(match?.href||sourceUrl(joined))+(anchor?'#'+anchor:''),external:!match};
};
export async function loadDoc(id:string):Promise<FullDoc>{
 if(!/^[a-z0-9-]+$/.test(id))throw Error('Invalid document id');
 const res=await fetch('/content/'+encodeURIComponent(id)+'.json');
 if(!res.ok)throw Error('Document unavailable');
 return await res.json() as FullDoc;
}
export async function loadPost(id:string):Promise<FullPost>{
 if(!/^[a-z0-9-]+$/.test(id))throw Error('Invalid post id');
 const res=await fetch('/content/post-'+encodeURIComponent(id)+'.json');
 if(!res.ok)throw Error('Post unavailable');
 return await res.json() as FullPost;
}
export async function deepSearch(query:string):Promise<Set<string>>{
 const res=await fetch('/content/search-index.json');
 if(!res.ok)return new Set();
 const index=await res.json() as {id:string;search:string}[];
 const terms=query.trim().toLowerCase().split(/\s+/).filter(Boolean);
 return new Set(index.filter(d=>terms.every(t=>d.search.includes(t))).map(d=>d.id));
}
