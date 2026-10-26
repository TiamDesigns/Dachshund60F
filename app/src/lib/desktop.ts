/**
 * The Doghouse desktop app (app/src-tauri/): when Doghouse runs inside it,
 * boards come through the app's hidapi bridge instead of WebHID, firmware is
 * written straight onto the board's UF2 drive, and the app updates itself
 * from GitHub Releases. In a browser none of this is loaded.
 */
import type { TauriHidBridge } from "@dachshund60f/host-lib";
import { getVersion } from "@tauri-apps/api/app";
import { Channel, invoke } from "@tauri-apps/api/core";
import type { Update } from "@tauri-apps/plugin-updater";

import { compareVersions, fetchNewestRelease } from "./app-version.ts";

/** A mounted UF2 drive and its INFO_UF2.TXT. */
export interface Uf2Drive {
  path: string;
  info: string;
}

/** A newer Doghouse, ready to download and install. */
export interface AppUpdate {
  version: string;
  notes: string | undefined;
  /** Download, install and restart. */
  install(onProgress?: (fraction: number | null) => void): Promise<void>;
}

/** What a check for a newer Doghouse found. */
export type UpdateCheck =
  /** This is the newest. */
  | { kind: "current" }
  /** A newer one the app can install itself. */
  | { kind: "install"; update: AppUpdate }
  /** A newer one to download by hand, and why the app can't install it. */
  | { kind: "download"; version: string; url: string; reason: string };

export interface Desktop {
  hid: TauriHidBridge;
  uf2Drives(): Promise<Uf2Drive[]>;
  uf2Write(drive: string, bytes: Uint8Array): Promise<void>;
  /** This copy's version. */
  version(): Promise<string>;
  /** Look for a newer Doghouse. Throws, with words for people, if it can't find out. */
  checkForUpdate(): Promise<UpdateCheck>;
  /** Open the download page in the computer's browser. */
  openReleasesPage(): Promise<void>;
}

export function inDesktopApp(): boolean {
  return "__TAURI_INTERNALS__" in globalThis;
}

/** The app's side of things; only call it in the app (inDesktopApp). */
export function desktopApp(): Desktop {
  return {
    hid: {
      invoke: (command, args) => invoke(command, args),
      channel<T>(onMessage: (message: T) => void) {
        const channel = new Channel<T>();
        channel.onmessage = onMessage;
        return channel;
      },
    },
    uf2Drives: () => invoke<Uf2Drive[]>("uf2_drives"),
    uf2Write: (drive, bytes) => invoke("uf2_write", { drive, data: [...bytes] }),
    version: () => getVersion(),
    openReleasesPage: () => invoke("open_releases_page"),
    async checkForUpdate() {
      const current = await getVersion();
      // Asks GitHub directly: for builds the updater can't serve.
      const byHand = async (reason: string, failure?: unknown): Promise<UpdateCheck> => {
        const newest = await fetchNewestRelease();
        if (newest && compareVersions(newest.version, current) > 0) {
          return { kind: "download", version: newest.version, url: newest.url, reason };
        }
        if (failure !== undefined) throw new Error(`The updater failed: ${errorWords(failure)}`);
        return { kind: "current" };
      };

      // Builds without the updater's public key (local ones, and releases
      // made before the key was set up) can't install updates themselves.
      if (!(await invoke<boolean>("updates_enabled"))) {
        return byHand("This copy of Doghouse was built without automatic updates, so install the new one yourself. Updates after that can install themselves.");
      }
      let update: Update | null;
      try {
        const { check } = await import("@tauri-apps/plugin-updater");
        update = await check({ timeout: 30_000 });
      } catch (err) {
        // A release without signed update files, or no network: GitHub may still know.
        console.warn("Doghouse updater:", err);
        return byHand(`The built-in updater couldn't fetch it (${errorWords(err)}), so download it instead.`, err);
      }
      if (!update) return { kind: "current" };
      const found = update;
      return {
        kind: "install",
        update: {
          version: found.version,
          notes: found.body,
          async install(onProgress) {
            let total: number | undefined;
            let done = 0;
            await found.downloadAndInstall((event) => {
              if (event.event === "Started") total = event.data.contentLength;
              else if (event.event === "Progress") done += event.data.chunkLength;
              onProgress?.(total ? Math.min(done / total, 1) : null);
            });
            const { relaunch } = await import("@tauri-apps/plugin-process");
            await relaunch();
          },
        },
      };
    },
  };
}

function errorWords(err: unknown): string {
  return err instanceof Error ? err.message : String(err);
}
