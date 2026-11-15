import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  base: '/ChristEye-Knowledge/',
  build: { outDir: 'dist', emptyOutDir: true, sourcemap: false },
  server: { host: true, port: 4173 },
});
