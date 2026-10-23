// The same Board checks against the mock and against the protocol handler
// and store built for the host (kb_host_device), plus HidBoard's own cases.
import { test, type TestContext } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

import {
  BoardError,
  defaultKeymap,
  HidBoard,
  MockBoardManager,
  protocol,
  type Board,
  type BoardEvent,
  type KeyboardDefinition,
} from "../src/index.js";
import { boardHandler, FakeTransport } from "./fake-device.js";
import { HostDeviceTransport, hostDevicePath } from "./host-device.js";

const definition = JSON.parse(
  readFileSync(new URL("../../../boards/Dachshund60F/keyboard.json", import.meta.url), "utf8"),
) as KeyboardDefinition;

async function rejectsWith(promise: Promise<unknown>, code: string): Promise<void> {
  await assert.rejects(promise, (err: unknown) => err instanceof BoardError && err.code === code);
}

const host = hostDevicePath();

async function openHostDevice(t: TestContext): Promise<HidBoard> {
  const board = await HidBoard.open(new HostDeviceTransport(host.path!));
  t.after(() => board.close());
  return board;
}

const subjects: [string, (t: TestContext) => Promise<Board>, { skip?: string }][] = [
  ["mock", async () => new MockBoardManager({ definitions: { Dachshund60F: definition } }).plug("Dachshund60F", "usb"), {}],
  ["host-built firmware", openHostDevice, host.skip ? { skip: host.skip } : {}],
];

for (const [name, open, options] of subjects) {
  test(`${name}: reports the Dachshund60F and its default keymap`, options, async (t) => {
    const board = await open(t);
    assert.equal(board.info.model, "Dachshund60F");
    assert.equal(board.info.protocolVersion, protocol.PROTOCOL_VERSION);
    assert.equal(board.info.layerCount, 4);
    assert.ok(board.info.id.length > 0);
    assert.deepEqual((await board.getKeymap()).keys, defaultKeymap("Dachshund60F")!.keys);
  });

  test(`${name}: edits, save, revert and factory reset`, options, async (t) => {
    const board = await open(t);
    const events: string[] = [];
    board.subscribe((e: BoardEvent) => events.push(e.type));
    const key = { layer: 1, row: 2, col: 1 };
    const before = (await board.getKeymap()).keys[1]![2]![1];

    assert.equal(await board.hasUnsavedChanges(), false);
    await board.setKey(key, "TG(2)");
    assert.equal((await board.getKeymap()).keys[1]![2]![1], "TG(2)");
    assert.equal(await board.hasUnsavedChanges(), true);

    await board.revert();
    assert.equal((await board.getKeymap()).keys[1]![2]![1], before);

    await board.setKey(key, "MO(3)");
    await board.save();
    assert.equal(await board.hasUnsavedChanges(), false);
    await board.setKey(key, "KC_Z");
    await board.revert();
    assert.equal((await board.getKeymap()).keys[1]![2]![1], "MO(3)");

    await board.factoryReset();
    assert.deepEqual((await board.getKeymap()).keys, defaultKeymap("Dachshund60F")!.keys);
    assert.deepEqual(events.slice(0, 6), ["changed", "reverted", "changed", "saved", "changed", "reverted"]);
  });

  test(`${name}: rejects bad positions and keycodes`, options, async (t) => {
    const board = await open(t);
    await rejectsWith(board.setKey({ layer: 4, row: 0, col: 0 }, "KC_A"), "out-of-range");
    await rejectsWith(board.setKey({ layer: 0, row: 6, col: 0 }, "KC_A"), "out-of-range");
    await rejectsWith(board.setKey({ layer: 0, row: 0, col: 15 }, "KC_A"), "out-of-range");
    await rejectsWith(board.setKey({ layer: 0, row: 0, col: 0 }, "KC_NOPE"), "invalid-value");
    await rejectsWith(board.setKey({ layer: 0, row: 0, col: 0 }, "MO(4)"), "invalid-value");
    assert.equal(await board.hasUnsavedChanges(), false);
  });
}

test("host-built firmware: a saved keymap survives a reboot, an unsaved edit doesn't", host.skip ? { skip: host.skip } : {}, async (t) => {
  const board = await openHostDevice(t);
  await board.setKey({ layer: 0, row: 0, col: 0 }, "KC_GRV");
  await board.save();
  await board.setKey({ layer: 0, row: 0, col: 1 }, "KC_B");
  await board.reboot();
  const keys = (await board.getKeymap()).keys;
  assert.equal(keys[0]![0]![0], "KC_GRV");
  assert.equal(keys[0]![0]![1], "KC_F1");
});

test("host-built firmware: info from the handshake", host.skip ? { skip: host.skip } : {}, async (t) => {
  const board = await openHostDevice(t);
  assert.equal(board.info.id, "HOST0001");
  assert.equal(board.build, "host");
  assert.deepEqual(board.info.capabilities, ["keymap", "persistence", "identify"]);
  assert.deepEqual((await board.getKeymap()).encoders, [[], [], [], []]);
  await board.identify();
  await rejectsWith(board.getSettings(), "not-supported");
  await rejectsWith(board.setEncoder(0, 0, { cw: "KC_A", ccw: "KC_B" }), "not-supported");
  assert.equal(await board.getBattery(), null);
});

test("a board with another protocol version opens but refuses everything", async () => {
  const t = new FakeTransport(boardHandler("SER1", protocol.PROTOCOL_VERSION + 1));
  const board = await HidBoard.open(t, { serialNumber: "USB-SERIAL" });
  assert.equal(board.compatible, false);
  assert.equal(board.info.protocolVersion, protocol.PROTOCOL_VERSION + 1);
  assert.equal(board.info.id, "USB-SERIAL");
  const before = t.requests.length;
  await rejectsWith(board.getKeymap(), "version-mismatch");
  await rejectsWith(board.setKey({ layer: 0, row: 0, col: 0 }, "KC_A"), "version-mismatch");
  await rejectsWith(board.save(), "version-mismatch");
  await rejectsWith(board.factoryReset(), "version-mismatch");
  await rejectsWith(board.reboot(), "version-mismatch");
  assert.equal(t.requests.length, before, "nothing sent after the handshake");
});

test("a board missing a capability says not-supported without asking it", async () => {
  const t = new FakeTransport(boardHandler("SER1"));
  const board = await HidBoard.open(t);
  assert.equal(board.info.id, "SER1");
  assert.equal(board.info.firmwareVersion, "1.2.3");
  const before = t.requests.length;
  await rejectsWith(board.save(), "not-supported");
  assert.equal(t.requests.length, before);
});

test("closing the transport reports disconnected", async () => {
  const t = new FakeTransport(boardHandler("SER1"));
  const board = await HidBoard.open(t);
  const events: string[] = [];
  board.subscribe((e) => events.push(e.type));
  await t.close();
  assert.deepEqual(events, ["disconnected"]);
  assert.equal(board.connected, false);
  await rejectsWith(board.identify(), "disconnected");
});
