import fs from 'node:fs';
import path from 'node:path';
const root=path.resolve('public/content');
const manifest=JSON.parse(fs.readFileSync(path.join(root,'manifest.json'),'utf8'));
const modules=JSON.parse(fs.readFileSync(path.join(root,'modules.json'),'utf8'));
const chips=JSON.parse(fs.readFileSync(path.join(root,'chips.json'),'utf8'));
const molecules=JSON.parse(fs.readFileSync(path.join(root,'molecules.json'),'utf8'));
const release=JSON.parse(fs.readFileSync(path.join(root,'release.json'),'utf8'));
if(release.media_assets<5)throw Error('Scientific figures were not included');
const unique=new Set(manifest.documents.map(x=>x.id));
if(unique.size!==manifest.documents.length)throw Error('Duplicate content IDs');
if(manifest.documents.length<300)throw Error('Incomplete corpus');
if(modules.length!==24)throw Error('Expected 24 scientific module profiles');
if(chips.length<30)throw Error('Insufficient StarChip manifests');
if(molecules.length<20)throw Error('Molecular data missing');
for(const x of manifest.documents){
 if(!fs.existsSync(path.join(root,'documents',x.id+'.json')))throw Error('Missing document: '+x.id);
 if(/patent/i.test(x.source_path)&&/\.(pdf|docx)$/i.test(x.source_path))throw Error('Confidential patent included');
}
const first=manifest.documents.find(x=>x.source_path==='specifications/v3/design/CE-A02_long_term_storage_Design.md');
if(!first)throw Error('Missing A02 source vertical slice');
const doc=JSON.parse(fs.readFileSync(path.join(root,'documents',first.id+'.json'),'utf8'));
if(!doc.content.includes('long-term')&&!doc.content.includes('Long-Term'))throw Error('Wrong A02 content');
console.log('PASS static content checks:',manifest.documents.length,'documents,',modules.length,'modules,',chips.length,'chips,',molecules.length,'molecular entries');
