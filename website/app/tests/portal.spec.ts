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
 await page.screenshot({path:'test-results/living-mobile.png',fullPage:false});
});
