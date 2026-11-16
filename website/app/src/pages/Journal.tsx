import { useEffect, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { ArrowLeft, ArrowRight, ArrowUpRight, Clock3, Feather, CalendarDays } from 'lucide-react';
import Markdown from '../components/Markdown';
import { catalog, loadPost, type FullPost } from '../lib/content';
export function Journal(){
 return <div className="page-wrap">
  <div className="container page-hero"><span className="section-kicker">THE ANAIERO JOURNAL / DISPATCHES FROM THE RESEARCH</span><h1>Ideas, experiments,<br/><em>and the work between.</em></h1><p>Research notes, architecture decisions and project milestones from We Develop Software LLC. Posts are authored as source-controlled Markdown and published with the same build as the rest of Anaiero.</p></div>
  <section className="section"><div className="container"><div className="journal-grid">{catalog.posts.map((p,i)=><Link to={p.href} key={p.id} className={'journal-card'+(i===0?' featured-post':'')}><div className="journal-icon"><Feather size={24}/></div><div className="journal-card-body"><span className="card-overline">{p.category.toUpperCase()} · {new Date(p.date+'T12:00:00Z').toLocaleDateString('en-US',{month:'short',day:'numeric',year:'numeric'})}</span><h2>{p.title}</h2><p>{p.summary}</p><span className="journal-read">Read article <ArrowUpRight size={16}/></span></div></Link>)}</div>
   <div className="journal-message"><CalendarDays size={25}/><div><h3>More field notes as the work progresses.</h3><p>The journal is a versioned publication, not a live stream of private conversations or lab telemetry.</p></div><Link to="/interest" className="text-link">Follow the project <ArrowRight size={15}/></Link></div>
  </div></section>
 </div>;
}
export function JournalPost(){
 const {slug}=useParams();
 const entry=catalog.posts.find(p=>p.id===slug);
 const [post,setPost]=useState<FullPost|null>(null),[error,setError]=useState(false);
 useEffect(()=>{let alive=true;setPost(null);setError(false);if(slug)loadPost(slug).then(p=>{if(alive)setPost(p);}).catch(()=>{if(alive)setError(true);});return()=>{alive=false;};},[slug]);
 if(!entry)return <div className="container page-hero"><h1>Article not found.</h1><Link to="/journal">Back to the journal</Link></div>;
 return <div className="page-wrap"><div className="container journal-article-wrap"><div className="breadcrumb"><Link to="/journal"><ArrowLeft size={15}/> Journal</Link><span>/</span>{entry.category}</div>
  <header className="journal-article-hero"><span className="section-kicker">{entry.category.toUpperCase()} · RESEARCH JOURNAL</span><h1>{entry.title}</h1><p>{entry.summary}</p><div className="doc-metadata"><span>We Develop Software LLC</span><span>{new Date(entry.date+'T12:00:00Z').toLocaleDateString('en-US',{year:'numeric',month:'long',day:'numeric'})}</span><span><Clock3 size={14}/> {entry.readMinutes} min read</span></div></header>
  <div className="journal-article-content">{error?<p>Could not load the article.</p>:!post?<p>Loading article…</p>:<Markdown markdown={post.markdown} source={post.source}/>}</div>
  <div className="article-end"><Feather size={23}/><p>Published as part of the ChristEye Anaiero research program. © 2026 We Develop Software LLC.</p><Link to="/journal">More journal notes <ArrowRight size={16}/></Link></div>
 </div></div>;
}
