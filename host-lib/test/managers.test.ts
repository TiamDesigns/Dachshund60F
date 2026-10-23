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
  type WebHidConnectionEvent,
  type WebHidDevice,
  type WebHidInputReportEvent,
} from "../src/node.js";
import { boardHandler, FakeTransport, type Handler } from "./fake-device.js";

const CONFIG = [{ usagePage: protocol.HID_USAGE_PAGE, usage: protocol.HID_USAGE }];

class FakeWebHidDevice implements WebHidDevice {
  opened = false;
  readonly productName = "Dachshund60F";
  private readonly listeners = new Set<(e: WebHidInputReportEvent) => void>();
  private readonly fake: FakeTransport;
  readonly sent: { reportId: number; length: number }[] = [];

  constructor(
    readonly vendorId: number,
    readonly productId: number,
    readonly collections: { usagePage?: number; usage?: number }[],
    handler: Handler,
  ) {
    this.fake = new FakeTransport(handler);
    this.fake.onReport((r) => {
      const event = { device: this, reportId: 0, data: new DataView(r.buffer) };
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

class FakeNodeHid implements NodeHidModule {
  devices: NodeHidDeviceInfo[] = [];
  readonly open = new Map<string, { closed: boolean; writes: number[][] }>();
  readonly HIDAsync = {
    open: async (path: string): Promise<NodeHidDevice> => {
      const fake = new FakeTransport(boardHandler(`SN-${path}`));
      const state = { closed: false, writes: [] as number[][] };
      this.open.set(path, state);
      let onData: (d: Uint8Array) => void = () => {};
      fake.onReport((r) => onData(r));
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
