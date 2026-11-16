import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import {env} from 'node:process';

export default defineConfig({
  plugins: [react()],
  base: env.ANAIERO_SITE_BASE || '/ChristEye-Knowledge/',
  build: { outDir: 'dist', emptyOutDir: true, sourcemap: false },
  server: { host: true, port: 4173 },
});
