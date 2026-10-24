// WebHID and node-hid board managers against stand-in APIs whose devices
// answer like a board (fake-device.ts).
import { test } from "node:test";
import assert from "node:assert/strict";

import {
  isNodeHidBoard,
  NodeHidBoardManager,
  protocol,
  WebHidBoardManager,
  type Board,
  type NodeHidDevice,
  type NodeHidDeviceInfo,
  type NodeHidModule,
  type WebHid,
  type WebHidCollection,
  type WebHidConnectionEvent,
  type WebHidDevice,
  type WebHidInputReportEvent,
} from "../src/node.js";
import { boardHandler, FakeTransport, type Handler } from "./fake-device.js";

const CONFIG = [{ usagePage: protocol.HID_USAGE_PAGE, usage: protocol.HID_USAGE }];
/** The same board over Bluetooth: one HID service, so the config reports carry an ID. */
const BLE_COLLECTIONS: WebHidCollection[] = [
  { usagePage: 1, usage: 6, inputReports: [{ reportId: 5 }], outputReports: [{ reportId: 5 }] },
  {
    usagePage: protocol.HID_USAGE_PAGE,
    usage: protocol.HID_USAGE,
    inputReports: [{ reportId: protocol.HID_BLE_REPORT_ID }],
    outputReports: [{ reportId: protocol.HID_BLE_REPORT_ID }],
  },
];

class FakeWebHidDevice implements WebHidDevice {
  opened = false;
  readonly productName = "Dachshund60F";
  private readonly listeners = new Set<(e: WebHidInputReportEvent) => void>();
  private readonly fake: FakeTransport;
  readonly sent: { reportId: number; length: number }[] = [];

  constructor(
    readonly vendorId: number,
    readonly productId: number,
    readonly collections: WebHidCollection[],
    handler: Handler,
  ) {
    this.fake = new FakeTransport(handler);
    const reportId = collections.find((c) => c.usagePage === protocol.HID_USAGE_PAGE)?.inputReports?.[0]?.reportId ?? 0;
    this.fake.onReport((r) => {
      // A keyboard report on the same device first, which the board must ignore.
      if (reportId !== 0) {
        for (const l of this.listeners) l({ device: this, reportId: 5, data: new DataView(new ArrayBuffer(8)) });
      }
      const event = { device: this, reportId, data: new DataView(r.buffer) };
      for (const l of this.listeners) l(event);
    });
  }
  async open(): Promise<void> {
    this.opened = true;
  }
  async close(): Promise<void> {
    this.opened = false;
  }
  async sendReport(reportId: number, data: Uint8Array): Promise<void> {
    assert.ok(this.opened);
    this.sent.push({ reportId, length: data.length });
    await this.fake.write(data);
  }
  addEventListener(_type: "inputreport", l: (e: WebHidInputReportEvent) => void): void {
    this.listeners.add(l);
  }
  removeEventListener(_type: "inputreport", l: (e: WebHidInputReportEvent) => void): void {
    this.listeners.delete(l);
  }
}

class FakeWebHid implements WebHid {
  granted: WebHidDevice[] = [];
  picked: WebHidDevice[] = [];
  filters: unknown;
  private readonly listeners = { connect: new Set<(e: WebHidConnectionEvent) => void>(), disconnect: new Set<(e: WebHidConnectionEvent) => void>() };
  async getDevices(): Promise<WebHidDevice[]> {
    return this.granted;
  }
  async requestDevice(options: { filters: unknown }): Promise<WebHidDevice[]> {
    this.filters = options.filters;
    return this.picked;
  }
  addEventListener(type: "connect" | "disconnect", l: (e: WebHidConnectionEvent) => void): void {
    this.listeners[type].add(l);
  }
  removeEventListener(type: "connect" | "disconnect", l: (e: WebHidConnectionEvent) => void): void {
    this.listeners[type].delete(l);
  }
  fire(type: "connect" | "disconnect", device: WebHidDevice): void {
    for (const l of this.listeners[type]) l({ device });
  }
}

