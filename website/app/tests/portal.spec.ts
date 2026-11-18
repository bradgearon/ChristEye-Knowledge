import {test,expect} from '@playwright/test';

test('homepage, architecture and responsive source navigation', async ({page})=>{
 const errors:string[]=[];page.on('pageerror',e=>errors.push(String(e)));
 await page.goto('./');
 await expect(page.getByRole('heading',{level:1})).toContainText('A computer that learns');
 await expect(page.locator('.home-quickstats')).toContainText('352');
 await page.screenshot({path:'test-results/home-desktop.png',fullPage:true});
 await page.locator('a.sidebar-link').filter({hasText:'System atlas'}).click();
 await expect(page.getByRole('heading',{level:1})).toContainText('Many participants');
 await page.getByRole('button',{name:'Choose architecture lens'}).count();
 await expect(page.locator('.atlas-info')).toContainText('LOTUS');
 expect(errors).toEqual([]);
});

test('studio digital-only labs produce real local receipts',async ({page})=>{
 await page.goto('./#/studio');
 await expect(page.getByRole('heading',{level:1})).toContainText('another way to understand');
 await page.locator('.lab-list-item').filter({hasText:'WindBus Route Inspector'}).click();
 await page.getByRole('button',{name:/Deliver simulated event/}).click();
 await expect(page.locator('.terminal')).toContainText('accepted');
 await page.locator('.lab-list-item').filter({hasText:'Lotus Decision Engine'}).click();
 await page.locator('.lab-controls select').selectOption('change_medium');
 await expect(page.locator('.outcome')).toContainText('VETO');
 await page.screenshot({path:'test-results/studio-desktop.png',fullPage:true});
});

test('full scientific document library opens long term storage source',async ({page})=>{
 await page.goto('./#/library');
 await expect(page.locator('.library-summary')).toContainText('352');
 await page.getByRole('combobox',{name:'Collection'}).selectOption('current-module-design');
 await page.getByRole('textbox',{name:'Search documentation'}).fill('CE-A02');
 const item=page.locator('.library-item').filter({hasText:'CE-A02'}).first();
 await expect(item).toBeVisible();await item.click();
 await expect(page.locator('.technical-article')).toContainText('Long Term Storage');
 await expect(page.locator('.doc-actions')).toContainText('Raw source');
 await page.screenshot({path:'test-results/read-document.png',fullPage:false});
});

test('mobile navigation and no horizontal overflow',async ({page})=>{
 await page.setViewportSize({width:390,height:844});await page.goto('./');
 await page.getByRole('button',{name:'Open navigation'}).click();
 await expect(page.locator('.site-sidebar')).toHaveClass(/open/);
 await page.locator('a.sidebar-link').filter({hasText:'Living systems'}).click();
 await expect(page.getByRole('heading',{level:1})).toContainText('Growth with purpose');
 await expect(page.locator('.site-sidebar')).not.toHaveClass(/open/);
 await page.screenshot({path:'test-results/living-mobile.png',fullPage:false});
});
test('interest and privacy are visible, but signups remain disabled without Cloudflare configuration',async({page})=>{
 await page.route('**/api/interest/config',r=>r.fulfill({
   status:200,contentType:'application/json',
   body:JSON.stringify({enabled:false,siteKey:null,privacyContact:null,consentVersion:'2026-11-15-v1'})
 }));
 await page.goto('./#/interest');
 await expect(page.getByRole('heading',{level:1})).toContainText('Follow the work');
 await expect(page.getByRole('status')).toContainText('Registration is being configured');
 await expect(page.getByRole('textbox',{name:'Email address'})).toHaveCount(0);
 await page.goto('./#/privacy');
 await expect(page.getByRole('heading',{level:1})).toContainText('Privacy by design');
 await expect(page.getByText('Information collected',{exact:true})).toBeVisible();
});
test('configured mock shows consent and submits through the protected API',async({page})=>{
 await page.addInitScript(()=>{
   const api={
     render:(_node:HTMLElement,config:Record<string,unknown>)=>{
       const cb=config.callback as (token:string)=>void;
       setTimeout(()=>cb('test-turnstile-token'),0);
       return 'test-widget';
     },
     remove:(_id:string)=>{},
     reset:(_id:string)=>{},
   };
   Object.defineProperty(window,'turnstile',{configurable:true,value:api});
 });
 await page.route('**/api/interest/config',r=>r.fulfill({
   status:200,contentType:'application/json',
   body:JSON.stringify({enabled:true,siteKey:'test-site-key',privacyContact:'privacy@example.test',consentVersion:'2026-11-15-v1'})
 }));
 let seen=false;
 await page.route('**/api/interest',async r=>{
   if(r.request().url().endsWith('/config'))return r.continue();
   const data=r.request().postDataJSON();
   expect(data.interest).toBe('research');
   expect(data.consent).toBe(true);
   expect(data.consentVersion).toBe('2026-11-15-v1');
   expect(data.turnstileToken).toBe('test-turnstile-token');
   seen=true;
   return r.fulfill({status:202,contentType:'application/json',body:JSON.stringify({ok:true,message:'Interest recorded.'})});
 });
 await page.goto('./#/interest');
 await page.locator('#research-email').fill('tester@example.com');
 await page.locator('#research-topic').selectOption('research');
 await page.getByRole('checkbox').check();
 await page.getByRole('button',{name:'Register interest'}).click();
 await expect(page.getByRole('status')).toContainText('Interest recorded');
 expect(seen).toBe(true);
});
