import {useEffect,useRef,useState} from 'react';
import type {FormEvent} from 'react';
import {Link} from 'react-router-dom';
import {ArrowRight,BookOpen,CheckCircle2,FlaskConical,HeartHandshake,LockKeyhole,Mail,ShieldCheck} from 'lucide-react';

type PublicConfig={
  enabled:boolean;siteKey:string|null;privacyContact:string|null;consentVersion:string;
};
type Status='loading'|'standby'|'ready'|'sending'|'saved'|'error';

declare global {
  interface Window {
    turnstile?: {
      render: (node:HTMLElement,options:Record<string,unknown>)=>string;
      remove: (id:string)=>void;
      reset: (id:string)=>void;
    };
  }
}

const categories=[
  {value:'research',label:'Scientific research & neuroscience'},
  {value:'engineering',label:'Software, hardware & engineering'},
  {value:'studio',label:'StarChip creative applications'},
  {value:'collaboration',label:'Research & development collaborations'},
  {value:'general',label:'All Anaiero updates'},
];

function TurnstileWidget({siteKey,onToken}:{siteKey:string;onToken:(token:string)=>void}){
  const node=useRef<HTMLDivElement>(null);
  const widgetId=useRef<string|null>(null);
  useEffect(()=>{
    let cancelled=false;
    const render=()=>{
      if(cancelled||!node.current||!window.turnstile||widgetId.current)return;
      widgetId.current=window.turnstile.render(node.current,{
        sitekey:siteKey,action:'interest_signup',
        callback:(v:unknown)=>onToken(typeof v==='string'?v:''),
        'expired-callback':()=>onToken(''),
        'error-callback':()=>onToken(''),
      });
    };
    let script:HTMLScriptElement|null=null;
    if(window.turnstile)render();
    else{
      script=document.createElement('script');
      script.src='https://challenges.cloudflare.com/turnstile/v0/api.js?render=explicit';
      script.async=true;
      script.addEventListener('load',render);
      document.head.appendChild(script);
    }
    return ()=>{
      cancelled=true;
      if(widgetId.current&&window.turnstile){
        window.turnstile.remove(widgetId.current);
        widgetId.current=null;
      }
      if(script)script.remove();
    };
  },[siteKey,onToken]);
  return <div ref={node} className="interest-turnstile" aria-label="Verification against automated submissions"/>;
}

