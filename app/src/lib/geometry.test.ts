import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

import { boardGeometry, fitUnit } from "./geometry.ts";

const def = JSON.parse(
  readFileSync(new URL("../../../boards/Dachshund60F/keyboard.json", import.meta.url), "utf8"),
);

test("split spacebar layout has 79 keys, full bar has 77", () => {
  assert.equal(boardGeometry(def, [0], 50, 4).keys.length, 79);
  assert.equal(boardGeometry(def, [1], 50, 4).keys.length, 77);
});

test("keys are placed at unit scale with the gap split around them", () => {
  const g = boardGeometry(def, [0], 50, 4);
  const esc = g.keys.find((k) => k.key.row === 0 && k.key.col === 0)!;
  assert.deepEqual({ x: esc.x, y: esc.y, w: esc.w, h: esc.h }, { x: 2, y: 2, w: 46, h: 46 });
  const bksp = g.keys.find((k) => k.key.row === 1 && k.key.col === 13)!;
  assert.equal(bksp.w, 2 * 50 - 4);
});

test("encoder and free-standing indicators are included", () => {
  const g = boardGeometry(def, [0], 50, 4);
  assert.equal(g.encoders.length, 1);
  assert.deepEqual(g.encoders[0]!.push, { row: 0, col: 14 });
  // Five LEDs in the row above the arrows; Caps Lock's LED sits under its key.
  assert.equal(g.indicators.length, 5);
  assert.ok(g.width >= 16 * 50, "board is at least 16u wide");
  assert.ok(g.height >= 6.25 * 50, "board is at least 6.25u tall");
});

test("fitUnit fits the board into the available width", () => {
  const unit = fitUnit(def, 900);
  const g = boardGeometry(def, [0], unit, 0);
  assert.ok(g.width <= 900);
  assert.equal(fitUnit(def, 100), 30); // clamped to the minimum
  assert.equal(fitUnit(def, 100000), 60); // and the maximum
});