function changes(manager: { subscribe(l: (b: Board[]) => void): () => void }): Board[][] {
  const seen: Board[][] = [];
  manager.subscribe((boards) => seen.push(boards));
  return seen;
}

const settle = () => new Promise((r) => setTimeout(r, 20));

test("WebHID: opens permitted boards, skips other devices, follows hotplug", async () => {
  const hid = new FakeWebHid();
  const board1 = new FakeWebHidDevice(0x1209, 0x0001, CONFIG, boardHandler("AAAA"));
  const bootKeyboard = new FakeWebHidDevice(0x1209, 0x0001, [{ usagePage: 1, usage: 6 }], boardHandler("X"));
  const other = new FakeWebHidDevice(0x1234, 0x0001, CONFIG, boardHandler("Y"));
  hid.granted = [board1, bootKeyboard, other];

  const manager = new WebHidBoardManager(hid);
  const seen = changes(manager);
  await manager.start();
  assert.deepEqual(manager.list().map((b) => b.info.id), ["AAAA"]);
  assert.equal(bootKeyboard.opened, false);
  assert.equal(other.opened, false);
  assert.ok(board1.sent.every((s) => s.reportId === 0 && s.length === protocol.REPORT_SIZE));

  const board2 = new FakeWebHidDevice(0x1209, 0x0001, CONFIG, boardHandler("BBBB"));
  hid.fire("connect", board2);
  await settle();
  assert.deepEqual(manager.list().map((b) => b.info.id), ["AAAA", "BBBB"]);

  const events: string[] = [];
  manager.list()[0]!.subscribe((e) => events.push(e.type));
  hid.fire("disconnect", board1);
  assert.deepEqual(manager.list().map((b) => b.info.id), ["BBBB"]);
  assert.deepEqual(events, ["disconnected"]);
  assert.equal(seen.length, 3);
  await manager.close();
});

test("WebHID: request() asks for our config interface and opens the pick", async () => {
  const hid = new FakeWebHid();
  hid.picked = [new FakeWebHidDevice(0x1209, 0x0001, CONFIG, boardHandler("CCCC"))];
  const manager = new WebHidBoardManager(hid);
  await manager.request();
  assert.deepEqual(hid.filters, [{ vendorId: 0x1209, productId: 0x0001, usagePage: 0xff00, usage: 0x44 }]);
  assert.deepEqual(manager.list().map((b) => b.info.id), ["CCCC"]);
  await manager.close();
});

test("WebHID: a device that doesn't answer is reported and left out", async () => {
  const hid = new FakeWebHid();
  hid.granted = [new FakeWebHidDevice(0x1209, 0x0001, CONFIG, () => null)];
  const errors: unknown[] = [];
  const manager = new WebHidBoardManager(hid, { timeoutMs: 10, retries: 0, onError: (e) => errors.push(e) });
  await manager.start();
  assert.equal(manager.list().length, 0);
  assert.equal(errors.length, 1);
});

test("WebHID: a board over Bluetooth uses its config report ID, and USB wins while both are there", async () => {
  const hid = new FakeWebHid();
  const ble = new FakeWebHidDevice(0x1209, 0x0001, BLE_COLLECTIONS, boardHandler("DDDD"));
  hid.granted = [ble];
  const manager = new WebHidBoardManager(hid);
  await manager.start();
  const [board] = manager.list();
  assert.equal(board?.info.connection, "ble");
  assert.equal(board?.info.id, "DDDD");
  assert.ok(ble.sent.length > 0 && ble.sent.every((s) => s.reportId === protocol.HID_BLE_REPORT_ID));

  // Plugged in too: listed once, over USB.
  const usb = new FakeWebHidDevice(0x1209, 0x0001, CONFIG, boardHandler("DDDD"));
  hid.fire("connect", usb);
  await settle();
  assert.deepEqual(manager.list().map((b) => [b.info.id, b.info.connection]), [["DDDD", "usb"]]);

  // Unplugged: Bluetooth carries on.
  hid.fire("disconnect", usb);
  assert.deepEqual(manager.list().map((b) => [b.info.id, b.info.connection]), [["DDDD", "ble"]]);
  await manager.close();
});

