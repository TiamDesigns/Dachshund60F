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
  display?: { pins: { din: string; clk: string; cs: string; dc: string; rst: string; busy: string } };
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

// Body of a devicetree node by label, e.g. "led_power: led_power { ... }"
// or "display_spi: &spi2 { ... }".
function dtNode(dts: string, label: string): string {
  const start = dts.search(new RegExp(`\\b${label}:\\s*&?[\\w@,-]+\\s*\\{`));
  assert.ok(start >= 0, `node ${label} not found`);
  let depth = 0;
  for (let i = dts.indexOf("{", start); i < dts.length; i++) {
    if (dts[i] === "{") depth++;
    else if (dts[i] === "}" && --depth === 0) return dts.slice(start, i + 1);
  }
  assert.fail(`node ${label} is not closed`);
}

// Pins of one function (e.g. SPIM_MOSI) in a pinctrl state node.
function psels(dts: string, state: string, fn: string): string[] {
  const re = new RegExp(`NRF_PSEL\\(${fn},\\s*(\\d),\\s*(\\d+)\\)`, "g");
  return [...dtNode(dts, state).matchAll(re)].map((m) => `P${m[1]}.${m[2]!.padStart(2, "0")}`);
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

    for (const state of ["spi3_default", "spi3_sleep"]) {
      assert.deepEqual(psels(dts, state, "SPIM_MOSI"), [board.indicators.dataPin], state);
    }

    const strip = dtNode(dts, "led_strip");
    assert.equal(Number(dtProp(strip, "chain-length").replace(/[<>\s]/g, "")), board.indicators.count);
  });

  if (board.display) {
    const pins = board.display.pins;

    test(`${name}: display header map matches board.json`, () => {
      const header = dtNode(dts, "display_header");
      const entries = [...dtProp(header, "gpio-map").matchAll(/<\s*(\d+)\s+0\s+&gpio([01])\s+(\d+)\s+0\s*>/g)];
      const map = Object.fromEntries(entries.map((m) => [m[1], `P${m[2]}.${m[3]!.padStart(2, "0")}`]));
      // J4 pin numbers, Waveshare e-Paper order: 3 DIN, 4 CLK, 5 CS, 6 DC, 7 RST, 8 BUSY.
      assert.deepEqual(map, { 3: pins.din, 4: pins.clk, 5: pins.cs, 6: pins.dc, 7: pins.rst, 8: pins.busy });
    });

    test(`${name}: display I2C and SPI pinctrl match board.json`, () => {
      for (const state of ["i2c0_default", "i2c0_sleep"]) {
        assert.deepEqual(psels(dts, state, "TWIM_SDA"), [pins.din], state);
        assert.deepEqual(psels(dts, state, "TWIM_SCL"), [pins.clk], state);
      }
      for (const state of ["spi2_default", "spi2_sleep"]) {
        assert.deepEqual(psels(dts, state, "SPIM_MOSI"), [pins.din], state);
        assert.deepEqual(psels(dts, state, "SPIM_SCK"), [pins.clk], state);
      }
      assert.match(dtProp(dtNode(dts, "display_spi"), "cs-gpios"), /<&display_header 5 GPIO_ACTIVE_LOW>/);
    });
  }

  test(`${name}: firmware Kconfig USB IDs match board.json`, () => {
    const kconfig = readFileSync(path.join(repoDir, "firmware", "Kconfig"), "utf8").replace(/\r\n/g, "\n");
    const usb = board.usb.keyboard;
    assert.equal(Number(kconfigDefault(kconfig, "KB_USB_VID")), Number(usb.vid));
    assert.equal(Number(kconfigDefault(kconfig, "KB_USB_PID")), Number(usb.pid));
    assert.equal(kconfigDefault(kconfig, "KB_USB_MANUFACTURER"), usb.manufacturer);
    assert.equal(kconfigDefault(kconfig, "KB_USB_PRODUCT"), usb.product);
  });

  test(`${name}: firmware indicator chain matches keyboard.json`, () => {
    const layout = JSON.parse(readFileSync(path.join(dir, "keyboard.json"), "utf8")) as {
      indicators: { id: number; role: string; label: string; level?: number }[];
    };
    const source = readFileSync(path.join(repoDir, "firmware", "src", "indicators.c"), "utf8");
    const table = /static const struct kb_led_def chain\[\] = \{([^;]*)\};/.exec(source);
    assert.ok(table, "chain table not found in firmware/src/indicators.c");
    const firmware = [...table[1]!.matchAll(/\{KB_LED_ROLE_(\w+), (\d+)\}/g)].map((m) => `${m[1]} ${m[2]}`);

    const expected = [...layout.indicators]
      .sort((a, b) => a.id - b.id)
      .map((ind) => {
        if (ind.role === "layer") return "LAYER 0";
        if (ind.role === "battery") return `BATTERY ${ind.level}`;
        const lock = /^(Caps|Num|Scroll) Lock$/.exec(ind.label);
        assert.ok(ind.role === "lock" && lock, `indicator ${ind.id}: unknown role ${ind.role} "${ind.label}"`);
        return `${lock[1]!.toUpperCase()} 0`;
      });
    assert.deepEqual(firmware, expected);
  });
}

const displaysDir = path.join(repoDir, "firmware", "displays");
const overlays = existsSync(displaysDir) ? readdirSync(displaysDir).filter((f) => f.endsWith(".overlay")) : [];

test("firmware/displays has panel overlays", () => {
  assert.ok(overlays.length > 0);
});

for (const file of overlays) {
  test(`display overlay ${file}: pins only through the J4 header`, () => {
    const text = readFileSync(path.join(displaysDir, file), "utf8");
    assert.doesNotMatch(text, /&gpio[01]\b/, "use <&display_header PIN FLAGS>, not raw &gpio pins");
    assert.doesNotMatch(text, /NRF_PSEL/, "pinctrl belongs in the board files");
    assert.match(text, /zephyr,display\s*=\s*&screen;/, "must choose its panel as zephyr,display");
    assert.match(text, /&display_(i2c|spi)\b/, "must sit on display_i2c or display_spi");
  });
}
