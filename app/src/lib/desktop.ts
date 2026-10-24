/**
 * The Doghouse desktop app (app/src-tauri/): when Doghouse runs inside it,
 * boards come through the app's hidapi bridge instead of WebHID, firmware is
 * written straight onto the board's UF2 drive, and the app updates itself
 * from GitHub Releases. In a browser none of this is loaded.
 */
import type { TauriHidBridge } from "@dachshund60f/host-lib";
import { Channel, invoke } from "@tauri-apps/api/core";

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

export interface Desktop {
  hid: TauriHidBridge;
  uf2Drives(): Promise<Uf2Drive[]>;
  uf2Write(drive: string, bytes: Uint8Array): Promise<void>;
  /** A newer Doghouse, or null. Throws if the update server can't be reached. */
  checkForUpdate(): Promise<AppUpdate | null>;
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
    async checkForUpdate() {
      // Builds without the updater's public key (local ones) have no updates.
      if (!(await invoke<boolean>("updates_enabled"))) return null;
      const { check } = await import("@tauri-apps/plugin-updater");
      const update = await check();
      if (!update) return null;
      return {
        version: update.version,
        notes: update.body,
        async install(onProgress) {
          let total: number | undefined;
          let done = 0;
          await update.downloadAndInstall((event) => {
            if (event.event === "Started") total = event.data.contentLength;
            else if (event.event === "Progress") done += event.data.chunkLength;
            onProgress?.(total ? Math.min(done / total, 1) : null);
          });
          const { relaunch } = await import("@tauri-apps/plugin-process");
          await relaunch();
        },
      };
    },
  };
}
