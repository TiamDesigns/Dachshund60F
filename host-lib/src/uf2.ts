/**
 * UF2 firmware files: checking one before it goes onto a board, and reading
 * the firmware info block (kb/version.h) out of it.
 *
 * A UF2 file is 512-byte blocks, each carrying up to 476 bytes for one flash
 * address (https://github.com/microsoft/uf2). The board's bootloader writes
 * whatever blocks it's given, so this is the last chance to refuse a file
 * for another chip, another board, or the wrong part of flash.
 */

const BLOCK = 512;
const MAGIC_START0 = 0x0a324655;
const MAGIC_START1 = 0x9e5d5157;
const MAGIC_END = 0x0ab16f30;
const FLAG_NOT_MAIN_FLASH = 0x00000001;
const FLAG_FAMILY_ID = 0x00002000;
const MAX_PAYLOAD = 476;

/** UF2 family of the nRF52840 (what the Adafruit bootloader accepts). */
export const NRF52840_FAMILY = 0xada52840;

/**
 * Where application firmware may go on the Dachshund60F: after the S140
 * SoftDevice and below the region the bootloader reserves (see
 * boards/Dachshund60F/bootloader/README.md).
 */
export const APP_FLASH = { start: 0x26000, end: 0xea000 } as const;

const INFO_MAGIC = new TextEncoder().encode("KBFWINFO");
const INFO_SIZE = 32;

/** The firmware info block (struct kb_fw_info). */
export interface FirmwareInfo {
  layout: number;
  protocolVersion: number;
  /** "major.minor.patch", as the handshake reports it. */
  version: string;
  model: string;
}

export interface Uf2Image {
  /** Lowest and one past the highest flash address written. */
  start: number;
  end: number;
  blocks: number;
  /** Null when the image has no info block (another project's firmware). */
  info: FirmwareInfo | null;
}

/** A file that isn't firmware this board can take. */
export class Uf2Error extends Error {
  override name = "Uf2Error";
}

const hex = (n: number) => `0x${n.toString(16)}`;

/**
 * Check a UF2 file block by block and read its info block. Throws Uf2Error
 * for anything malformed, for another chip family, or for blocks outside
 * the application flash.
 */
export function parseUf2(file: Uint8Array): Uf2Image {
  if (file.length === 0 || file.length % BLOCK !== 0) {
    throw new Uf2Error("not a UF2 file: its size isn't a whole number of 512-byte blocks");
  }
  const v = new DataView(file.buffer, file.byteOffset, file.byteLength);
  const count = file.length / BLOCK;
  const chunks: { addr: number; data: Uint8Array }[] = [];
  let total: number | null = null;
  const seen = new Set<number>();

  for (let i = 0; i < count; i++) {
    const o = i * BLOCK;
    if (
      v.getUint32(o, true) !== MAGIC_START0 ||
      v.getUint32(o + 4, true) !== MAGIC_START1 ||
      v.getUint32(o + BLOCK - 4, true) !== MAGIC_END
    ) {
      throw new Uf2Error(`not a UF2 file: block ${i} is damaged`);
    }
    const flags = v.getUint32(o + 8, true);
    const addr = v.getUint32(o + 12, true);
    const size = v.getUint32(o + 16, true);
    const blockNo = v.getUint32(o + 20, true);
    const numBlocks = v.getUint32(o + 24, true);
    const family = v.getUint32(o + 28, true);

    if (flags & FLAG_NOT_MAIN_FLASH) continue;
    if (!(flags & FLAG_FAMILY_ID) || family !== NRF52840_FAMILY) {
      throw new Uf2Error(`this file is for another chip (family ${hex(family)}), not the nRF52840`);
    }
    if (size === 0 || size > MAX_PAYLOAD) throw new Uf2Error(`block ${i} has a bad payload size`);
    if (total === null) total = numBlocks;
    if (numBlocks !== total || blockNo >= total || seen.has(blockNo)) {
      throw new Uf2Error(`block ${i} doesn't fit the file's numbering`);
    }
    seen.add(blockNo);
    if (addr < APP_FLASH.start || addr + size > APP_FLASH.end) {
      throw new Uf2Error(
        `the file writes ${hex(addr)}, outside the firmware area (${hex(APP_FLASH.start)}-${hex(APP_FLASH.end)}); ` +
          "it may be a bootloader or another board's firmware",
      );
    }
    chunks.push({ addr, data: file.subarray(o + 32, o + 32 + size) });
  }
  if (total === null || seen.size !== total) {
    throw new Uf2Error("the file is incomplete: some blocks are missing");
  }

  const start = Math.min(...chunks.map((c) => c.addr));
  const end = Math.max(...chunks.map((c) => c.addr + c.data.length));
  const image = new Uint8Array(end - start).fill(0xff);
  for (const c of chunks) image.set(c.data, c.addr - start);
  return { start, end, blocks: total, info: findInfo(image) };
}