export function InterestPage(){
  const [config,setConfig]=useState<PublicConfig|null>(null);
  const [status,setStatus]=useState<Status>('loading');
  const [email,setEmail]=useState('');
  const [interest,setInterest]=useState('general');
  const [consent,setConsent]=useState(false);
  const [token,setToken]=useState('');
  const [message,setMessage]=useState('');

  useEffect(()=>{
    let alive=true;
    fetch('/api/interest/config',{cache:'no-store'})
      .then(async r=>r.ok?await r.json() as PublicConfig:Promise.reject(Error('Unavailable')))
      .then(c=>{if(alive){setConfig(c);setStatus(c.enabled?'ready':'standby');}})
      .catch(()=>{if(alive)setStatus('standby');});
    return ()=>{alive=false;};
  },[]);
  async function submit(e:FormEvent<HTMLFormElement>){
    e.preventDefault();
    if(!config?.enabled||!consent||!token||status==='sending')return;
    setStatus('sending');
    setMessage('');
    const source=window.location.hash.replace(/^#/,'')||'/interest';
    try{
      const response=await fetch('/api/interest',{
        method:'POST',
        credentials:'same-origin',
        headers:{'content-type':'application/json'},
        body:JSON.stringify({
          email,interest,consent,consentVersion:config.consentVersion,
          turnstileToken:token,source,website:'',
        }),
      });
      const result=await response.json() as {ok?:boolean;message?:string;error?:string};
      if(!response.ok||!result.ok)throw Error(result.error||'Registration was not completed.');
      setStatus('saved');
      setMessage(result.message||'Your interest was recorded.');
      setEmail('');
    }catch(error){
      setStatus('error');
      setMessage(error instanceof Error?error.message:'An error occurred.');
      setToken('');
    }
  }
  return <div className="page-block interest-view">
    <div className="interest-intro">
      <span className="eyebrow"><HeartHandshake size={16}/> CHRISTEYE ANAIERO · WE DEVELOP SOFTWARE LLC</span>
      <h1>Follow the work.<br/><em>Help shape what comes next.</em></h1>
      <p>Join the research interest list for engineering releases, neuroscience progress and future StarChip demonstrations. Choose the part of Anaiero you want to hear more about.</p>
    </div>
    <div className="interest-grid">
      <div className="interest-benefits">
        <div><FlaskConical size={22}/><h3>Scientific research</h3><p>Work on neural networks, learning, Roots, molecular mechanisms and responsible experimental methods.</p></div>
        <div><BookOpen size={22}/><h3>Developer progress</h3><p>Versioned technical publications, source-linked architectures, software examples and specialist modules.</p></div>
        <div><Mail size={22}/><h3>Future collaboration</h3><p>Register interest in academic research, product development or the future modular-computing platform.</p></div>
        <p className="interest-disclosure"><ShieldCheck size={16}/> Real software results and biological research hypotheses are communicated separately. We will not sell your contact information.</p>
      </div>
      <div className="interest-card">
        <div className="eyebrow">RESEARCH INTEREST</div>
        <h2>Stay connected.</h2>
        {status==='loading'&&<div className="interest-standby" role="status">Checking signup availability…</div>}
        {status==='standby'&&<div className="interest-standby" role="status">
          <LockKeyhole size={25}/>
          <h3>Registration is being configured.</h3>
          <p>The site is public, but the secure signup database and verification are not active yet. No email addresses are collected until the privacy and verification setup is complete.</p>
          <Link className="quiet-link" to="/library">Browse the research library <ArrowRight size={15}/></Link>
        </div>}
        {status==='saved'&&<div className="interest-success" role="status">
          <CheckCircle2 size={32}/>
          <h3>Interest recorded.</h3>
          <p>{message}</p>
          <p>This is not yet a confirmed email subscription. A confirmation mechanism will be introduced before mailing updates.</p>
        </div>}
        {(status==='ready'||status==='sending'||status==='error')&&<form onSubmit={submit} className="interest-form">
          <label htmlFor="research-email">Email address</label>
          <input id="research-email" name="email" type="email" autoComplete="email" maxLength={254}
            placeholder="you@example.com" required value={email} onChange={e=>setEmail(e.target.value)}/>
          <label htmlFor="research-topic">Area of interest</label>
          <select id="research-topic" name="interest" value={interest} onChange={e=>setInterest(e.target.value)}>
            {categories.map(x=><option key={x.value} value={x.value}>{x.label}</option>)}
          </select>
          <label className="interest-consent">
            <input type="checkbox" required checked={consent} onChange={e=>setConsent(e.target.checked)}/>
            <span>I agree to have my email and selected topic stored by We Develop Software LLC for Anaiero research updates, with the right to request removal. <Link to="/privacy">Read the privacy notice.</Link></span>
          </label>
          <div className="interest-honeypot" aria-hidden="true"><label>Leave this field empty<input type="text" name="website" tabIndex={-1} autoComplete="off"/></label></div>
          {config?.siteKey&&<TurnstileWidget siteKey={config.siteKey} onToken={setToken}/>}
          {status==='error'&&<p role="alert" className="interest-error">{message}</p>}
          <button className="btn btn-primary btn-large" type="submit" disabled={!consent||!token||status==='sending'}>
            {status==='sending'?'Saving interest…':'Register interest'} <ArrowRight size={17}/>
          </button>
          <p className="interest-fineprint">Registrations are saved privately in Cloudflare D1. Email ownership is not yet verified; marketing emails are not automatically sent.</p>
        </form>}
      </div>
    </div>
  </div>;
}

export function PrivacyPage(){
  const [contact,setContact]=useState<string|null>(null);
  useEffect(()=>{
    fetch('/api/interest/config',{cache:'no-store'}).then(r=>r.json())
      .then((x:{privacyContact?:string|null})=>setContact(x.privacyContact||null))
      .catch(()=>{});
  },[]);
  return <div className="page-block privacy-view">
    <div className="eyebrow"><ShieldCheck size={15}/> ANAIERO / PRIVACY & CONSENT</div>
    <h1>Privacy by design.</h1>
    <p>ChristEye Anaiero is a research and development website operated by We Develop Software LLC. This notice describes the optional research-interest form. The form is disabled until the private storage, verification and contact mechanisms are configured.</p>
    <section><h2>Information collected</h2><p>With your explicit consent, we record the email address you submit, your chosen research-interest category, the source page, the time of consent and the consent-policy version. We do not make that registration data publicly searchable.</p></section>
    <section><h2>Purpose and status</h2><p>We use these details to understand interest in Anaiero, potential collaborations and future research updates. A newly saved entry is marked awaiting confirmation. It is not proof that the email address belongs to the registrant, and the current endpoint does not automatically send marketing email.</p></section>
    <section><h2>Security and providers</h2><p>Registration data is stored in a private Cloudflare D1 database. Cloudflare Turnstile checks that form submissions are genuine, including server-side token validation. Cloudflare may process technical request information for security and service delivery under its policies. No subscriber list is exposed through the public API.</p></section>
    <section><h2>Retention and removal</h2><p>We plan to keep only the data necessary for the interest list and to honor withdrawal requests. A fixed retention schedule and confirmation/unsubscribe workflow must be finalized before sending subscription emails. You can request deletion using the configured privacy contact.</p>
      {contact?<p>Privacy contact: <a href={'mailto:'+contact}>{contact}</a></p>
        :<p>Privacy contact details are not yet configured, so the registration form remains disabled. Please do not post private information in public GitHub issues.</p>}</section>
    <section><h2>Scientific documents and rights</h2><p>Public research content is versioned in the project repository under the WDS Public Research Reference License. The separate confidential patent document is not part of this public website.</p></section>
    <Link to="/interest" className="quiet-link">Return to the interest page <ArrowRight size={15}/></Link>
  </div>;
}
