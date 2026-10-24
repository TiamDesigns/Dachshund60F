/**
 * Boards through WebHID (Chrome and Edge). The browser only shows a site the
 * devices the user picked once with requestDevice(), which needs a click;
 * after that start() finds them again and hotplug works on its own.
 *
 * The WebHID types are declared here (structurally) so the library needs no
 * extra type package.
 */
import { HidManagerBase, isConfigUsage, KEYBOARD_USB_IDS, type HidManagerOptions } from "./hid-manager.js";
import { HID_USAGE, HID_USAGE_PAGE, REPORT_SIZE } from "./protocol.gen.js";
import { TransportEvents, type Transport } from "./transport.js";

export interface WebHidReportInfo {
  reportId?: number;
}

export interface WebHidCollection {
  usagePage?: number;
  usage?: number;
  inputReports?: readonly WebHidReportInfo[];
  outputReports?: readonly WebHidReportInfo[];
}

export interface WebHidInputReportEvent {
  readonly device: WebHidDevice;
  readonly reportId: number;
  readonly data: DataView;
}

export interface WebHidDevice {
  readonly opened: boolean;
  readonly vendorId: number;
  readonly productId: number;
  readonly productName: string;
  readonly collections: readonly WebHidCollection[];
  open(): Promise<void>;
  close(): Promise<void>;
  sendReport(reportId: number, data: Uint8Array): Promise<void>;
  addEventListener(type: "inputreport", listener: (event: WebHidInputReportEvent) => void): void;
  removeEventListener(type: "inputreport", listener: (event: WebHidInputReportEvent) => void): void;
}

export interface WebHidConnectionEvent {
  readonly device: WebHidDevice;
}

export interface WebHid {
  getDevices(): Promise<WebHidDevice[]>;
  requestDevice(options: {
    filters: { vendorId?: number; productId?: number; usagePage?: number; usage?: number }[];
  }): Promise<WebHidDevice[]>;
  addEventListener(type: "connect" | "disconnect", listener: (event: WebHidConnectionEvent) => void): void;
  removeEventListener(type: "connect" | "disconnect", listener: (event: WebHidConnectionEvent) => void): void;
}

/** navigator.hid, if this browser has WebHID. */
export function webHid(): WebHid | undefined {
  const nav = (globalThis as { navigator?: { hid?: WebHid } }).navigator;
  return nav?.hid;
}

/** True for the config interface of one of our keyboards. */
export function isWebHidBoard(device: WebHidDevice): boolean {
  return (
    KEYBOARD_USB_IDS.some((id) => id.vendorId === device.vendorId && id.productId === device.productId) &&
    device.collections.some((c) => isConfigUsage(c.usagePage, c.usage))
  );
}

/**
 * The config collection's report ID: 0 over USB, where it has an interface
 * of its own, and HID_BLE_REPORT_ID over Bluetooth, where it shares the HID
 * service with the keyboard's reports.
 */
export function configReportId(device: WebHidDevice): number {
  const config = device.collections.find((c) => isConfigUsage(c.usagePage, c.usage));
  return config?.outputReports?.[0]?.reportId ?? config?.inputReports?.[0]?.reportId ?? 0;
}

class WebHidTransport implements Transport {
  readonly connection: "usb" | "ble";
  private readonly reportId: number;
  private readonly events = new TransportEvents();
  private readonly onInput = (event: WebHidInputReportEvent): void => {
    if (event.reportId !== this.reportId) return;
    this.events.report(new Uint8Array(event.data.buffer, event.data.byteOffset, event.data.byteLength).slice());
  };

  constructor(readonly device: WebHidDevice) {
    this.reportId = configReportId(device);
    this.connection = this.reportId === 0 ? "usb" : "ble";
    device.addEventListener("inputreport", this.onInput);
  }

  async write(report: Uint8Array): Promise<void> {
    if (this.events.isClosed) throw new Error("transport closed");
    const data = new Uint8Array(REPORT_SIZE);
    data.set(report.subarray(0, REPORT_SIZE));
    await this.device.sendReport(this.reportId, data);
  }

  onReport(listener: (report: Uint8Array) => void): () => void {
    return this.events.onReport(listener);
  }

  onClose(listener: () => void): () => void {
    return this.events.onClose(listener);
  }

  /** The device is gone: no close() call on it. */
  lost(): void {
    this.device.removeEventListener("inputreport", this.onInput);
    this.events.close();
  }

  async close(): Promise<void> {
    if (this.events.isClosed) return;
    this.lost();
    if (this.device.opened) await this.device.close().catch(() => undefined);
  }
}

export class WebHidBoardManager extends HidManagerBase {
  private readonly transports = new Map<WebHidDevice, WebHidTransport>();
  private readonly ids = new Map<WebHidDevice, string>();
  private nextId = 0;

  private readonly onConnect = (event: WebHidConnectionEvent): void => void this.open(event.device);
  private readonly onDisconnect = (event: WebHidConnectionEvent): void => {
    this.transports.get(event.device)?.lost();
    this.transports.delete(event.device);
  };

  constructor(
    private readonly hid: WebHid,
    options: HidManagerOptions = {},
  ) {
    super(options);
    hid.addEventListener("connect", this.onConnect);
    hid.addEventListener("disconnect", this.onDisconnect);
  }

  /** Open every board this site already has permission for. */
  async start(): Promise<void> {
    await Promise.all((await this.hid.getDevices()).map((device) => this.open(device)));
  }

  /** Ask the user to pick a board (call from a click handler), then open it. */
  async request(): Promise<void> {
    const filters = KEYBOARD_USB_IDS.map((id) => ({ ...id, usagePage: HID_USAGE_PAGE, usage: HID_USAGE }));
    const devices = await this.hid.requestDevice({ filters });
    await Promise.all(devices.map((device) => this.open(device)));
  }

  /** Stop following hotplug and close every board. */
  override async close(): Promise<void> {
    this.hid.removeEventListener("connect", this.onConnect);
    this.hid.removeEventListener("disconnect", this.onDisconnect);
    await super.close();
  }

  private key(device: WebHidDevice): string {
    let id = this.ids.get(device);
    if (id === undefined) {
      id = `webhid-${this.nextId++}`;
      this.ids.set(device, id);
    }
    return id;
  }

  private async open(device: WebHidDevice): Promise<void> {
    if (!isWebHidBoard(device)) return;
    await this.add(this.key(device), async () => {
      if (!device.opened) await device.open();
      const transport = new WebHidTransport(device);
      this.transports.set(device, transport);
      return transport;
    });
  }
}
