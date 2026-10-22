import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

import {
  BoardError,
  DEFAULT_SETTINGS,
  defaultKeymap,
  isMatrixPositionUsed,
  isValidKeycode,
  MockBoardManager,
  type Board,
  type BoardEvent,
  type KeyboardDefinition,
} from "../src/index.js";

const definition = JSON.parse(
  readFileSync(new URL("../../../boards/Dachshund60F/keyboard.json", import.meta.url), "utf8"),
) as KeyboardDefinition;

function manager(): MockBoardManager {
  return new MockBoardManager({ definitions: { Dachshund60F: definition } });
}

async function rejectsWith(promise: Promise<unknown>, code: string): Promise<void> {
  await assert.rejects(promise, (err: unknown) => err instanceof BoardError && err.code === code);
}

test("default Dachshund60F keymap covers the matrix with valid keycodes", () => {
  const keymap = defaultKeymap("Dachshund60F")!;
  assert.equal(keymap.keys.length, 4);
  for (const layer of keymap.keys) {
    assert.equal(layer.length, definition.matrix.rows);
    layer.forEach((row, r) => {
      assert.equal(row.length, definition.matrix.cols);
      row.forEach((kc, c) => {
        assert.ok(isValidKeycode(kc, 4), `${kc} at (${r},${c})`);
        if (!isMatrixPositionUsed(definition, r, c)) {
          assert.equal(kc, "KC_NO", `unused position (${r},${c}) should be KC_NO`);
        }
      });
    });
  }
  // Every real switch has a non-empty action on the base layer.
  for (const k of definition.keys) {
    assert.notEqual(keymap.keys[0]![k.row]![k.col], "KC_NO", `base layer (${k.row},${k.col})`);
  }
  assert.equal(keymap.encoders.length, 4);
  for (const layer of keymap.encoders) assert.equal(layer.length, definition.encoders.length);
});

test("default keymaps are fresh copies", () => {
  const a = defaultKeymap("Dachshund60F")!;
  a.keys[0]![0]![0] = "KC_A";
  assert.equal(defaultKeymap("Dachshund60F")!.keys[0]![0]![0], "KC_ESC");
  assert.equal(defaultKeymap("NoSuchBoard"), undefined);
});

test("manager lists boards as they are plugged and unplugged", () => {
  const m = manager();
  const seen: number[] = [];
  m.subscribe((boards) => seen.push(boards.length));

  const usb = m.plug("Dachshund60F", "usb", { name: "Biscuit" });
  const dongle = m.plug("Dachshund60F", "dongle");
  assert.notEqual(usb.info.id, dongle.info.id);
  assert.equal(usb.info.connection, "usb");
  assert.equal(dongle.info.connection, "dongle");
  assert.deepEqual(m.list().map((b) => b.info.id), [usb.info.id, dongle.info.id]);

  m.unplug(usb.info.id);
  assert.deepEqual(m.list().map((b) => b.info.id), [dongle.info.id]);
  assert.deepEqual(seen, [1, 2, 1]);
  assert.throws(() => m.plug("Corgi40", "usb"), /no keyboard definition/);
  m.dispose();
});

test("edits stay live until saved, and revert restores the saved state", async () => {
  const board: Board = manager().plug("Dachshund60F", "usb");
  const events: string[] = [];
  board.subscribe((e: BoardEvent) => events.push(e.type));

  assert.equal(await board.hasUnsavedChanges(), false);
  await board.setKey({ layer: 0, row: 2, col: 1 }, "KC_B");
  assert.equal((await board.getKeymap()).keys[0]![2]![1], "KC_B");
  assert.equal(await board.hasUnsavedChanges(), true);

  await board.revert();
  assert.equal((await board.getKeymap()).keys[0]![2]![1], "KC_Q");
  assert.equal(await board.hasUnsavedChanges(), false);

  await board.setKey({ layer: 1, row: 2, col: 1 }, "MO(3)");
  await board.save();
  assert.equal(await board.hasUnsavedChanges(), false);
  await board.revert();
  assert.equal((await board.getKeymap()).keys[1]![2]![1], "MO(3)");

  assert.deepEqual(events, ["changed", "reverted", "changed", "saved", "reverted"]);
});

test("getKeymap returns a copy", async () => {
  const board = manager().plug("Dachshund60F", "usb");
  const keymap = await board.getKeymap();
  keymap.keys[0]![0]![0] = "KC_Z";
  assert.equal((await board.getKeymap()).keys[0]![0]![0], "KC_ESC");
});

