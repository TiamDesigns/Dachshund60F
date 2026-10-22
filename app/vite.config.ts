import { fileURLToPath } from "node:url";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import { defineConfig } from "vite";

const repoRoot = fileURLToPath(new URL("..", import.meta.url));

export default defineConfig({
  plugins: [svelte()],
  resolve: {
    alias: {
      // host-lib is consumed as TypeScript source, so there is no build step to forget.
      "@dachshund60f/host-lib": fileURLToPath(new URL("../host-lib/src/index.ts", import.meta.url)),
      // Keyboard definitions live with the boards: boards/<model>/keyboard.json.
      "@boards": fileURLToPath(new URL("../boards", import.meta.url)),
    },
  },
  server: {
    port: 5173,
    strictPort: true,
    fs: { allow: [repoRoot] },
  },
  // Tauri (later) serves the built files from disk.
  base: "./",
  build: { target: "es2022" },
});
