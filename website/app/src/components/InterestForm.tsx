import { useEffect, useRef, useState } from 'react';
import { ArrowRight, CheckCircle2, ShieldCheck } from 'lucide-react';
import { Link } from 'react-router-dom';
declare global { interface Window { turnstile?: {
  render:(container:HTMLElement, options:Record<string,unknown>)=>string;
  reset:(id:string)=>void;
  remove:(id:string)=>void;
} } }
const siteKey = import.meta.env.VITE_TURNSTILE_SITE_KEY as string|undefined;
const privacyEmail = import.meta.env.VITE_PRIVACY_EMAIL as string|undefined;
const enabled=Boolean(siteKey && privacyEmail);
const interests=[
 ['research','Research & neuroscience'],['engineering','Engineering & software'],
 ['collaboration','Collaborations'],['studio','StarChip creative tools'],['general','All updates'],
];
export default function InterestForm({compact=false}:{compact?:boolean}){
 const container=useRef<HTMLDivElement>(null);
 const widget=useRef<string|null>(null);
 const [token,setToken]=useState('');
 const [email,setEmail]=useState('');
 const [area,setArea]=useState('general');
 const [consent,setConsent]=useState(false);
 const [state,setState]=useState<'idle'|'sending'|'done'|'error'>('idle');
 const [message,setMessage]=useState('');
 useEffect(()=>{
  if(!enabled)return;
  let cancelled=false;
  const render=()=>{
   if(cancelled||!container.current||!window.turnstile||widget.current)return;
   widget.current=window.turnstile.render(container.current,{
    sitekey:siteKey,action:'interest',
    callback:(value:unknown)=>setToken(typeof value==='string'?value:''),
    'expired-callback':()=>setToken(''),
    'error-callback':()=>setToken('')
   });
  };
  if(window.turnstile){render();return()=>{cancelled=true;if(widget.current&&window.turnstile)window.turnstile.remove(widget.current);widget.current=null;};}
  const script=document.createElement('script');
  script.src='https://challenges.cloudflare.com/turnstile/v0/api.js?render=explicit';
  script.async=true;script.addEventListener('load',render);document.head.appendChild(script);
  return()=>{cancelled=true;if(widget.current&&window.turnstile)window.turnstile.remove(widget.current);widget.current=null;script.remove();};
 },[]);
 async function submit(e:React.FormEvent<HTMLFormElement>){
  e.preventDefault();
  if(!enabled||!consent||!token||state==='sending')return;
  setState('sending');setMessage('');
  try{
   const res=await fetch('/api/interest',{method:'POST',headers:{'content-type':'application/json'},
    body:JSON.stringify({email,interest:area,consent,turnstileToken:token,source:window.location.pathname,website:''})});
   const data=await res.json() as {ok?:boolean;message?:string;error?:string};
   if(!res.ok||!data.ok)throw Error(data.error||'The request could not be saved.');
   setState('done');setMessage(data.message||'Your interest has been recorded.');
  }catch(err){
   setState('error');setMessage(err instanceof Error?err.message:'Please try again.');
   setToken('');if(widget.current&&window.turnstile)window.turnstile.reset(widget.current);
  }
 }
 if(!enabled)return <div className={'interest-form interest-pending'+(compact?' compact':'')}>
  <div className="form-standby"><ShieldCheck size={21}/><div><strong>Research interest registration is being prepared.</strong>
  <p>The public signup opens after the privacy contact, database and anti-spam verification are configured. No emails are collected by this site yet.</p>
  </div></div><Link to="/journal" className="quiet-link">Read the research journal <ArrowRight size={15}/></Link>
 </div>;
 return <form onSubmit={submit} className={'interest-form'+(compact?' compact':'')} aria-label="Research interest registration">
  {state==='done'?<div className="form-success" role="status"><CheckCircle2 size={26}/><h3>You're on our interest list.</h3><p>{message}</p></div>:<>
  <label htmlFor={compact?'interest-email-mini':'interest-email'} className="form-label">Your email address</label>
  <div className="form-line">
   <input id={compact?'interest-email-mini':'interest-email'} type="email" required autoComplete="email" maxLength={254}
     value={email} onChange={e=>setEmail(e.target.value)} placeholder="you@example.com" aria-label="Your email address"/>
   <button className="button button-primary" type="submit" disabled={!consent||!token||state==='sending'}>
     {state==='sending'?'Saving…':'Follow Anaiero'} <ArrowRight size={16}/>
   </button>
  </div>
  <label htmlFor={compact?'interest-area-mini':'interest-area'} className="form-label second-label">I’m interested in</label>
  <select id={compact?'interest-area-mini':'interest-area'} value={area} onChange={e=>setArea(e.target.value)}>
   {interests.map(([v,t])=><option key={v} value={v}>{t}</option>)}
  </select>
  <label className="checkline"><input type="checkbox" checked={consent} onChange={e=>setConsent(e.target.checked)}/>
   <span>I agree to receive Anaiero research and development updates from We Develop Software LLC. I can request removal. <Link to="/privacy">Privacy details</Link>.</span>
  </label>
  <div className="honeypot" aria-hidden="true"><label>Website<input type="text" name="website" tabIndex={-1} autoComplete="off"/></label></div>
  <div ref={container} className="turnstile-slot" aria-label="Spam protection verification"/>
  {state==='error'&&<p className="form-error" role="alert">{message}</p>}
  <p className="form-footnote">No spam. Interest is stored privately; email dispatch will begin only after a confirmed mailing workflow is in place.</p>
  </>}
 </form>;
}
