import { test } from "node:test";
import assert from "node:assert/strict";

import {
  allKeycodes,
  isValidKeycode,
  KEYCODE_CATEGORIES,
  keycodeInfo,
  keycodeLayer,
  NO_KEY,
  TRANSPARENT,
} from "../src/index.js";

test("keycode ids are unique", () => {
  const ids = allKeycodes(4).map((k) => k.id);
  assert.equal(new Set(ids).size, ids.length);
});

test("every keycode is in a known category and has a name", () => {
  const categories = new Set(KEYCODE_CATEGORIES.map((c) => c.id));
  for (const k of allKeycodes(4)) {
    assert.ok(categories.has(k.category), `${k.id} has category ${k.category}`);
    assert.ok(k.name.length > 0, `${k.id} has no name`);
  }
});

test("every category has keycodes", () => {
  const used = new Set(allKeycodes(4).map((k) => k.category));
  for (const c of KEYCODE_CATEGORIES) assert.ok(used.has(c.id), `${c.id} is empty`);
});

test("layer keycodes depend on the layer count", () => {
  assert.ok(isValidKeycode("MO(3)", 4));
  assert.ok(!isValidKeycode("MO(4)", 4));
  assert.ok(!isValidKeycode("MO(1)", 1));
  assert.equal(keycodeInfo("TG(2)", 4)?.label, "TG 2");
  assert.equal(keycodeInfo("TG(2)", 4)?.category, "layers");
  assert.equal(keycodeLayer("OSL(3)"), 3);
  assert.equal(keycodeLayer("KC_A"), undefined);
  assert.equal(keycodeLayer("BT_SEL(1)"), undefined);
});

test("Bluetooth profile keycodes are limited to the profile count", () => {
  assert.equal(keycodeInfo("BT_SEL(0)", 4)?.label, "BT1");
  assert.ok(isValidKeycode("BT_SEL(2)", 4));
  assert.ok(!isValidKeycode("BT_SEL(3)", 4));
});

test("unknown and malformed keycodes are rejected", () => {
  for (const id of ["", "KC_", "KC_FOO", "MO()", "MO(-1)", "MO(1", "XX(1)", "mo(1)", "KC_A "]) {
    assert.ok(!isValidKeycode(id, 4), `${JSON.stringify(id)} should be invalid`);
  }
});

test("no-op and transparent keys exist", () => {
  assert.equal(keycodeInfo(NO_KEY, 4)?.label, "");
  assert.equal(keycodeInfo(TRANSPARENT, 4)?.label, "▽");
});
