import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import { Link } from 'react-router-dom';
import { mdRef, rawUrl } from '../lib/content';
type Props={markdown:string;source?:string};
export default function Markdown({markdown,source='docs/index.md'}:Props){
 return <div className="markdown-body">
  <ReactMarkdown remarkPlugins={[remarkGfm]} components={{
   a:({href,children,...rest})=>{
    const dest=mdRef(source,href||'#');
    return dest.external
      ? <a href={dest.url} rel="noopener noreferrer" target="_blank" {...rest}>{children}</a>
      : <Link to={dest.url} {...rest}>{children}</Link>;
   },
   img:({src,alt,...rest})=>{
    let dest=src||'';
    if(dest && !/^https?:\/\//i.test(dest)){
     const target=mdRef(source,dest);
     dest=target.external?target.url:rawUrl(source);
    }
    return <img src={dest} alt={alt||'Document figure'} loading="lazy" {...rest}/>;
   },
   table:({children})=><div className="table-scroller"><table>{children}</table></div>
  }}>{markdown}</ReactMarkdown>
 </div>;
}
