# Doghouse desktop app

Doghouse in its own window (Tauri 2), for Windows, macOS and Linux. The web
UI in `app/` is the same; this adds what a browser can't do everywhere:

- **HID** (`src/hid.rs`): macOS and Linux WebViews have no WebHID, so the app
  lists, opens and writes HID devices with hidapi and streams their reports
  to the UI over a channel. host-lib's `TauriHidBoardManager`
  (`host-lib/src/tauri-hid.ts`) is the other end. Boards come in on their
  own: there is no "Connect a board…" step.
- **Firmware updates** (`src/uf2.rs`): after the board restarts into its
  bootloader, the app finds its UF2 drive (a mounted drive with an
  `INFO_UF2.TXT` whose Board-ID matches) and writes the file onto it. No
  drive picker.
- **Auto-update** (Tauri's updater plugin): at start-up, and every six
  hours while it's open, the app checks the latest GitHub release for a
  newer Doghouse; the sidebar then offers "Install and restart". Update
  files are signed, and the app checks the signature against a public key
  built into it. App settings (bottom of the sidebar) shows the version and
  has a **Check for updates** button that says what it found or why it
  couldn't check.
- **Updates by hand:** a build without the updater key (a local build, or a
  release made before the key was set up), or one whose updater fails, asks
  GitHub's releases API instead. When there's a newer Doghouse it offers
  "Download from GitHub", which opens the release page in the browser. The
  installed copy can't replace itself in that case; the new installer does.

## Run and build

You need Rust (rustup) and Node 20 or later, plus, per OS:

- **Windows:** the Microsoft C++ Build Tools ("Desktop development with C++")
  and WebView2 (already on Windows 10 and 11).
- **macOS:** the Xcode command line tools (`xcode-select --install`).
- **Linux:** `sudo apt install libwebkit2gtk-4.1-dev libudev-dev librsvg2-dev libxdo-dev build-essential`
  (or your distribution's versions).

```
cd app
npm install
npm run tauri dev      # the app on the Vite dev server, reloading as you edit
npm run tauri build    # installers in src-tauri/target/release/bundle/
```

A local build has no updater key, so it can't install updates itself; it
still says when a newer release is out. The Rust
side has its own checks: `cargo fmt --check`, `cargo clippy --all-targets -- -D warnings`
and `cargo test` in `app/src-tauri` (after `npm run build`, since the app
embeds `app/dist`). CI runs them.

**Linux:** the logged-in user needs access to the boards' hidraw devices.
The .deb and .rpm install `linux/70-dachshund.rules` into udev's rules; for
the AppImage (or Chrome over WebHID), copy it to `/etc/udev/rules.d/` and run
`sudo udevadm control --reload && sudo udevadm trigger`.

## Releases

`.github/workflows/doghouse-release.yml` builds the app on Windows, macOS
(universal: Apple silicon and Intel) and Linux, and puts the installers on a
**draft** GitHub release called `doghouse-v<version>`.

One-time setup, for auto-update:

1. Make the update signing key on your own computer:
   `npx tauri signer generate -w ~/.tauri/doghouse.key` (from `app/`), with
   a password. Keep the private key and password safe and out of the repo:
   without them, installed copies can never be updated again.
2. In the repo's Settings → Secrets and variables → Actions:
   - secret `TAURI_SIGNING_PRIVATE_KEY`: the contents of `doghouse.key`
   - secret `TAURI_SIGNING_PRIVATE_KEY_PASSWORD`: its password
   - variable `TAURI_UPDATER_PUBKEY`: the contents of `doghouse.key.pub`

Without the variable the workflow still builds, with a warning, and those
builds don't update themselves.

Each release:

1. Bump `version` in `app/package.json` (the app takes its version from
   there) and merge.
2. Run the workflow from the Actions tab ("Doghouse release" → Run
   workflow), or push a tag `doghouse-v<version>`.
3. Check the draft release, then publish it. Or tick **Publish** when you
   run the workflow, and it publishes the release (as the latest) once every
   installer is on it. Installed copies look at
   `releases/latest/download/latest.json`, so the newest published release
   of the repo must be a Doghouse one.

## Not done yet

- **Code signing.** The installers aren't signed, so Windows SmartScreen
  and macOS Gatekeeper warn on first launch (macOS: right-click → Open).
  Signing (a Windows Authenticode certificate, an Apple Developer ID with
  notarisation) is in `SCHEDULED_TASKS.md`. Update signing above is separate
  and works without it.
- The bundle identifier is `com.tiamdesigns.doghouse`; changing it later
  makes the OS treat the app as a different one.
