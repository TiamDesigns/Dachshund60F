// Checks that the Zephyr board files and firmware Kconfig agree with
// boards/<name>/board.json, the source of truth for pins and USB IDs.
import { test } from "node:test";
import assert from "node:assert/strict";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import path from "node:path";

const repoDir = fileURLToPath(new URL("../../../", import.meta.url));
const boardsDir = path.join(repoDir, "boards");

interface BoardDef {
  matrix: { rowPins: string[]; colPins: string[] };
  encoders: { pinA: string; pinB: string }[];
  indicators: { count: number; dataPin: string; powerEnablePin: string };
  power: { chargeStatusPin?: string };
  usb: { keyboard: { vid: string; pid: string; manufacturer: string; product: string } };
}

// "&gpio1 10 (...)" -> "P1.10", in order of appearance.
function gpioPins(text: string): string[] {
  return [...text.matchAll(/&gpio([01])\s+(\d+)/g)].map(
    (m) => `P${m[1]}.${m[2]!.padStart(2, "0")}`,
  );
}

// Value text of a devicetree property, up to its terminating semicolon.
function dtProp(dts: string, name: string): string {
  const m = new RegExp(`(?:^|\\s)${name}\\s*=([^;]*);`).exec(dts);
  assert.ok(m, `property ${name} not found`);
  return m[1]!;
}

// Body of a devicetree node by label, e.g. "led_power: led_power { ... }".
function dtNode(dts: string, label: string): string {
  const start = dts.search(new RegExp(`\\b${label}:\\s*[\\w@,-]+\\s*\\{`));
  assert.ok(start >= 0, `node ${label} not found`);
  let depth = 0;
  for (let i = dts.indexOf("{", start); i < dts.length; i++) {
    if (dts[i] === "{") depth++;
    else if (dts[i] === "}" && --depth === 0) return dts.slice(start, i + 1);
  }
  assert.fail(`node ${label} is not closed`);
}

function kconfigDefault(kconfig: string, symbol: string): string {
  const m = new RegExp(`config ${symbol}\\n(?:\\t.*\\n)*?\\tdefault (.+)`).exec(kconfig);
  assert.ok(m, `Kconfig ${symbol} has no default`);
  return m[1]!.trim().replace(/^"(.*)"$/, "$1");
}

const zephyrBoards = readdirSync(boardsDir, { withFileTypes: true })
  .filter((d) => d.isDirectory())
  .map((d) => d.name)
  .filter((name) => existsSync(path.join(boardsDir, name, "board.yml")));

test("at least one board has Zephyr board files", () => {
  assert.ok(zephyrBoards.length > 0);
});

for (const name of zephyrBoards) {
  const dir = path.join(boardsDir, name);
  const board = JSON.parse(readFileSync(path.join(dir, "board.json"), "utf8")) as BoardDef;
  const dtsFiles = readdirSync(dir).filter((f) => f.endsWith(".dts") || f.endsWith(".dtsi"));
  const dts = dtsFiles.map((f) => readFileSync(path.join(dir, f), "utf8")).join("\n");

  test(`${name}: devicetree matrix pins match board.json`, () => {
    assert.deepEqual(gpioPins(dtProp(dts, "matrix-row-gpios")), board.matrix.rowPins);
    assert.deepEqual(gpioPins(dtProp(dts, "matrix-col-gpios")), board.matrix.colPins);
  });

  test(`${name}: devicetree encoder and charger pins match board.json`, () => {
    const enc = board.encoders[0]!;
    assert.deepEqual(gpioPins(dtProp(dts, "encoder-a-gpios")), [enc.pinA]);
    assert.deepEqual(gpioPins(dtProp(dts, "encoder-b-gpios")), [enc.pinB]);
    assert.deepEqual(gpioPins(dtProp(dts, "charge-status-gpios")), [board.power.chargeStatusPin]);
  });

  test(`${name}: devicetree LED pins and chain length match board.json`, () => {
    const power = dtNode(dts, "led_power");
    assert.deepEqual(gpioPins(dtProp(power, "enable-gpios")), [board.indicators.powerEnablePin]);

    const mosi = [...dts.matchAll(/NRF_PSEL\(SPIM_MOSI,\s*(\d),\s*(\d+)\)/g)].map(
      (m) => `P${m[1]}.${m[2]!.padStart(2, "0")}`,
    );
    assert.ok(mosi.length > 0, "no SPIM_MOSI pinctrl");
    for (const pin of mosi) assert.equal(pin, board.indicators.dataPin);

    const strip = dtNode(dts, "led_strip");
    assert.equal(Number(dtProp(strip, "chain-length").replace(/[<>\s]/g, "")), board.indicators.count);
  });

  test(`${name}: firmware Kconfig USB IDs match board.json`, () => {
    const kconfig = readFileSync(path.join(repoDir, "firmware", "Kconfig"), "utf8").replace(/\r\n/g, "\n");
    const usb = board.usb.keyboard;
    assert.equal(Number(kconfigDefault(kconfig, "KB_USB_VID")), Number(usb.vid));
    assert.equal(Number(kconfigDefault(kconfig, "KB_USB_PID")), Number(usb.pid));
    assert.equal(kconfigDefault(kconfig, "KB_USB_MANUFACTURER"), usb.manufacturer);
    assert.equal(kconfigDefault(kconfig, "KB_USB_PRODUCT"), usb.product);
  });
}
