/**
 * Boards through the Doghouse desktop app (app/src-tauri/src/hid.rs), whose
 * WebView has no WebHID on macOS and Linux: the app lists, opens and writes
 * HID devices with hidapi and streams their reports back. This adapts that
 * bridge to the node-hid shape, so the node-hid manager runs on it as is.
 *
 * host-lib doesn't depend on Tauri: the app passes in its invoke and a way to
 * make a channel (from @tauri-apps/api/core).
 */
import {
  NodeHidBoardManager,
  type NodeHidDevice,
  type NodeHidDeviceInfo,
  type NodeHidManagerOptions,
  type NodeHidModule,
} from "./node-hid.js";

/** What the app's hid_open channel carries. */
export type TauriHidEvent = { type: "report"; data: number[] } | { type: "closed" };

export interface TauriHidBridge {
  invoke<T>(command: string, args?: Record<string, unknown>): Promise<T>;
  /** A Tauri Channel that calls onMessage for each message. */
  channel<T>(onMessage: (message: T) => void): unknown;
}

/** node-hid's module shape over the desktop app's bridge. */
export function tauriHid(bridge: TauriHidBridge): NodeHidModule {
  return {
    devicesAsync: () => bridge.invoke<NodeHidDeviceInfo[]>("hid_devices"),
    HIDAsync: {
      async open(path: string): Promise<NodeHidDevice> {
        const dataListeners = new Set<(data: Uint8Array) => void>();
        const errorListeners = new Set<(error: unknown) => void>();
        const channel = bridge.channel<TauriHidEvent>((event) => {
          if (event.type === "report") {
            const data = Uint8Array.from(event.data);
            for (const listener of dataListeners) listener(data);
          } else {
            for (const listener of errorListeners) listener(new Error("device closed"));
          }
        });
        const handle = await bridge.invoke<number>("hid_open", { path, onEvent: channel });
        let closed = false;
        const device: NodeHidDevice = {
          write: (data) => bridge.invoke<number>("hid_write", { handle, data }),
          on(event: "data" | "error", listener: (arg: never) => void) {
            (event === "data" ? dataListeners : errorListeners).add(listener as never);
            return device;
          },
          async close() {
            if (closed) return;
            closed = true;
            dataListeners.clear();
            errorListeners.clear();
            await bridge.invoke("hid_close", { handle });
          },
        };
        return device;
      },
    },
  };
}

/** Boards in the desktop app: the node-hid manager on the app's bridge. */
export class TauriHidBoardManager extends NodeHidBoardManager {
  constructor(bridge: TauriHidBridge, options: Omit<NodeHidManagerOptions, "hid"> = {}) {
    super({ ...options, hid: tauriHid(bridge) });
  }
}
