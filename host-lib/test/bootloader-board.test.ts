// Checks that each board's Adafruit nRF52 bootloader definition
// (boards/<name>/bootloader/board.h) and its devicetree flash layout agree with
// boards/<name>/board.json and with the layout the bootloader expects.
import { test } from "node:test";
import assert from "node:assert/strict";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import path from "node:path";

const repoDir = fileURLToPath(new URL("../../../", import.meta.url));
const boardsDir = path.join(repoDir, "boards");

// Adafruit bootloader on the nRF52840: it starts at 0xf4000 and reserves
// DFU_APP_DATA_RESERVED = 10 pages below itself, so UF2 app writes stop there.
const BOOTLOADER_START = 0xf4000;
const USER_FLASH_END = BOOTLOADER_START - 10 * 4096;
// The app starts after the MBR and S140 v6.1.1 SoftDevice.
const APP_START = 0x26000;

interface BoardDef {
  matrix: { rowPins: string[]; colPins: string[]; diodeDirection: string };
  indicators: { count: number; dataPin: string; powerEnablePin: string; powerEnableActive: string };
  usb: { bootloader: { vid: string; pid: string; manufacturer: string; product: string } };
}

// Value of `#define NAME value` in a C header, without a trailing comment.
function define(header: string, name: string): string {
  const m = new RegExp(`^#define\\s+${name}\\s+(.+?)\\s*(?://.*)?$`, "m").exec(header);
  assert.ok(m, `#define ${name} not found`);
  return m[1]!;
}

// "PINNUM(1, 10)" -> "P1.10".
function pin(value: string): string {
  const m = /^PINNUM\((\d),\s*(\d+)\)$/.exec(value);
  assert.ok(m, `${value} is not a PINNUM(port, pin)`);
  return `P${m[1]}.${m[2]!.padStart(2, "0")}`;
}

const cString = (value: string) => value.replace(/^"(.*)"$/, "$1");

// "partition@26000 { ... reg = <0x00026000 0x000c4000>;" for each partition label.
function partitions(dts: string): Map<string, { start: number; size: number }> {
  const out = new Map<string, { start: number; size: number }>();
  const re = /label\s*=\s*"([^"]+)";\s*reg\s*=\s*<\s*(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s*>/g;
  for (const m of dts.matchAll(re)) out.set(m[1]!, { start: Number(m[2]), size: Number(m[3]) });
  return out;
}

const bootloaderBoards = readdirSync(boardsDir, { withFileTypes: true })
  .filter((d) => d.isDirectory())
  .map((d) => d.name)
  .filter((name) => existsSync(path.join(boardsDir, name, "bootloader", "board.h")));

test("at least one board has bootloader board files", () => {
  assert.ok(bootloaderBoards.length > 0);
});

for (const name of bootloaderBoards) {
  const dir = path.join(boardsDir, name);
  const board = JSON.parse(readFileSync(path.join(dir, "board.json"), "utf8")) as BoardDef;
  const header = readFileSync(path.join(dir, "bootloader", "board.h"), "utf8").replace(/\r\n/g, "\n");

  test(`${name}: bootloader LED pins and count match board.json`, () => {
    assert.equal(pin(define(header, "LED_NEOPIXEL")), board.indicators.dataPin);
    assert.equal(Number(define(header, "NEOPIXELS_NUMBER")), board.indicators.count);
    // The bootloader drives the power pin high, so only an active-high switch works.
    assert.equal(board.indicators.powerEnableActive, "high");
    assert.equal(pin(define(header, "NEOPIXEL_POWER_PIN")), board.indicators.powerEnablePin);
  });

  test(`${name}: bootloader DFU key is Esc (row 0, col 0)`, () => {
    // COL2ROW: board_init2() drives the column high, the row reads high through the diode.
    assert.equal(board.matrix.diodeDirection, "COL2ROW");
    assert.equal(pin(define(header, "DFU_ESC_COL_PIN")), board.matrix.colPins[0]);
    assert.equal(pin(define(header, "BUTTON_DFU")), board.matrix.rowPins[0]);
    assert.equal(define(header, "BUTTON_PULL"), "NRF_GPIO_PIN_PULLDOWN");
  });

  test(`${name}: bootloader USB IDs and names match board.json`, () => {
    const usb = board.usb.bootloader;
    assert.equal(Number(define(header, "USB_DESC_VID")), Number(usb.vid));
    assert.equal(Number(define(header, "USB_DESC_UF2_PID")), Number(usb.pid));
    assert.equal(Number(define(header, "USB_DESC_CDC_ONLY_PID")), Number(usb.pid));
    assert.equal(cString(define(header, "BLEDIS_MANUFACTURER")), usb.manufacturer);
    assert.equal(cString(define(header, "UF2_PRODUCT_NAME")), usb.product);
    assert.ok(cString(define(header, "UF2_VOLUME_LABEL")).length <= 11, "FAT volume labels are at most 11 characters");
  });

  test(`${name}: devicetree flash layout fits the bootloader`, () => {
    const dtsFiles = readdirSync(dir).filter((f) => f.endsWith(".dts"));
    const parts = partitions(dtsFiles.map((f) => readFileSync(path.join(dir, f), "utf8")).join("\n"));
    const code = parts.get("code_partition");
    const storage = parts.get("storage");
    const boot = parts.get("adafruit_boot");
    assert.ok(code && storage && boot, "code_partition, storage and adafruit_boot partitions are required");
    assert.equal(code.start, APP_START);
    assert.ok(code.start + code.size <= USER_FLASH_END, "code partition runs past the last address UF2 can write");
    assert.ok(storage.start >= USER_FLASH_END, "storage must sit in the area DFU never erases");
    assert.ok(storage.start + storage.size <= BOOTLOADER_START, "storage overlaps the bootloader");
    assert.equal(boot.start, BOOTLOADER_START);
  });
}