test("setKey validates layer, position and keycode", async () => {
  const board = manager().plug("Dachshund60F", "usb");
  await rejectsWith(board.setKey({ layer: 4, row: 0, col: 0 }, "KC_A"), "out-of-range");
  await rejectsWith(board.setKey({ layer: -1, row: 0, col: 0 }, "KC_A"), "out-of-range");
  await rejectsWith(board.setKey({ layer: 0, row: 6, col: 0 }, "KC_A"), "out-of-range");
  await rejectsWith(board.setKey({ layer: 0, row: 0, col: 15 }, "KC_A"), "out-of-range");
  await rejectsWith(board.setKey({ layer: 0, row: 0.5, col: 0 }, "KC_A"), "out-of-range");
  // (3, 12) is an empty matrix slot on the Dachshund60F.
  await rejectsWith(board.setKey({ layer: 0, row: 3, col: 12 }, "KC_A"), "out-of-range");
  await rejectsWith(board.setKey({ layer: 0, row: 0, col: 0 }, "KC_NOPE"), "invalid-value");
  await rejectsWith(board.setKey({ layer: 0, row: 0, col: 0 }, "MO(4)"), "invalid-value");
  // Encoder push is a real switch.
  await board.setKey({ layer: 0, row: 0, col: 14 }, "KC_MPLY");
  assert.equal(await board.hasUnsavedChanges(), true);
});

test("setEncoder validates encoder and keycodes", async () => {
  const board = manager().plug("Dachshund60F", "usb");
  await board.setEncoder(2, 0, { cw: "KC_PGDN", ccw: "KC_PGUP" });
  assert.deepEqual((await board.getKeymap()).encoders[2]![0], { cw: "KC_PGDN", ccw: "KC_PGUP" });
  await rejectsWith(board.setEncoder(0, 1, { cw: "KC_A", ccw: "KC_B" }), "out-of-range");
  await rejectsWith(board.setEncoder(9, 0, { cw: "KC_A", ccw: "KC_B" }), "out-of-range");
  await rejectsWith(board.setEncoder(0, 0, { cw: "KC_A", ccw: "nope" }), "invalid-value");
});

test("settings are validated and saved with the keymap", async () => {
  const board = manager().plug("Dachshund60F", "dongle", { name: "Noodle" });
  assert.deepEqual(await board.getSettings(), { ...DEFAULT_SETTINGS, name: "Noodle" });

  const next = await board.setSettings({ debounceMs: 8, nkro: false });
  assert.equal(next.debounceMs, 8);
  assert.equal(next.nkro, false);
  assert.equal(await board.hasUnsavedChanges(), true);

  await rejectsWith(board.setSettings({ debounceMs: 0 }), "invalid-value");
  await rejectsWith(board.setSettings({ debounceMs: 2.5 }), "invalid-value");
  await rejectsWith(board.setSettings({ tappingTermMs: 99 }), "invalid-value");
  await rejectsWith(board.setSettings({ sleepTimeoutMin: 121 }), "invalid-value");
  await rejectsWith(board.setSettings({ indicatorBrightness: 101 }), "invalid-value");
  await rejectsWith(board.setSettings({ name: "Sir Wigglesworth III" }), "invalid-value");
  // A rejected patch changes nothing.
  assert.equal((await board.getSettings()).debounceMs, 8);

  await board.save();
  await board.factoryReset();
  assert.deepEqual(await board.getSettings(), DEFAULT_SETTINGS);
});

test("battery simulation charges on USB and drains wirelessly", async () => {
  const m = manager();
  const usb = m.plug("Dachshund60F", "usb", { battery: { percent: 99, charging: true } });
  const dongle = m.plug("Dachshund60F", "dongle", { battery: { percent: 6, charging: false } });
  const levels: number[] = [];
  dongle.subscribe((e) => {
    if (e.type === "battery") levels.push(e.battery.percent);
  });

  usb.tick();
  assert.deepEqual(await usb.getBattery(), { percent: 100, charging: false });
  usb.tick();
  assert.deepEqual(await usb.getBattery(), { percent: 100, charging: false });

  dongle.tick();
  dongle.tick();
  assert.deepEqual(await dongle.getBattery(), { percent: 5, charging: false });
  assert.deepEqual(levels, [5]); // floors at 5% and stops reporting
});

test("calls on an unplugged board fail with disconnected", async () => {
  const m = manager();
  const board = m.plug("Dachshund60F", "usb");
  const events: string[] = [];
  board.subscribe((e) => events.push(e.type));
  m.unplug(board.info.id);
  assert.deepEqual(events, ["disconnected"]);
  await rejectsWith(board.getKeymap(), "disconnected");
  await rejectsWith(board.setKey({ layer: 0, row: 0, col: 0 }, "KC_A"), "disconnected");
});
