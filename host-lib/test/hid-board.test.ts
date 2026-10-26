// The same Board checks against the mock and against the protocol handler
// and store built for the host (kb_host_device), plus HidBoard's own cases.
import { test, type TestContext } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

import {
  backupBoard,
  BoardError,
  defaultKeymap,
  DEFAULT_ADVANCED_OPTIONS,
  restoreBoard,
  HidBoard,
  MockBoardManager,
  protocol,
  type Board,
  type BoardEvent,
  type KeyboardDefinition,
} from "../src/index.js";
import * as codec from "../src/advanced-codec.js";
import { boardHandler, FakeTransport, response } from "./fake-device.js";
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

  test(`${name}: outputs and Bluetooth host profiles`, options, async (t) => {
    const board = await open(t);
    const c = await board.getConnection!();
    assert.equal(c.active, "usb");
    assert.equal(c.preferred, "usb");
    assert.equal(c.usb, true);
    assert.equal(c.profile, 0);
    assert.equal(c.paired.length, 3);
    assert.equal(c.paired[1], true);

    // Preferring Bluetooth moves the keys there only once a host is on it.
    await board.setOutput!("ble");
    const after = await board.getConnection!();
    assert.equal(after.preferred, "ble");
    assert.equal(after.active, after.bluetooth === "connected" ? "ble" : "usb");

    await board.selectProfile!(2);
    assert.equal((await board.getConnection!()).profile, 2);
    await board.clearProfile!(1);
    assert.equal((await board.getConnection!()).paired[1], false);
    await rejectsWith(board.selectProfile!(3), "out-of-range");
    await rejectsWith(board.clearProfile!(-1), "out-of-range");
    await rejectsWith(board.setOutput!("serial" as "usb"), "invalid-value");
    // None of it is an unsaved edit: the board keeps it by itself.
    assert.equal(await board.hasUnsavedChanges(), false);
  });

  test(`${name}: the 2.4 GHz dongle`, options, async (t) => {
    const board = await open(t);
    // Paired, but the dongle isn't plugged in here: keys stay on USB even when it is preferred.
    assert.deepEqual(await board.getDongle!(), { paired: true, link: "searching", pairing: false });
    await board.setOutput!("dongle");
    const c = await board.getConnection!();
    assert.equal(c.preferred, "dongle");
    assert.equal(c.active, "usb");
    await board.setOutput!("usb");

    await board.pairDongle!("start");
    assert.equal((await board.getDongle!()).pairing, true);
    await board.pairDongle!("stop");
    assert.equal((await board.getDongle!()).pairing, false);
    await board.pairDongle!("forget");
    assert.deepEqual(await board.getDongle!(), { paired: false, link: "off", pairing: false });
    await rejectsWith(board.pairDongle!("again" as "start"), "invalid-value");
  });

  test(`${name}: battery and input timing`, options, async (t) => {
    const board = await open(t);
    const battery = await board.getBattery();
    if (battery) {
      assert.ok(battery.percent >= 0 && battery.percent <= 100);
      assert.equal(typeof battery.charging, "boolean");
    }
    const timing = await board.getTiming!();
    assert.equal(timing.scanPeriodUs, 1000);
    assert.ok(timing.scanAvgUs <= timing.scanMaxUs && timing.reportAvgUs <= timing.reportMaxUs);
    assert.ok(timing.keyWakes <= timing.scans);
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
  assert.deepEqual(board.info.capabilities, [
    "keymap",
    "persistence",
    "encoders",
    "macros",
    "advancedKeys",
    "indicators",
    "settings",
    "battery",
    "identify",
    "bluetooth",
    "timing",
    "dongle",
    "featureSwitches",
  ]);
  assert.deepEqual((await board.getKeymap()).encoders, defaultKeymap("Dachshund60F")!.encoders);
  await board.identify();
  // kb_host_device: on USB and charging, last read at 72% on battery.
  assert.deepEqual(await board.getBattery(), { percent: 72, charging: true, millivolts: 3960, usbPower: true });
  const timing = await board.getTiming(true);
  assert.deepEqual(timing, {
    scanPeriodUs: 1000,
    scanAvgUs: 95,
    scanMaxUs: 140,
    reportAvgUs: 30,
    reportMaxUs: 80,
    keyWakes: 12,
    scans: 5000,
  });
  assert.equal((await board.getTiming()).scans, 0);
});

