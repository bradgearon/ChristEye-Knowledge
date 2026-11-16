import { useEffect, useState } from 'react';
import { Link, NavLink, useLocation } from 'react-router-dom';
import { ArrowRight, ArrowUpRight, Github, Menu, X } from 'lucide-react';

export function Brand({small=false}:{small?:boolean}) {
 return <Link to="/" className={'brand'+(small?' brand-small':'')} aria-label="Anaiero home">
  <svg className="brand-mark" viewBox="0 0 54 54" role="img" aria-label="Anaiero symbol"><defs><linearGradient id="brand-gradient" x1="0" y1="0" x2="1" y2="1"><stop stopColor="#7aefdd"/><stop offset="1" stopColor="#c9b5ff"/></linearGradient></defs>
   <rect x="1" y="1" width="52" height="52" rx="14" fill="#102b3d" stroke="#315268"/><circle cx="27" cy="27" r="17" fill="none" stroke="url(#brand-gradient)" strokeWidth="1.4"/><path d="M27 6v42M6 27h42M12 12l30 30M42 12L12 42" stroke="#9eece1" opacity=".3"/><circle cx="27" cy="27" r="6.5" fill="url(#brand-gradient)"/><circle cx="27" cy="27" r="2.2" fill="#0e283b"/></svg>
  <span className="brand-lockup"><strong>ChristEye <em>Anaiero</em></strong><small>WE DEVELOP SOFTWARE LLC</small></span>
 </Link>;
}
const links=[
 ['System','/system'],['Living Systems','/living'],['Science','/science'],['Engineering','/docs'],['StarChip Studio','/studio'],['Journal','/journal']
];
function ActiveLink({to,children,onClick}:{to:string;children:string;onClick?:()=>void}){
 return <NavLink to={to} onClick={onClick} className={({isActive})=>'nav-link'+(isActive?' active':'')}>{children}</NavLink>;
}
export function ScrollToTop(){
 const {pathname}=useLocation();
 useEffect(()=>{window.scrollTo({top:0,left:0,behavior:'instant'});document.title='ChristEye Anaiero | We Develop Software LLC';},[pathname]);
 return null;
}
export default function Layout({children}:{children:React.ReactNode}){
 const [open,setOpen]=useState(false);
 const location=useLocation();
 useEffect(()=>setOpen(false),[location.pathname]);
 return <div className="site-shell">
  <div className="announcement"><span className="announcement-dot"/><span>A research platform for the next generation of computing.</span><Link to="/about">Explore the mission <ArrowRight size={13}/></Link></div>
  <header className="site-header"><div className="header-inner">
   <Brand/>
   <nav className={'main-nav'+(open?' nav-open':'')} aria-label="Primary navigation">
    {links.map(([text,to])=><ActiveLink key={to} to={to} onClick={()=>setOpen(false)}>{text}</ActiveLink>)}
    <Link className="button button-primary mobile-cta" to="/interest">Follow the research <ArrowRight size={15}/></Link>
   </nav>
   <div className="header-actions"><Link className="header-cta" to="/interest">Follow the research <ArrowUpRight size={15}/></Link>
    <button className="menu-toggle" type="button" aria-label={open?'Close menu':'Open menu'} aria-expanded={open} onClick={()=>setOpen(v=>!v)}>{open?<X size={22}/>:<Menu size={22}/>}</button>
   </div>
  </div></header>
  <main id="main-content">{children}</main>
  <footer className="site-footer"><div className="container footer-main">
   <div className="footer-company"><Brand small/><p>A long-horizon scientific and engineering research program investigating modular computing, learning, structure, and responsible growth.</p><span className="footer-copyright">© 2026 We Develop Software LLC · All rights reserved.</span></div>
   <div className="footer-links"><h4>Discover</h4><Link to="/system">System architecture</Link><Link to="/living">Living systems</Link><Link to="/science">Science and evidence</Link><Link to="/studio">StarChip Studio</Link></div>
   <div className="footer-links"><h4>Research library</h4><Link to="/docs">Technical documents</Link><Link to="/journal">Journal and updates</Link><Link to="/about">WDS and the project</Link><Link to="/interest">Register interest</Link></div>
   <div className="footer-links"><h4>Policies & sources</h4><Link to="/privacy">Privacy and consent</Link><a href="https://github.com/bradgearon/ChristEye-Knowledge/blob/main/LICENSE.md" target="_blank" rel="noreferrer">WDS research license <ArrowUpRight size={12}/></a><a href="https://github.com/bradgearon/ChristEye-Knowledge" target="_blank" rel="noreferrer"><Github size={14}/> GitHub repository</a><a href="https://bradgearon.github.io/ChristEye-Knowledge/" target="_blank" rel="noreferrer">Previous documentation site <ArrowUpRight size={12}/></a></div>
  </div><div className="container footer-bottom"><span>Scientific hypotheses are distinguished from software demonstrations and qualified experiments.</span><span>ChristEye Anaiero / Research Edition 2026</span></div></footer>
 </div>;
}
