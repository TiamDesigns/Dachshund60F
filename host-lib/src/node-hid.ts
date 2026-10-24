/**
 * Boards through hidapi on the desktop, via the node-hid package (v3, its
 * async API). node-hid is loaded only when a manager is created without one,
 * so the library itself (and the browser build) doesn't depend on it; the
 * desktop shell installs it.
 *
 * hidapi has no hotplug events, so the manager polls the device list.
 */
import { CONFIG_INTERFACE_NUMBER, HidManagerBase, isConfigUsage, isKeyboardUsbId, type HidManagerOptions } from "./hid-manager.js";
import { HID_BLE_REPORT_ID, REPORT_SIZE } from "./protocol.gen.js";
import { TransportEvents, type Transport } from "./transport.js";

/** The parts of node-hid's device list entries this uses. */
export interface NodeHidDeviceInfo {
  vendorId: number;
  productId: number;
  path?: string;
  serialNumber?: string;
  usagePage?: number;
  usage?: number;
  interface?: number;
  /** hidapi's bus type (0.13 and later): 2 is Bluetooth. */
  busType?: number;
}

/** hidapi's HID_API_BUS_BLUETOOTH. */
const BUS_BLUETOOTH = 2;

/** The parts of node-hid's HIDAsync this uses. */
export interface NodeHidDevice {
  write(data: number[]): Promise<number>;
  on(event: "data", listener: (data: Uint8Array) => void): unknown;
  on(event: "error", listener: (error: unknown) => void): unknown;
  close(): Promise<void>;
}

export interface NodeHidModule {
  devicesAsync(): Promise<NodeHidDeviceInfo[]>;
  HIDAsync: { open(path: string): Promise<NodeHidDevice> };
}

/** Load node-hid. Throws if it isn't installed. */
export async function loadNodeHid(): Promise<NodeHidModule> {
  const name = "node-hid"; // not a static import: it's an optional dependency
  const mod = (await import(name)) as NodeHidModule & { default?: NodeHidModule };
  return typeof mod.devicesAsync === "function" ? mod : mod.default!;
}

/**
 * True for the config interface of one of our keyboards. Linux's hidraw
 * backend reports no usage page, so there the interface number decides.
 */
export function isNodeHidBoard(info: NodeHidDeviceInfo): boolean {
  if (!isKeyboardUsbId(info.vendorId, info.productId) || !info.path) return false;
  if (info.usagePage) return isConfigUsage(info.usagePage, info.usage);
  return info.interface === CONFIG_INTERFACE_NUMBER;
}

class NodeHidTransport implements Transport {
  readonly connection: "usb" | "ble";
  /** 0 over USB; over Bluetooth the config reports carry HID_BLE_REPORT_ID. */
  private readonly reportId: number;
  private readonly events = new TransportEvents();

  constructor(
    private readonly device: NodeHidDevice,
    bluetooth: boolean,
  ) {
    this.connection = bluetooth ? "ble" : "usb";
    this.reportId = bluetooth ? HID_BLE_REPORT_ID : 0;
    device.on("data", (data) => {
      // hidapi puts the report ID first when the device uses them.
      if (this.reportId === 0) this.events.report(Uint8Array.from(data));
      else if (data[0] === this.reportId) this.events.report(Uint8Array.from(data.subarray(1)));
    });
    device.on("error", () => void this.close());
  }

  async write(report: Uint8Array): Promise<void> {
    if (this.events.isClosed) throw new Error("transport closed");
    // hidapi wants the report ID first: 0 over USB, where the interface has none.
    const data = new Array<number>(REPORT_SIZE + 1).fill(0);
    data[0] = this.reportId;
    report.subarray(0, REPORT_SIZE).forEach((b, i) => (data[i + 1] = b));
    try {
      await this.device.write(data);
    } catch (err) {
      await this.close();
      throw err;
    }
  }

  onReport(listener: (report: Uint8Array) => void): () => void {
    return this.events.onReport(listener);
  }

  onClose(listener: () => void): () => void {
    return this.events.onClose(listener);
  }

  async close(): Promise<void> {
    if (this.events.isClosed) return;
    this.events.close();
    await this.device.close().catch(() => undefined);
  }
}

export interface NodeHidManagerOptions extends HidManagerOptions {
  /** node-hid, or a stand-in for tests. Default: loaded on start(). */
  hid?: NodeHidModule;
  /** How often to look for boards plugged in or out. Default 1000 ms. */
  pollMs?: number;
}

export class NodeHidBoardManager extends HidManagerBase {
  private hid: NodeHidModule | undefined;
  private readonly pollMs: number;
  private timer: ReturnType<typeof setInterval> | undefined;
  private scanning: Promise<void> | undefined;

  constructor(options: NodeHidManagerOptions = {}) {
    super(options);
    this.hid = options.hid;
    this.pollMs = options.pollMs ?? 1000;
  }

  /** Open every board plugged in now, then keep polling for changes. */
  async start(): Promise<void> {
    this.hid ??= await loadNodeHid();
    await this.scan();
    this.timer ??= setInterval(() => void this.scan(), this.pollMs);
    // Don't keep a Node process alive just to poll.
    (this.timer as { unref?: () => void }).unref?.();
  }

  /** Stop polling and close every board. */
  override async close(): Promise<void> {
    clearInterval(this.timer);
    this.timer = undefined;
    await this.scanning;
    await super.close();
  }

  /** Look for boards plugged in or out since the last scan. */
  scan(): Promise<void> {
    this.scanning ??= this.scanOnce().finally(() => (this.scanning = undefined));
    return this.scanning;
  }

  private async scanOnce(): Promise<void> {
    const hid = this.hid;
    if (!hid) return;
    let devices: NodeHidDeviceInfo[];
    try {
      devices = (await hid.devicesAsync()).filter(isNodeHidBoard);
    } catch (err) {
      this.options.onError?.(err);
      return;
    }
    const present = new Set(devices.map((d) => d.path!));
    // Unplugged: hidapi doesn't always report an error on a read, so close what's gone.
    await Promise.all(this.keys().filter((key) => !present.has(key)).map((key) => this.remove(key)));
    await Promise.all(
      devices.map((info) =>
        this.add(
          info.path!,
          async () => new NodeHidTransport(await hid.HIDAsync.open(info.path!), info.busType === BUS_BLUETOOTH),
          info.serialNumber,
        ),
      ),
    );
  }
}