test("host-built firmware: saved settings and colours survive a reboot", host.skip ? { skip: host.skip } : {}, async (t) => {
  const board = await openHostDevice(t);
  await board.setSettings({ name: "Brötchen", tappingTermMs: 180 });
  await board.setIndicators({ caps: "#123456", layers: ["#000000", "#ff0000", "#00ff00", "#0000ff"] });
  await board.save();
  await board.setSettings({ name: "unsaved" });
  await board.reboot();
  const settings = await board.getSettings();
  assert.equal(settings.name, "Brötchen");
  assert.equal(settings.tappingTermMs, 180);
  const colours = await board.getIndicators();
  assert.equal(colours.caps, "#123456");
  assert.deepEqual(colours.layers, ["#000000", "#ff0000", "#00ff00", "#0000ff"]);
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

// The advanced-key checks the mock makes (advanced.ts) must match the firmware's.
for (const [name, open, options] of subjects) {
  test(`${name}: advanced keys`, options, async (t) => {
    const board = (await open(t)) as Required<Board>;
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
    const opts = await board.setAdvancedOptions({ autoShift: true, mouse: { ...before.options.mouse, maxSpeed: 40 } });
    assert.equal(opts.autoShift, true);
    assert.deepEqual(before.options.off, { tapHold: false, tapDance: false, combos: false });
    await board.setAdvancedOptions({ off: { tapHold: true, tapDance: true, combos: false } });

    const after = await board.getAdvancedKeys();
    assert.deepEqual(after.combos[5], combo);
    assert.deepEqual(after.tapDances[0], dance);
    assert.deepEqual(after.keyOverrides[1], override);
    assert.deepEqual(after.leader[2], seq);
    assert.deepEqual(after.tapHold[3], rule);
    assert.equal(after.options.mouse.maxSpeed, 40);
    assert.deepEqual(after.options.off, { tapHold: true, tapDance: true, combos: false });
    assert.equal(after.options.leaderTimeoutMs, before.options.leaderTimeoutMs);

    // The board checks what the codec can't.
    await rejectsWith(board.setAdvancedEntry("combos", 0, { ...combo, keys: [{ row: 2, col: 1 }] }), "invalid-value");
    await rejectsWith(board.setAdvancedEntry("combos", 0, { ...combo, keys: [{ row: 6, col: 0 }, { row: 0, col: 0 }] }), "invalid-value");
    await rejectsWith(board.setAdvancedEntry("tapDances", 1, { ...dance, hold: "LT(1,KC_A)" }), "invalid-value");
    await rejectsWith(board.setAdvancedEntry("combos", 0, { ...combo, result: "TD(0)" }), "invalid-value");
    await rejectsWith(board.setAdvancedEntry("tapDances", 1, { ...dance, tap: "LCTL_T(KC_A)" }), "invalid-value");
    await rejectsWith(board.setAdvancedEntry("keyOverrides", 0, { ...override, replacement: "MO(1)" }), "invalid-value");
    await rejectsWith(board.setAdvancedEntry("keyOverrides", 0, { ...override, triggerMods: 0 }), "invalid-value");
    await rejectsWith(board.setAdvancedEntry("tapHold", 0, { ...rule, tappingTermMs: 20 }), "invalid-value");
    await board.setAdvancedEntry("leader", 3, { keys: ["KC_T"], result: "TD(1)" });
    await board.setAdvancedEntry("leader", 3, { keys: [], result: "KC_NO" });
    await rejectsWith(board.setAdvancedEntry("combos", 16, combo), "out-of-range");
    await rejectsWith(board.setAdvancedOptions({ autoShiftMs: 10 }), "invalid-value");
    assert.deepEqual((await board.getAdvancedKeys()).combos[0], before.combos[0]);

    // Saved (and reloaded with the keymap, on a board that restarts).
    await board.save();
    await board.setAdvancedEntry("combos", 5, before.combos[0]!);
    await board.revert();
    if (board instanceof HidBoard) await board.reboot();
    assert.deepEqual((await board.getAdvancedKeys()).combos[5], combo);
    await board.factoryReset();
    assert.deepEqual(await board.getAdvancedKeys(), before);
  });

  test(`${name}: settings`, options, async (t) => {
    const board = await open(t);
    const before = await board.getSettings();
    assert.equal(before.debounceAlgorithm, "eager-press");
    const next = await board.setSettings({
      name: "Wiener 🐶",
      debounceAlgorithm: "defer-per-key",
      debounceMs: 0,
      tappingTermMs: 2000,
      nkro: false,
      sleepTimeoutMin: 240,
      indicatorBrightness: 100,
    });
    assert.deepEqual(await board.getSettings(), next);
    assert.equal(next.name, "Wiener 🐶");
    assert.equal(await board.hasUnsavedChanges(), true);

    for (const bad of [
      { name: "x".repeat(17) },
      { name: "🐶🐶🐶🐶🐶" }, // 20 bytes
      { debounceAlgorithm: "slow" as never },
      { debounceMs: 101 },
      { tappingTermMs: 49 },
      { tappingTermMs: 2001 },
      { sleepTimeoutMin: 241 },
      { indicatorBrightness: 101 },
      { debounceMs: 1.5 },
    ]) {
      await rejectsWith(board.setSettings(bad), "invalid-value");
    }
    assert.deepEqual(await board.getSettings(), next);

    await board.revert();
    assert.deepEqual(await board.getSettings(), before);
  });

  test(`${name}: indicator colours`, options, async (t) => {
    const board = (await open(t)) as Required<Board>;
    const before = await board.getIndicators();
    assert.equal(before.layers.length, 4);
    assert.equal(before.layers[0], "#000000");
    assert.equal(before.caps, "#ffdcb4");
    assert.equal(before.batteryLowPercent, 15);

    const next = await board.setIndicators({ battery: "#00ff00", batteryLowPercent: 25 });
    assert.equal(next.layers[1], before.layers[1]);
    await board.setIndicators({ layers: ["#000000", "#ff0000", before.layers[2]!, "#abcdef"] });
    const got = await board.getIndicators();
    assert.equal(got.battery, "#00ff00");
    assert.equal(got.batteryLowPercent, 25);
    assert.deepEqual(got.layers, ["#000000", "#ff0000", before.layers[2], "#abcdef"]);

    for (const bad of [
      { caps: "red" },
      { num: "#FFFFFF" },
      { batteryLowPercent: 101 },
      { layers: ["#000000"] },
      { layers: ["#000000", "#000000", "#000000", "#00000g"] },
    ]) {
      await rejectsWith(board.setIndicators(bad), "invalid-value");
    }

    await board.factoryReset();
    assert.deepEqual(await board.getIndicators(), before);
  });

  test(`${name}: back up, factory reset and restore`, options, async (t) => {
    const board = (await open(t)) as Required<Board>;
    const fresh = await backupBoard(board);
    assert.equal(fresh.board.model, "Dachshund60F");
    assert.ok(fresh.settings && fresh.advanced && fresh.macros && fresh.indicators);

    await board.setKey({ layer: 1, row: 1, col: 1 }, "KC_MUTE");
    await board.setEncoder(1, 0, { cw: "KC_RGHT", ccw: "KC_LEFT" });
    await board.setSettings({ name: "Pretzel", tappingTermMs: 220 });
    await board.setAdvancedEntry("tapDances", 3, { tap: "KC_A", hold: "KC_B", doubleTap: "KC_NO", tripleTap: "KC_NO", termMs: 0 });
    await board.setAdvancedOptions({ autoShift: true });
    await board.setMacros([[{ type: "text", text: "woof" }]]);
    await board.setIndicators({ caps: "#00ff00", layers: ["#000000", "#010203", "#040506", "#070809"] });
    await board.save();
    const backup = JSON.parse(JSON.stringify(await backupBoard(board))) as typeof fresh; // as from a file

    // Nothing to do on a board that still has it all.
    assert.equal(await restoreBoard(board, backup), 0);

    await board.factoryReset();
    assert.deepEqual({ ...(await backupBoard(board)), takenAt: "" }, { ...fresh, takenAt: "" });
    const writes = await restoreBoard(board, backup);
    assert.equal(writes, 7);
    assert.equal(await board.hasUnsavedChanges(), false, "restore saves");
    await board.revert();
    assert.deepEqual({ ...(await backupBoard(board)), takenAt: "" }, { ...backup, takenAt: "" });

    await rejectsWith(restoreBoard(board, { ...backup, board: { ...backup.board, model: "Corgi65" } }), "invalid-value");
  });

  test(`${name}: macros`, options, async (t) => {
    const board = (await open(t)) as Required<Board>;
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
    await rejectsWith(board.setMacros([[{ type: "tap", keycode: "LCTL_T(KC_A)" }]]), "invalid-value");
    await rejectsWith(board.setMacros([[{ type: "tap", keycode: "MS_BTN1" }]]), "invalid-value");
    await rejectsWith(board.setMacros([[{ type: "text", text: "a".repeat(1024) }]]), "storage-full");
    assert.deepEqual(await board.getMacros(), [[{ type: "text", text: "x" }]]);
    await board.setMacros([[{ type: "tap", keycode: "KC_VOLU" }, { type: "tap", keycode: "RALT(KC_E)" }]]);

    await board.revert();
    assert.deepEqual(await board.getMacros(), []);
  });
}

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
  await rejectsWith(board.getSettings(), "not-supported");
  await rejectsWith(board.setSettings({ nkro: false }), "not-supported");
  await rejectsWith(board.getIndicators(), "not-supported");
  assert.equal(t.requests.length, before);
});

