import { test } from "node:test";
import assert from "node:assert/strict";

import { countAssigned, resolveKey } from "./keymap.ts";

const T = "KC_TRNS";
const keys = [
  [["KC_A", "KC_B", T]],
  [[T, "KC_1", T]],
  [[T, T, T]],
];

test("own key wins when it is not transparent", () => {
  assert.deepEqual(resolveKey(keys, 1, 0, 1), { own: "KC_1", effective: "KC_1", fromLayer: 1 });
  assert.deepEqual(resolveKey(keys, 0, 0, 0), { own: "KC_A", effective: "KC_A", fromLayer: 0 });
});

test("transparent keys fall through to the nearest lower layer", () => {
  assert.deepEqual(resolveKey(keys, 2, 0, 1), { own: T, effective: "KC_1", fromLayer: 1 });
  assert.deepEqual(resolveKey(keys, 2, 0, 0), { own: T, effective: "KC_A", fromLayer: 0 });
});

test("transparent all the way down does nothing", () => {
  assert.deepEqual(resolveKey(keys, 2, 0, 2), { own: T, effective: "KC_NO", fromLayer: 0 });
});

test("out-of-range positions read as no key", () => {
  assert.deepEqual(resolveKey(keys, 5, 0, 0), { own: "KC_NO", effective: "KC_NO", fromLayer: 5 });
  assert.deepEqual(resolveKey(keys, 0, 9, 9), { own: "KC_NO", effective: "KC_NO", fromLayer: 0 });
});

test("countAssigned ignores transparent and no-op keys", () => {
  assert.equal(countAssigned([["KC_A", T, "KC_NO", "MO(1)"]]), 2);
});
