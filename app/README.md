# Doghouse

The configurator for Dachshund60F and future dog-named boards. Svelte 5 +
TypeScript, built with Vite. It runs in Chrome or Edge and talks to real
boards over WebHID, or to simulated ones in demo mode; a Tauri shell will wrap
the same frontend into a signed desktop app later (that needs Rust and the
MSVC build tools).

```
cd app
npm install
npm run dev        # http://localhost:5173, or http://localhost:5173/?demo
```

Other scripts: `npm run check` (svelte-check), `npm test` (pure logic, with
Node's built-in test runner) and `npm run build` (static files in `dist/`).

## What it does

- **Your pack:** every connected board, with its name, model and breed,
  connection (USB cable, 2.4 GHz dongle or Bluetooth) and battery.
- **Keys:** the board drawn from `boards/<model>/keyboard.json`, with layer
  tabs, the spacebar layout option, transparent keys showing what they fall
  through to, the roller (forward, back and press), and the LED row (the
  battery gauge follows the battery). Click a key and pick from the palette,
  or drag keys from the palette or between keys. Undo/redo (Ctrl+Z, Ctrl+Y),
  Delete clears a key, Ctrl+S saves.
- **Settings:** board name, debounce, tapping term, NKRO, sleep timeout,
  indicator brightness, factory reset.
- **About:** serial, connection, firmware and protocol versions, the
  firmware's features, and a button that restarts the board into its UF2
  bootloader for a firmware update.

Edits go to the board immediately (as live, unsaved changes), and **Save to
board** stores them in flash; **Revert** goes back to the last save.

## How it fits together

- The app talks only to host-lib's typed `Board` / `BoardManager` API
  (`host-lib/src/types.ts`), imported as TypeScript source through a Vite
  alias. No raw protocol bytes reach the UI.
- Real boards come from `WebHidBoardManager`. The first time, **Connect a
  board…** opens the browser's device picker (WebHID needs a click); after
  that Doghouse opens the board by itself whenever it's plugged in, and drops
  it when it's unplugged.
- Demo mode (`?demo` in the URL, or a browser without WebHID) adds
  `MockBoardManager`'s simulated boards. Open "Demo mode" at the bottom of the
  sidebar to plug in more boards by USB or dongle, unplug them, or set the
  battery. Mock boards validate every edit the way the firmware must.
- The app asks for what the board's firmware supports: a board without the
  settings capability shows no settings (true of the current firmware, until
  phase 10), the roller can't be remapped until encoders arrive (phase 9), and
  Identify, Save and Revert appear only when the firmware has them.
- A board on another protocol version is listed with a message saying which
  side to update, and Doghouse never writes to it.
- Keyboard definitions are read from `boards/`, so the layout has one source.

## Design language

Warm earth tones (`src/app.css`): oat and cream surfaces, espresso text, a
walnut case, terracotta for actions, ochre for energy, sage for "all good".
Dark mode is a warm den. Boards are dogs: a dachshund avatar, battery as a
bone, paw-print toasts, "Your pack" for the board list.