test("firmware without feature switches: a short options entry, and nothing switched off", async () => {
  const { Capability, Command, FeatureTable } = protocol;
  const fallback = boardHandler("SER1", protocol.PROTOCOL_VERSION, Capability.keymap | Capability.advancedKeys);
  const options = codec.encodeOptions({ ...DEFAULT_ADVANCED_OPTIONS, autoShiftMs: 200 }).subarray(0, codec.OPTIONS_SIZE_V1);
  const written: Uint8Array[] = [];
  const t = new FakeTransport((req) => {
    if (req.command === Command.getFeature) {
      const { table, index } = protocol.decodeGetFeatureRequest(req.payload);
      assert.equal(table, FeatureTable.options.id);
      return response(req.command, req.sequence, 0, protocol.encodeGetFeatureResponse({ table, index, data: options }));
    }
    if (req.command === Command.setFeature) {
      written.push(protocol.decodeSetFeatureRequest(req.payload).data);
      return response(req.command, req.sequence, 0);
    }
    return fallback(req);
  });
  const board = await HidBoard.open(t);
  const current = await board.setAdvancedOptions({ autoShift: true });
  assert.equal(current.autoShiftMs, 200);
  assert.deepEqual(current.off, { tapHold: false, tapDance: false, combos: false });
  assert.equal(written[0]!.length, codec.OPTIONS_SIZE_V1);
  await rejectsWith(board.setAdvancedOptions({ off: { tapHold: true, tapDance: false, combos: false } }), "not-supported");
  assert.equal(written.length, 1);
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