function findInfo(image: Uint8Array): FirmwareInfo | null {
  outer: for (let i = 0; i + INFO_SIZE <= image.length; i++) {
    for (let k = 0; k < INFO_MAGIC.length; k++) {
      if (image[i + k] !== INFO_MAGIC[k]) continue outer;
    }
    const b = image.subarray(i, i + INFO_SIZE);
    const model = b.subarray(16, 32);
    const nul = model.indexOf(0);
    return {
      layout: b[8]!,
      protocolVersion: b[9]!,
      version: `${b[10]}.${b[11]}.${b[12]}`,
      model: new TextDecoder().decode(nul < 0 ? model : model.subarray(0, nul)),
    };
  }
  return null;
}

/** Compare "major.minor.patch" versions (a suffix such as "-dev" is ignored): negative if a is older than b. */
export function compareVersions(a: string, b: string): number {
  const parts = (v: string) => v.split(".").map((p) => Number.parseInt(p, 10) || 0);
  const pa = parts(a);
  const pb = parts(b);
  for (let i = 0; i < 3; i++) {
    const d = (pa[i] ?? 0) - (pb[i] ?? 0);
    if (d !== 0) return d;
  }
  return 0;
}

/**
 * The Board-ID line of a UF2 drive's INFO_UF2.TXT, or null. The
 * Dachshund60F's bootloader says "nRF52840-Dachshund60F-v1".
 */
export function uf2BoardId(infoText: string): string | null {
  const m = /^Board-ID:\s*(\S+)/m.exec(infoText);
  return m ? m[1]! : null;
}

/** Build a UF2 file from an image, for tests and the demo (256-byte payloads, like most tools). */
export function makeUf2(image: Uint8Array, start: number, family = NRF52840_FAMILY): Uint8Array {
  const payload = 256;
  const count = Math.ceil(image.length / payload);
  const out = new Uint8Array(count * BLOCK);
  const v = new DataView(out.buffer);
  for (let i = 0; i < count; i++) {
    const o = i * BLOCK;
    const data = image.subarray(i * payload, (i + 1) * payload);
    v.setUint32(o, MAGIC_START0, true);
    v.setUint32(o + 4, MAGIC_START1, true);
    v.setUint32(o + 8, FLAG_FAMILY_ID, true);
    v.setUint32(o + 12, start + i * payload, true);
    v.setUint32(o + 16, data.length, true);
    v.setUint32(o + 20, i, true);
    v.setUint32(o + 24, count, true);
    v.setUint32(o + 28, family, true);
    out.set(data, o + 32);
    v.setUint32(o + BLOCK - 4, MAGIC_END, true);
  }
  return out;
}

/** A firmware info block's bytes (struct kb_fw_info), for tests and the demo. */
export function encodeFirmwareInfo(info: Omit<FirmwareInfo, "layout"> & { layout?: number }): Uint8Array {
  const b = new Uint8Array(INFO_SIZE);
  b.set(INFO_MAGIC, 0);
  const [major, minor, patch] = info.version.split(".").map(Number);
  b[8] = info.layout ?? 1;
  b[9] = info.protocolVersion;
  b[10] = major ?? 0;
  b[11] = minor ?? 0;
  b[12] = patch ?? 0;
  b.set(new TextEncoder().encode(info.model).subarray(0, 16), 16);
  return b;
}
