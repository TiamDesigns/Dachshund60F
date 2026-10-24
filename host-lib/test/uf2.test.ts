import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

import {
  APP_FLASH,
  compareVersions,
  encodeFirmwareInfo,
  makeUf2,
  parseUf2,
  uf2BoardId,
  Uf2Error,
} from "../src/index.js";

function image(size: number, infoAt?: number): Uint8Array {
  const img = new Uint8Array(size).map((_, i) => i & 0xff);
  if (infoAt !== undefined) {
    img.set(encodeFirmwareInfo({ protocolVersion: 1, version: "0.2.3", model: "Dachshund60F" }), infoAt);
  }
  return img;
}

test("a firmware image round-trips and its info block is found", () => {
  // The block straddles two 256-byte payloads.
  const file = makeUf2(image(4000, 250), APP_FLASH.start);
  const r = parseUf2(file);
  assert.equal(r.start, APP_FLASH.start);
  assert.equal(r.end, APP_FLASH.start + 4000);
  assert.equal(r.blocks, Math.ceil(4000 / 256));
  assert.deepEqual(r.info, { layout: 1, protocolVersion: 1, version: "0.2.3", model: "Dachshund60F" });
});

test("an image without an info block parses with info null", () => {
  assert.equal(parseUf2(makeUf2(image(1000), APP_FLASH.start)).info, null);
});

test("refuses files that aren't firmware for this board", () => {
  const ok = makeUf2(image(1000, 0), APP_FLASH.start);
  const refuses = (file: Uint8Array, why: RegExp) =>
    assert.throws(() => parseUf2(file), (err: unknown) => err instanceof Uf2Error && why.test(err.message));

  refuses(new Uint8Array(0), /not a UF2/);
  refuses(ok.subarray(0, 700), /not a UF2/);
  const damaged = ok.slice();
  damaged[512 + 3] = damaged[512 + 3]! ^ 0xff;
  refuses(damaged, /damaged/);
  refuses(makeUf2(image(1000, 0), APP_FLASH.start, 0xe48bff56), /another chip/); // RP2040
  refuses(makeUf2(image(1000, 0), 0x0), /outside the firmware area/); // over the SoftDevice
  refuses(makeUf2(image(1000, 0), APP_FLASH.end - 512), /outside the firmware area/); // into the bootloader's pages
  refuses(ok.subarray(0, 1024), /incomplete/);
  const twice = ok.slice();
  twice.set(ok.subarray(0, 512), 512); // block 0 again instead of block 1
  refuses(twice, /numbering/);
});

test("the info block layout matches kb/version.h", () => {
  const header = readFileSync(new URL("../../../firmware/core/include/kb/version.h", import.meta.url), "utf8");
  assert.match(header, /#define KB_FW_INFO_MAGIC "KBFWINFO"/);
  assert.match(header, /#define KB_FW_INFO_SIZE 32u/);
  assert.equal(encodeFirmwareInfo({ protocolVersion: 1, version: "1.2.3", model: "X" }).length, 32);
});

test("version comparison", () => {
  assert.ok(compareVersions("0.2.0", "0.1.9") > 0);
  assert.ok(compareVersions("0.10.0", "0.9.0") > 0);
  assert.ok(compareVersions("1.0.0", "1.0.1") < 0);
  assert.equal(compareVersions("1.2.3", "1.2.3"), 0);
  assert.equal(compareVersions("0.1.0-dev", "0.1.0"), 0);
});

test("reads the Board-ID from a UF2 drive's INFO_UF2.TXT", () => {
  const info = "UF2 Bootloader 0.9.2 lib/nrfx (v2.0.0)\r\nModel: Dachshund60F\r\nBoard-ID: nRF52840-Dachshund60F-v1\r\nDate: Oct 23 2026\r\n";
  assert.equal(uf2BoardId(info), "nRF52840-Dachshund60F-v1");
  assert.equal(uf2BoardId("hello"), null);
});
