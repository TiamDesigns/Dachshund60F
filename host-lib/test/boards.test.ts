// Validates every boards/<name>/ definition: layout geometry, matrix mapping
// and pin assignments. Runs with the host-lib tests until the GUI has its own.
import { test } from "node:test";
import assert from "node:assert/strict";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import path from "node:path";

const boardsDir = fileURLToPath(new URL("../../../boards/", import.meta.url));

interface MatrixPos {
  row: number;
  col: number;
}

interface Key extends MatrixPos {
  x: number;
  y: number;
  w?: number;
  h?: number;
  option?: [number, number];
}

interface KeyboardDef {
  name: string;
  matrix: { rows: number; cols: number };
  layoutOptions: { id: string; choices: string[]; default: number }[];
  keys: Key[];
  encoders: { id: number; x: number; y: number; push?: MatrixPos }[];
  indicators: { id: number; underKey?: MatrixPos; at?: { x: number; y: number } }[];
}

interface BoardDef {
  name: string;
  matrix: { rows: number; cols: number; rowPins: string[]; colPins: string[] };
  encoders: { pinA: string; pinB: string }[];
  indicators: { count: number; dataPin: string; powerEnablePin: string };
  power: { chargeStatusPin?: string };
  reserved: Record<string, string>;
}

const boardNames = readdirSync(boardsDir, { withFileTypes: true })
  .filter((d) => d.isDirectory())
  .map((d) => d.name);

const readJson = <T>(file: string): T => JSON.parse(readFileSync(file, "utf8")) as T;

// Keys present for one combination of layout option choices.
function visibleKeys(def: KeyboardDef, choices: number[]): Key[] {
  return def.keys.filter((k) => !k.option || choices[k.option[0]] === k.option[1]);
}

function allChoiceCombos(def: KeyboardDef): number[][] {
  let combos: number[][] = [[]];
  for (const opt of def.layoutOptions) {
    combos = combos.flatMap((c) => opt.choices.map((_, i) => [...c, i]));
  }
  return combos;
}

test("boards/ has at least one board", () => {
  assert.ok(boardNames.length > 0);
});

for (const name of boardNames) {
  const dir = path.join(boardsDir, name);
  const kbFile = path.join(dir, "keyboard.json");
  const boardFile = path.join(dir, "board.json");

  test(`${name}: has keyboard.json and board.json`, () => {
    assert.ok(existsSync(kbFile), "keyboard.json missing");
    assert.ok(existsSync(boardFile), "board.json missing");
  });
  if (!existsSync(kbFile) || !existsSync(boardFile)) continue;

  const kb = readJson<KeyboardDef>(kbFile);
  const board = readJson<BoardDef>(boardFile);
  const encoderPushes = kb.encoders.flatMap((e) => (e.push ? [e.push] : []));

  test(`${name}: names match the folder`, () => {
    assert.equal(kb.name, name);
    assert.equal(board.name, name);
  });

  test(`${name}: matrix size agrees between files and pin lists`, () => {
    assert.deepEqual(kb.matrix, { rows: board.matrix.rows, cols: board.matrix.cols });
    assert.equal(board.matrix.rowPins.length, board.matrix.rows);
    assert.equal(board.matrix.colPins.length, board.matrix.cols);
  });

  test(`${name}: every matrix position is in range`, () => {
    const positions: MatrixPos[] = [
      ...kb.keys,
      ...encoderPushes,
      ...kb.indicators.flatMap((i) => (i.underKey ? [i.underKey] : [])),
    ];
    for (const p of positions) {
      assert.ok(p.row >= 0 && p.row < kb.matrix.rows, `row ${p.row} out of range`);
      assert.ok(p.col >= 0 && p.col < kb.matrix.cols, `col ${p.col} out of range`);
    }
  });

  test(`${name}: layout options reference valid choices`, () => {
    for (const k of kb.keys) {
      if (!k.option) continue;
      const opt = kb.layoutOptions[k.option[0]];
      assert.ok(opt, `key (${k.row},${k.col}) uses unknown option ${k.option[0]}`);
      assert.ok(k.option[1] < opt.choices.length, `key (${k.row},${k.col}) uses unknown choice`);
    }
  });

  for (const choices of allChoiceCombos(kb)) {
    const keys = visibleKeys(kb, choices);

    test(`${name} [${choices.join(",")}]: matrix positions are unique`, () => {
      const seen = new Set<string>();
      for (const p of [...keys, ...encoderPushes]) {
        const id = `${p.row},${p.col}`;
        assert.ok(!seen.has(id), `matrix position (${id}) used twice`);
        seen.add(id);
      }
    });

    test(`${name} [${choices.join(",")}]: keys and encoders do not overlap`, () => {
      const eps = 1e-9;
      const rects = [
        ...keys.map((k) => ({ id: `key (${k.row},${k.col})`, x: k.x, y: k.y, w: k.w ?? 1, h: k.h ?? 1 })),
        ...kb.encoders.map((e) => ({ id: `encoder ${e.id}`, x: e.x, y: e.y, w: 1, h: 1 })),
      ];
      for (let i = 0; i < rects.length; i++) {
        for (let j = i + 1; j < rects.length; j++) {
          const a = rects[i]!;
          const b = rects[j]!;
          const overlapX = a.x < b.x + b.w - eps && b.x < a.x + a.w - eps;
          const overlapY = a.y < b.y + b.h - eps && b.y < a.y + a.h - eps;
          assert.ok(!(overlapX && overlapY), `${a.id} and ${b.id} overlap`);
        }
      }
    });
  }

  test(`${name}: pins are valid nRF52840 GPIOs, unique and not reserved`, () => {
    const used = [
      ...board.matrix.rowPins,
      ...board.matrix.colPins,
      ...board.encoders.flatMap((e) => [e.pinA, e.pinB]),
      board.indicators.dataPin,
      board.indicators.powerEnablePin,
      ...(board.power.chargeStatusPin ? [board.power.chargeStatusPin] : []),
    ];
    const seen = new Set<string>();
    for (const pin of used) {
      const m = /^P([01])\.(\d{2})$/.exec(pin);
      assert.ok(m, `bad pin name ${pin}`);
      const num = Number(m[2]);
      assert.ok(m[1] === "0" ? num <= 31 : num <= 15, `${pin} does not exist on nRF52840`);
      assert.ok(!seen.has(pin), `${pin} assigned twice`);
      assert.ok(!(pin in board.reserved), `${pin} is reserved: ${board.reserved[pin]}`);
      seen.add(pin);
    }
  });

  test(`${name}: indicator count matches keyboard.json`, () => {
    assert.equal(board.indicators.count, kb.indicators.length);
  });

  test(`${name}: indicator ids follow the LED chain and each has one position`, () => {
    kb.indicators.forEach((ind, i) => {
      assert.equal(ind.id, i, `indicator ${i} has id ${ind.id}`);
      assert.ok(!!ind.underKey !== !!ind.at, `indicator ${ind.id} needs exactly one of underKey or at`);
    });
  });

  for (const choices of allChoiceCombos(kb)) {
    test(`${name} [${choices.join(",")}]: free-standing indicators are not under a key`, () => {
      for (const ind of kb.indicators) {
        if (!ind.at) continue;
        for (const k of visibleKeys(kb, choices)) {
          const inside =
            ind.at.x > k.x && ind.at.x < k.x + (k.w ?? 1) && ind.at.y > k.y && ind.at.y < k.y + (k.h ?? 1);
          assert.ok(!inside, `indicator ${ind.id} is under key (${k.row},${k.col})`);
        }
      }
    });
  }
}
