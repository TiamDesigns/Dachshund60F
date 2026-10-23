import { test } from "node:test";
import assert from "node:assert/strict";

import { allKeycodes, decodeKeycode, defaultKeymap, encodeKeycode, KeycodeError } from "../src/index.js";

test("every 16-bit number decodes to an id that encodes back to it", () => {
  for (let n = 0; n <= 0xffff; n++) {
    const id = decodeKeycode(n);
    assert.equal(encodeKeycode(id), n, `${n.toString(16)} -> ${id}`);
  }
});

test("every catalog id encodes and decodes back to itself", () => {
  for (const { id } of allKeycodes(16)) {
    const n = encodeKeycode(id);
    assert.ok(!decodeKeycode(n).startsWith("RAW("), `${id} decodes to RAW`);
    assert.equal(decodeKeycode(n), id);
  }
});

test("the default keymap encodes with no RAW keycodes", () => {
  for (const layer of defaultKeymap("Dachshund60F")!.keys) {
    for (const row of layer) {
      for (const id of row) assert.equal(decodeKeycode(encodeKeycode(id)), id);
    }
  }
});

test("known numbers", () => {
  assert.equal(encodeKeycode("KC_NO"), 0x0000);
  assert.equal(encodeKeycode("KC_TRNS"), 0x0001);
  assert.equal(encodeKeycode("KC_A"), 0x0004);
  assert.equal(encodeKeycode("MO(1)"), 0x4001);
  assert.equal(encodeKeycode("TG(2)"), 0x4102);
  assert.equal(encodeKeycode("LSFT(KC_1)"), 0x221e);
  assert.equal(encodeKeycode("LT(1, KC_SPC)"), 0x612c);
  assert.equal(encodeKeycode("LCTL_T(KC_A)"), 0x8104);
  assert.equal(encodeKeycode("MT(MOD_LSFT,KC_A)"), 0x8204);
  assert.equal(encodeKeycode("OSM(MOD_RSFT)"), 0x5012);
  assert.equal(encodeKeycode("RAW(0x7fff)"), 0x7fff);
  assert.equal(decodeKeycode(0x612c), "LT(1,KC_SPC)");
});

test("unknown ids and out-of-range arguments throw KeycodeError", () => {
  for (const id of ["KC_NOPE", "MO(256)", "BT_SEL(9)", "LT(16,KC_A)", "LSFT(MO(1))", "RAW(0x10000)", "MO()", "MO"]) {
    assert.throws(() => encodeKeycode(id), KeycodeError, id);
  }
  assert.throws(() => decodeKeycode(0x10000), KeycodeError);
  assert.throws(() => decodeKeycode(-1), KeycodeError);
});
