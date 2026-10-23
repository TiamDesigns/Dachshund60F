import { test } from "node:test";
import assert from "node:assert/strict";

import {
  checkAdvancedEntry,
  defaultAdvancedKeys,
  emptyAdvancedEntry,
  firstFreeAdvancedSlot,
  isAdvancedEntryUsed,
  macroBytes,
  type AdvancedTable,
} from "../src/index.js";

const TABLES: AdvancedTable[] = ["combos", "tapDances", "keyOverrides", "leader", "tapHold"];
const ctx = { layerCount: 4, rows: 6, cols: 15 };

test("empty slots are unused and pass the board's checks", () => {
  for (const t of TABLES) {
    assert.equal(isAdvancedEntryUsed(t, emptyAdvancedEntry(t)), false, t);
    checkAdvancedEntry(t, emptyAdvancedEntry(t), ctx);
  }
});

test("firstFreeAdvancedSlot skips used slots and says when a table is full", () => {
  const combos = defaultAdvancedKeys().combos;
  assert.equal(firstFreeAdvancedSlot("combos", combos), 0);
  combos[0] = { keys: [{ row: 2, col: 1 }, { row: 2, col: 2 }], result: "KC_ESC", timeoutMs: 0, layers: [] };
  assert.equal(firstFreeAdvancedSlot("combos", combos), 1);
  assert.equal(firstFreeAdvancedSlot("combos", combos.map(() => combos[0]!)), -1);
  const rules = defaultAdvancedKeys().tapHold;
  rules[0] = { row: 0, col: 0, permissiveHold: false, holdOnOtherKeyPress: false, retroTap: false, tappingTermMs: 200, quickTapMs: 0 };
  assert.equal(firstFreeAdvancedSlot("tapHold", rules), 1);
});

test("macroBytes counts the board's bytes and refuses what it can't store", () => {
  assert.equal(macroBytes([[{ type: "text", text: "hi" }], []]), 4);
  assert.equal(macroBytes([[{ type: "tap", keycode: "KC_A" }]]), 4);
  assert.equal(macroBytes([[{ type: "delay", ms: 70000 }]]), null);
  assert.equal(macroBytes([[{ type: "text", text: "é" }]]), null);
});
