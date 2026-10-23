import { test } from "node:test";
import assert from "node:assert/strict";

import * as codec from "../src/advanced-codec.js";
import { decodeKeycode, encodeKeycode, KeycodeError, protocol } from "../src/index.js";

test("M(n), TD(n) and the new action keys round-trip", () => {
  assert.equal(encodeKeycode("M(0)"), 0xa000);
  assert.equal(encodeKeycode("M(31)"), 0xa01f);
  assert.equal(encodeKeycode("TD(15)"), 0xa10f);
  assert.equal(decodeKeycode(0xa005), "M(5)");
  assert.equal(decodeKeycode(0xa10f), "TD(15)");
  assert.throws(() => encodeKeycode("M(32)"), KeycodeError);
  assert.throws(() => encodeKeycode("TD(16)"), KeycodeError);
  for (const id of ["AS_TOGG", "DM_REC1", "DM_PLY2", "MS_WHLR", "MS_BTN5"]) {
    assert.equal(decodeKeycode(encodeKeycode(id)), id);
  }
});

test("entries have the sizes protocol.yaml gives", () => {
  const T = protocol.FeatureTable;
  const options = {
    leaderTimeoutMs: 300, capsWordOnBothShifts: true, capsWordIdleMs: 5000, autoShift: false, autoShiftMs: 175,
    mouse: { intervalMs: 16, moveDelta: 4, maxSpeed: 20, timeToMaxMs: 1000, wheelIntervalMs: 80 },
  };
  assert.equal(codec.encodeOptions(options).length, T.options.size);
  assert.deepEqual(codec.decodeOptions(codec.encodeOptions(options)), options);
  const combo = { keys: [{ row: 0, col: 1 }, { row: 5, col: 14 }], result: "KC_ESC", timeoutMs: 0, layers: [0, 15] };
  assert.equal(codec.encodeCombo(combo).length, T.combo.size);
  assert.deepEqual(codec.decodeCombo(codec.encodeCombo(combo)), combo);
  assert.equal(codec.encodeTapDance({ tap: "KC_A", hold: "KC_NO", doubleTap: "KC_B", tripleTap: "KC_NO", termMs: 0 }).length, T.tapDance.size);
  assert.equal(codec.encodeLeader({ keys: ["KC_A"], result: "KC_B" }).length, T.leader.size);
  assert.equal(codec.encodeTapHold(null).length, T.tapHold.size);
  assert.equal(codec.decodeTapHold(codec.encodeTapHold(null)), null);
  assert.equal(codec.encodeEncoder({ cw: "KC_VOLU", ccw: "KC_TRNS" }).length, T.encoder.size);
});

test("entry fields land where the firmware reads them", () => {
  // Combo: row, col x4, result, timeout, layers (all little-endian).
  const b = codec.encodeCombo({ keys: [{ row: 1, col: 2 }, { row: 3, col: 4 }], result: "KC_TAB", timeoutMs: 0x1234, layers: [1] });
  assert.deepEqual([...b], [1, 2, 3, 4, 0xff, 0xff, 0xff, 0xff, 0x2b, 0, 0x34, 0x12, 2, 0]);
  const o = codec.encodeKeyOverride({ triggerMods: 0x02, triggerKey: "KC_BSPC", replacement: "KC_DEL", layers: [] });
  assert.deepEqual([...o], [0x02, 0x2a, 0x4c, 0, 0, 0]);
  const r = codec.encodeTapHold({ row: 3, col: 0, permissiveHold: true, holdOnOtherKeyPress: true, retroTap: false, tappingTermMs: 200, quickTapMs: 0 });
  assert.deepEqual([...r], [3, 0, 3, 200, 0, 0, 0]);
});

test("bad values are KeycodeErrors", () => {
  assert.throws(() => codec.encodeLeader({ keys: ["MO(1)"], result: "KC_A" }), KeycodeError);
  assert.throws(() => codec.encodeLeader({ keys: ["KC_A", "KC_B", "KC_C", "KC_D", "KC_E"], result: "KC_A" }), KeycodeError);
  assert.throws(() => codec.encodeCombo({ keys: [], result: "KC_A", timeoutMs: 0, layers: [16] }), KeycodeError);
  assert.throws(() => codec.encodeCombo({ keys: [], result: "KC_A", timeoutMs: 70000, layers: [] }), KeycodeError);
  // An unused override needs no trigger key.
  assert.deepEqual([...codec.encodeKeyOverride({ triggerMods: 0, triggerKey: "KC_NO", replacement: "KC_NO", layers: [] })], [0, 0, 0, 0, 0, 0]);
});

test("macros: steps to bytes and back", () => {
  const macros = [
    [{ type: "text" as const, text: "ab" }, { type: "tap" as const, keycode: "KC_ENT" }],
    [],
    [{ type: "delay" as const, ms: 500 }, { type: "down" as const, keycode: "KC_LSFT" }],
  ];
  const bytes = codec.encodeMacros(macros);
  assert.deepEqual([...bytes], [0x61, 0x62, 0x01, 0x28, 0, 0x00, 0x00, 0x04, 0xf4, 0x01, 0x02, 0xe1, 0, 0x00]);
  const padded = new Uint8Array(64);
  padded.set(bytes);
  assert.deepEqual(codec.decodeMacros(padded), macros);
  assert.deepEqual(codec.decodeMacros(new Uint8Array(16)), []);
  assert.throws(() => codec.encodeMacros([[{ type: "text", text: "\n" }]]), KeycodeError);
  assert.throws(() => codec.encodeMacros([[{ type: "delay", ms: -1 }]]), KeycodeError);
  // A cut-off step ends the list.
  assert.deepEqual(codec.decodeMacros(Uint8Array.from([0x61, 0x01, 0x04])), [[{ type: "text", text: "a" }]]);
});
