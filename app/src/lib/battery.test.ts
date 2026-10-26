import { test } from "node:test";
import assert from "node:assert/strict";

import { batteryColor } from "./battery.ts";

test("a full battery is white", () => {
  assert.equal(batteryColor(100), "#ffffff");
});

test("the colour shades towards yellow as the charge drops", () => {
  assert.equal(batteryColor(15), "#f5c428");
  const blue = (p: number) => parseInt(batteryColor(p).slice(5), 16);
  assert.ok(blue(80) > blue(50) && blue(50) > blue(20), "less blue, so more yellow, as it drains");
});

test("below 15% it is red", () => {
  assert.equal(batteryColor(14), "#e0402e");
  assert.equal(batteryColor(0), "#e0402e");
  assert.equal(batteryColor(-5), "#e0402e");
  assert.equal(batteryColor(120), "#ffffff");
});