class FakeNodeHid implements NodeHidModule {
  devices: NodeHidDeviceInfo[] = [];
  readonly open = new Map<string, { closed: boolean; writes: number[][] }>();
  readonly HIDAsync = {
    open: async (path: string): Promise<NodeHidDevice> => {
      const fake = new FakeTransport(boardHandler(`SN-${path}`));
      const state = { closed: false, writes: [] as number[][] };
      this.open.set(path, state);
      let onData: (d: Uint8Array) => void = () => {};
      // hidapi hands over Bluetooth reports with their ID first.
      const bluetooth = this.devices.find((d) => d.path === path)?.busType === 2;
      fake.onReport((r) => {
        if (!bluetooth) return onData(r);
        onData(Uint8Array.of(5, 0, 0, 0, 0, 0, 0, 0, 0)); // a keyboard report, ignored
        onData(Uint8Array.of(protocol.HID_BLE_REPORT_ID, ...r));
      });
      return {
        write: async (data: number[]) => {
          state.writes.push(data);
          await fake.write(Uint8Array.from(data.slice(1)));
          return data.length;
        },
        on: (event: string, l: (x: never) => void) => {
          if (event === "data") onData = l as (d: Uint8Array) => void;
        },
        close: async () => {
          state.closed = true;
        },
      } as NodeHidDevice;
    },
  };
  async devicesAsync(): Promise<NodeHidDeviceInfo[]> {
    return this.devices;
  }
}

test("node-hid: picks the config interface by usage page, or by interface number on Linux", () => {
  const base = { vendorId: 0x1209, productId: 0x0001, path: "p" };
  assert.equal(isNodeHidBoard({ ...base, usagePage: 0xff00, usage: 0x44, interface: 2 }), true);
  assert.equal(isNodeHidBoard({ ...base, usagePage: 0x01, usage: 0x06, interface: 0 }), false);
  assert.equal(isNodeHidBoard({ ...base, usagePage: 0, interface: 2 }), true);
  assert.equal(isNodeHidBoard({ ...base, interface: 0 }), false);
  assert.equal(isNodeHidBoard({ ...base, productId: 0x0002, usagePage: 0xff00, usage: 0x44 }), false);
});

test("node-hid: scans open new boards and close unplugged ones", async () => {
  const hid = new FakeNodeHid();
  const config = { vendorId: 0x1209, productId: 0x0001, usagePage: 0xff00, usage: 0x44 };
  hid.devices = [
    { ...config, path: "a" },
    { vendorId: 0x1209, productId: 0x0001, usagePage: 1, usage: 6, path: "kbd" },
  ];
  const manager = new NodeHidBoardManager({ hid, pollMs: 60_000 });
  await manager.start();
  assert.deepEqual(manager.list().map((b) => b.info.id), ["SN-a"]);
  assert.ok(!hid.open.has("kbd"));
  const writes = hid.open.get("a")!.writes;
  assert.ok(writes.every((w) => w.length === protocol.REPORT_SIZE + 1 && w[0] === 0), "report ID 0 first");

  hid.devices = [{ ...config, path: "b" }];
  await manager.scan();
  assert.deepEqual(manager.list().map((b) => b.info.id), ["SN-b"]);
  assert.equal(hid.open.get("a")!.closed, true);
  await manager.close();
  assert.equal(hid.open.get("b")!.closed, true);
});

test("node-hid: a board over Bluetooth gets its report ID on every write", async () => {
  const hid = new FakeNodeHid();
  hid.devices = [{ vendorId: 0x1209, productId: 0x0001, usagePage: 0xff00, usage: 0x44, path: "bt", busType: 2 }];
  const manager = new NodeHidBoardManager({ hid, pollMs: 60_000 });
  await manager.start();
  const [board] = manager.list();
  assert.equal(board?.info.connection, "ble");
  assert.equal(board?.info.id, "SN-bt");
  const writes = hid.open.get("bt")!.writes;
  assert.ok(writes.length > 0 && writes.every((w) => w[0] === protocol.HID_BLE_REPORT_ID));
  await manager.close();
});
