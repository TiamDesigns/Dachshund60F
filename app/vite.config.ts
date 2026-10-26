import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import { defineConfig } from "vite";

const repoRoot = fileURLToPath(new URL("..", import.meta.url));
// The app's version lives in package.json (the desktop app reads it from there too).
const { version } = JSON.parse(readFileSync(new URL("package.json", import.meta.url), "utf8")) as { version: string };

export default defineConfig({
  plugins: [svelte()],
  define: { __DOGHOUSE_VERSION__: JSON.stringify(version) },
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
