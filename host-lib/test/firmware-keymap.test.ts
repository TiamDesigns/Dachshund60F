// Checks that the firmware keycode model and compiled-in default keymap agree
// with the keycode catalog and default keymaps that Doghouse and the mock
// board use, so the GUI shows what the board actually does.
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import path from "node:path";

import { allKeycodes, defaultKeymap } from "../src/index.js";

const repoDir = fileURLToPath(new URL("../../../", import.meta.url));
const keycodesH = readFileSync(path.join(repoDir, "firmware/core/include/kb/keycodes.h"), "utf8");
const keymapC = readFileSync(path.join(repoDir, "firmware/src/default_keymap.c"), "utf8");
const keymapH = readFileSync(path.join(repoDir, "firmware/src/default_keymap.h"), "utf8");

// The firmware's short aliases for the two special keycodes.
const ALIASES: Record<string, string> = { XXXXXXX: "KC_NO", _______: "KC_TRNS" };

function stripComments(c: string): string {
  return c.replace(/\/\*[\s\S]*?\*\//g, "").replace(/\/\/.*$/gm, "");
}

// Object-like and function-like macro names defined in a header.
function macros(h: string): { plain: Set<string>; fn: Set<string> } {
  const plain = new Set<string>();
  const fn = new Set<string>();
  for (const m of h.matchAll(/^#define\s+(\w+)(\()?/gm)) {
    (m[2] ? fn : plain).add(m[1]!);
  }
  return { plain, fn };
}

function defineNumber(h: string, name: string): number {
  const m = new RegExp(`^#define\\s+${name}\\s+(\\d+)`, "m").exec(h);
  assert.ok(m, `${name} not defined`);
  return Number(m[1]);
}

// [layer][row][col] keycode ids from the default_keymap initializer.
function firmwareKeymap(c: string): string[][][] {
  const body = stripComments(c);
  const start = body.indexOf("=", body.indexOf("default_keymap["));
  assert.ok(start >= 0, "default_keymap initializer not found");

  const layers: string[][][] = [];
  let depth = 0;
  let item = "";
  let row: string[] = [];
  let layer: string[][] = [];
  for (const ch of body.slice(start + 1)) {
    if (ch === "{") {
      depth++;
      continue;
    }
    if (depth === 3 && (ch === "," || ch === "}")) {
      // Commas inside BT_SEL(0) etc. sit inside parentheses.
      if (ch === "," && (item.match(/\(/g)?.length ?? 0) > (item.match(/\)/g)?.length ?? 0)) {
        item += ch;
        continue;
      }
      const id = item.replace(/\s+/g, "");
      if (id) row.push(ALIASES[id] ?? id);
      item = "";
    } else if (depth === 3) {
      item += ch;
    }
    if (ch === "}") {
      if (depth === 3) {
        layer.push(row);
        row = [];
      } else if (depth === 2) {
        layers.push(layer);
        layer = [];
      } else if (depth === 1) {
        break;
      }
      depth--;
    }
  }
  return layers;
}

const { plain, fn } = macros(keycodesH);
const firmware = firmwareKeymap(keymapC);

test("every Doghouse keycode is defined in the firmware keycode model", () => {
  for (const { id } of allKeycodes(16)) {
    const call = /^(\w+)\(\d+\)$/.exec(id);
    if (call) assert.ok(fn.has(call[1]!), `${call[1]}(n) is not a firmware macro`);
    else assert.ok(plain.has(id), `${id} is not defined in kb/keycodes.h`);
  }
});

test("firmware default keymap has the size in default_keymap.h", () => {
  const layers = defineNumber(keymapH, "DEFAULT_KEYMAP_LAYERS");
  const rows = defineNumber(keymapH, "DEFAULT_KEYMAP_ROWS");
  const cols = defineNumber(keymapH, "DEFAULT_KEYMAP_COLS");
  assert.equal(firmware.length, layers);
  for (const [l, layer] of firmware.entries()) {
    assert.equal(layer.length, rows, `layer ${l} rows`);
    for (const [r, row] of layer.entries()) assert.equal(row.length, cols, `layer ${l} row ${r} columns`);
  }
});

test("firmware default keymap matches the Dachshund60F matrix", () => {
  const board = JSON.parse(readFileSync(path.join(repoDir, "boards/Dachshund60F/board.json"), "utf8")) as {
    matrix: { rows: number; cols: number };
  };
  assert.equal(defineNumber(keymapH, "DEFAULT_KEYMAP_ROWS"), board.matrix.rows);
  assert.equal(defineNumber(keymapH, "DEFAULT_KEYMAP_COLS"), board.matrix.cols);
});

test("firmware default keymap matches host-lib's Dachshund60F default", () => {
  const host = defaultKeymap("Dachshund60F");
  assert.ok(host);
  // Encoders are mapped in phase 9; only keys are compared for now.
  assert.deepEqual(firmware, host.keys);
});

// Boards on Nordic's own boards bring their own default keymap.
test("the Pup's firmware default keymap matches host-lib's", () => {
  const c = readFileSync(path.join(repoDir, "boards/Pup/firmware/default_keymap.c"), "utf8");
  const h = readFileSync(path.join(repoDir, "boards/Pup/firmware/board_keymap.h"), "utf8");
  const board = JSON.parse(readFileSync(path.join(repoDir, "boards/Pup/board.json"), "utf8")) as {
    matrix: { rows: number; cols: number };
  };
  const pup = firmwareKeymap(c);
  assert.equal(pup.length, defineNumber(h, "DEFAULT_KEYMAP_LAYERS"));
  assert.equal(defineNumber(h, "DEFAULT_KEYMAP_ROWS"), board.matrix.rows);
  assert.equal(defineNumber(h, "DEFAULT_KEYMAP_COLS"), board.matrix.cols);
  assert.deepEqual(pup, defaultKeymap("Pup")?.keys);
});

test("firmware default keymap uses only defined keycodes", () => {
  for (const layer of firmware) {
    for (const id of layer.flat()) {
      const call = /^(\w+)\(\d+\)$/.exec(id);
      assert.ok(call ? fn.has(call[1]!) : plain.has(id), `${id} is not defined in kb/keycodes.h`);
    }
  }
});
