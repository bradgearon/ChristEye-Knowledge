import {defineConfig,devices} from '@playwright/test';
export default defineConfig({
 testDir:'./tests',timeout:25_000,expect:{timeout:12_000},fullyParallel:true,
 reporter:'line',use:{baseURL:'http://127.0.0.1:4173/ChristEye-Knowledge/',trace:'retain-on-failure'},
 projects:[{name:'chromium',use:{...devices['Desktop Chrome'],viewport:{width:1440,height:900}}}],
 webServer:{command:'npm run preview -- --port 4173 --strictPort',url:'http://127.0.0.1:4173/ChristEye-Knowledge/',reuseExistingServer:!process.env.CI,timeout:30_000}
});
