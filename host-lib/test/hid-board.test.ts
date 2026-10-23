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
  assert.deepEqual(board.info.capabilities, ["keymap", "persistence", "encoders", "macros", "advancedKeys", "identify"]);
  assert.deepEqual((await board.getKeymap()).encoders, defaultKeymap("Dachshund60F")!.encoders);
  await board.identify();
  await rejectsWith(board.getSettings(), "not-supported");
  assert.equal(await board.getBattery(), null);
});

test("host-built firmware: encoders", host.skip ? { skip: host.skip } : {}, async (t) => {
  const board = await openHostDevice(t);
  await board.setEncoder(2, 0, { cw: "KC_PGDN", ccw: "KC_PGUP" });
  assert.deepEqual((await board.getKeymap()).encoders[2], [{ cw: "KC_PGDN", ccw: "KC_PGUP" }]);
  assert.equal(await board.hasUnsavedChanges(), true);
  await rejectsWith(board.setEncoder(4, 0, { cw: "KC_A", ccw: "KC_B" }), "out-of-range");
  await rejectsWith(board.setEncoder(0, 1, { cw: "KC_A", ccw: "KC_B" }), "out-of-range");
  await rejectsWith(board.setEncoder(0, 0, { cw: "TD(0)", ccw: "KC_B" }), "invalid-value");
  await rejectsWith(board.setEncoder(0, 0, { cw: "KC_NOPE", ccw: "KC_B" }), "invalid-value");
});

test("host-built firmware: advanced keys", host.skip ? { skip: host.skip } : {}, async (t) => {
  const board = await openHostDevice(t);
  const before = await board.getAdvancedKeys();
  assert.equal(before.combos.length, 16);
  assert.deepEqual(before.combos[0], { keys: [], result: "KC_NO", timeoutMs: 0, layers: [] });
  assert.equal(before.options.autoShiftMs, 175);
  assert.deepEqual(before.tapHold, new Array(16).fill(null));

  const combo = { keys: [{ row: 2, col: 1 }, { row: 2, col: 2 }], result: "KC_ESC", timeoutMs: 40, layers: [0, 1] };
  await board.setAdvancedEntry("combos", 5, combo);
  const dance = { tap: "KC_A", hold: "KC_LCTL", doubleTap: "MO(1)", tripleTap: "KC_NO", termMs: 0 };
  await board.setAdvancedEntry("tapDances", 0, dance);
  const override = { triggerMods: 0x02, triggerKey: "KC_BSPC", replacement: "KC_DEL", layers: [] };
  await board.setAdvancedEntry("keyOverrides", 1, override);
  const seq = { keys: ["KC_G", "KC_H"], result: "LSFT(KC_1)" };
  await board.setAdvancedEntry("leader", 2, seq);
  const rule = {
    row: 3, col: 0, permissiveHold: true, holdOnOtherKeyPress: false, retroTap: true,
    tappingTermMs: 220, quickTapMs: 100,
  };
  await board.setAdvancedEntry("tapHold", 3, rule);
  const options = await board.setAdvancedOptions({ autoShift: true, mouse: { ...before.options.mouse, maxSpeed: 40 } });
  assert.equal(options.autoShift, true);

  const after = await board.getAdvancedKeys();
  assert.deepEqual(after.combos[5], combo);
  assert.deepEqual(after.tapDances[0], dance);
  assert.deepEqual(after.keyOverrides[1], override);
  assert.deepEqual(after.leader[2], seq);
  assert.deepEqual(after.tapHold[3], rule);
  assert.equal(after.options.mouse.maxSpeed, 40);
  assert.equal(after.options.leaderTimeoutMs, before.options.leaderTimeoutMs);

  // The board checks what the codec can't.
  await rejectsWith(board.setAdvancedEntry("combos", 0, { ...combo, keys: [{ row: 2, col: 1 }] }), "invalid-value");
  await rejectsWith(board.setAdvancedEntry("combos", 0, { ...combo, keys: [{ row: 6, col: 0 }, { row: 0, col: 0 }] }), "invalid-value");
  await rejectsWith(board.setAdvancedEntry("tapDances", 1, { ...dance, hold: "LT(1,KC_A)" }), "invalid-value");
  await rejectsWith(board.setAdvancedEntry("combos", 16, combo), "out-of-range");
  await rejectsWith(board.setAdvancedOptions({ autoShiftMs: 10 }), "invalid-value");
  assert.deepEqual((await board.getAdvancedKeys()).combos[0], before.combos[0]);

  // Saved and reloaded with the keymap.
  await board.save();
  await board.reboot();
  assert.deepEqual((await board.getAdvancedKeys()).combos[5], combo);
  await board.factoryReset();
  assert.deepEqual(await board.getAdvancedKeys(), before);
});

test("host-built firmware: macros", host.skip ? { skip: host.skip } : {}, async (t) => {
  const board = await openHostDevice(t);
  assert.deepEqual(await board.getMacros(), []);
  const macros = [
    [{ type: "text" as const, text: "Hello, world!" }, { type: "tap" as const, keycode: "KC_ENT" }],
    [],
    [
      { type: "down" as const, keycode: "KC_LCTL" },
      { type: "tap" as const, keycode: "KC_C" },
      { type: "up" as const, keycode: "KC_LCTL" },
      { type: "delay" as const, ms: 300 },
      { type: "tap" as const, keycode: "LSFT(KC_INS)" },
    ],
  ];
  await board.setMacros(macros);
  assert.deepEqual(await board.getMacros(), macros);
  assert.equal(await board.hasUnsavedChanges(), true);

  // Shorter macros leave nothing of the longer ones behind.
  await board.setMacros([[{ type: "text", text: "x" }]]);
  assert.deepEqual(await board.getMacros(), [[{ type: "text", text: "x" }]]);

  await rejectsWith(board.setMacros([[{ type: "tap", keycode: "M(1)" }]]), "invalid-value");
  await rejectsWith(board.setMacros([[{ type: "text", text: "é" }]]), "invalid-value");
  await rejectsWith(board.setMacros([[{ type: "text", text: "a".repeat(1024) }]]), "storage-full");
  assert.deepEqual(await board.getMacros(), [[{ type: "text", text: "x" }]]);

  await board.revert();
  assert.deepEqual(await board.getMacros(), []);
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
  await rejectsWith(board.setEncoder(0, 0, { cw: "KC_A", ccw: "KC_B" }), "not-supported");
  await rejectsWith(board.getAdvancedKeys(), "not-supported");
  await rejectsWith(board.getMacros(), "not-supported");
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
