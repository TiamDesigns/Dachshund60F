import { test } from "node:test";
import assert from "node:assert/strict";

import { EditHistory, type Edit } from "./history.ts";

const key = (before: string, after: string): Edit => ({ kind: "key", layer: 0, row: 1, col: 2, before, after });

test("undo and redo walk the history in order", () => {
  const h = new EditHistory();
  assert.equal(h.canUndo, false);
  h.record(key("KC_A", "KC_B"));
  h.record(key("KC_B", "KC_C"));

  assert.deepEqual(h.undo(), key("KC_B", "KC_C"));
  assert.deepEqual(h.undo(), key("KC_A", "KC_B"));
  assert.equal(h.undo(), undefined);
  assert.equal(h.canRedo, true);

  assert.deepEqual(h.redo(), key("KC_A", "KC_B"));
  assert.deepEqual(h.redo(), key("KC_B", "KC_C"));
  assert.equal(h.redo(), undefined);
});

test("a new edit clears redo", () => {
  const h = new EditHistory();
  h.record(key("KC_A", "KC_B"));
  h.undo();
  h.record(key("KC_A", "KC_Z"));
  assert.equal(h.canRedo, false);
  assert.deepEqual(h.undo(), key("KC_A", "KC_Z"));
});

test("no-op edits are not recorded", () => {
  const h = new EditHistory();
  h.record(key("KC_A", "KC_A"));
  h.record({
    kind: "encoder",
    layer: 0,
    encoder: 0,
    before: { cw: "KC_VOLU", ccw: "KC_VOLD" },
    after: { cw: "KC_VOLU", ccw: "KC_VOLD" },
  });
  assert.equal(h.canUndo, false);
});

test("history is capped", () => {
  const h = new EditHistory(3);
  for (const kc of ["KC_A", "KC_B", "KC_C", "KC_D"]) h.record(key("KC_NO", kc));
  let n = 0;
  while (h.undo()) n++;
  assert.equal(n, 3);
});

test("clear empties both stacks", () => {
  const h = new EditHistory();
  h.record(key("KC_A", "KC_B"));
  h.record(key("KC_B", "KC_C"));
  h.undo();
  h.clear();
  assert.equal(h.canUndo, false);
  assert.equal(h.canRedo, false);
});

test("peeking doesn't move the history", () => {
  const h = new EditHistory();
  assert.equal(h.peekUndo(), undefined);
  h.record(key("KC_A", "KC_B"));
  assert.deepEqual(h.peekUndo(), key("KC_A", "KC_B"));
  assert.equal(h.canUndo, true);
  assert.equal(h.canRedo, false);
  h.undo();
  assert.equal(h.peekUndo(), undefined);
  assert.deepEqual(h.peekRedo(), key("KC_A", "KC_B"));
  assert.equal(h.canRedo, true);
});
