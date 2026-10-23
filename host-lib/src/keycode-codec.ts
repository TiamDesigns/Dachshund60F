/**
 * Keycode ids (as the GUI and keycodes.ts use them, e.g. "KC_A", "MO(1)",
 * "LSFT(KC_1)") to the firmware's 16-bit numbers and back. The numbers come
 * from keycodes.gen.ts, generated from firmware/core/include/kb/keycodes.h.
 *
 * Numbers with no name decode to "RAW(0x1234)", which encodes back to the
 * same number, so a keymap always round-trips even when the GUI doesn't know
 * a keycode yet.
 */
import { KEYCODE_CONSTANTS as K } from "./keycodes.gen.js";
import type { KeycodeId } from "./types.js";

const c = (name: string): number => {
  const v = K[name];
  if (v === undefined) throw new Error(`keycodes.h has no ${name}`);
  return v;
};

/** Keycode names: everything in keycodes.h that is a keycode, not a range or bit. */
const NAMED = new Map<string, number>(
  Object.entries(K).filter(([name]) => /^(KC|QK|CW|NK|AS|OUT|BT|MS|DM)_/.test(name)),
);
const BY_NUMBER = new Map<number, string>();
for (const [name, value] of NAMED) {
  // First name wins, so KC_NO isn't shadowed by a later alias.
  if (!BY_NUMBER.has(value)) BY_NUMBER.set(value, name);
}

const LAYER_OPS: Record<string, number> = {
  MO: c("KB_LAYER_OP_MO"),
  TG: c("KB_LAYER_OP_TG"),
  TO: c("KB_LAYER_OP_TO"),
  DF: c("KB_LAYER_OP_DF"),
  OSL: c("KB_LAYER_OP_OSL"),
};
const LAYER_OP_NAMES = Object.fromEntries(Object.entries(LAYER_OPS).map(([k, v]) => [v, k]));

/** Single-modifier wrappers: LSFT(kc) and friends, and the mod-tap aliases LSFT_T(kc). */
const MODS: Record<string, number> = {
  LCTL: c("MOD_LCTL"),
  LSFT: c("MOD_LSFT"),
  LALT: c("MOD_LALT"),
  LGUI: c("MOD_LGUI"),
  RCTL: c("MOD_RCTL"),
  RSFT: c("MOD_RSFT"),
  RALT: c("MOD_RALT"),
  RGUI: c("MOD_RGUI"),
};
const MOD_NAMES = Object.fromEntries(Object.entries(MODS).map(([k, v]) => [v, k]));

const BT_SEL_BASE = c("BT_CLR") + 0x10; // BT_SEL(n) = 0x7120 | n in keycodes.h
const BT_PROFILE_MAX = c("KB_BT_PROFILE_MAX");

export class KeycodeError extends Error {}

function int(text: string, max: number, what: string): number {
  const n = /^0x[0-9a-f]+$/i.test(text) ? parseInt(text, 16) : /^\d+$/.test(text) ? Number(text) : NaN;
  if (!Number.isInteger(n) || n < 0 || n > max) throw new KeycodeError(`${what} out of range: ${text}`);
  return n;
}

function modsOf(text: string): number {
  const named = MODS[text.replace(/^MOD_/, "")];
  return named ?? int(text, 0x1f, "modifiers");
}

/** The 16-bit number for a keycode id. Throws KeycodeError for anything unknown. */
export function encodeKeycode(id: KeycodeId): number {
  const plain = NAMED.get(id);
  if (plain !== undefined) return plain;

  const m = /^([A-Z_]+)\((.*)\)$/.exec(id.replace(/\s+/g, ""));
  if (!m) throw new KeycodeError(`unknown keycode ${id}`);
  const fn = m[1]!;
  const args = splitArgs(m[2]!);

  if (fn === "RAW" && args.length === 1) return int(args[0]!, 0xffff, "keycode");
  if (fn in LAYER_OPS && args.length === 1) {
    return c("KB_KC_LAYER") | (LAYER_OPS[fn]! << 8) | int(args[0]!, 0xff, "layer");
  }
  if (fn === "BT_SEL" && args.length === 1) return BT_SEL_BASE | int(args[0]!, BT_PROFILE_MAX - 1, "profile");
  if (fn === "OSM" && args.length === 1) return c("KB_KC_OSM") | modsOf(args[0]!);
  if (fn === "M" && args.length === 1) {
    return c("KB_KC_MACRO") + int(args[0]!, c("KB_KC_MACRO_MAX") - c("KB_KC_MACRO"), "macro");
  }
  if (fn === "TD" && args.length === 1) {
    return c("KB_KC_TAP_DANCE") + int(args[0]!, c("KB_KC_TAP_DANCE_MAX") - c("KB_KC_TAP_DANCE"), "tap dance");
  }
  if (fn === "LT" && args.length === 2) {
    return c("KB_KC_LT") | (int(args[0]!, 0xf, "layer") << 8) | basicKey(args[1]!);
  }
  if (fn === "MT" && args.length === 2) return c("KB_KC_MT") | (modsOf(args[0]!) << 8) | basicKey(args[1]!);
  if (fn.endsWith("_T") && fn.slice(0, -2) in MODS && args.length === 1) {
    return c("KB_KC_MT") | (MODS[fn.slice(0, -2)]! << 8) | basicKey(args[0]!);
  }
  if (fn in MODS && args.length === 1) return c("KB_KC_MODDED") | (MODS[fn]! << 8) | basicKey(args[0]!);
  throw new KeycodeError(`unknown keycode ${id}`);
}

function splitArgs(text: string): string[] {
  const out: string[] = [];
  let depth = 0;
  let cur = "";
  for (const ch of text) {
    if (ch === "(") depth++;
    if (ch === ")") depth--;
    if (ch === "," && depth === 0) {
      out.push(cur);
      cur = "";
    } else cur += ch;
  }
  out.push(cur);
  return out;
}

/** A basic HID key (KC_A ... KC_RGUI) as its 8-bit usage. */
function basicKey(id: string): number {
  const v = NAMED.get(id);
  if (v === undefined || v > c("KB_KC_BASIC_MAX")) throw new KeycodeError(`${id} is not a basic key`);
  return v;
}

function keyName(usage: number): string | undefined {
  const name = BY_NUMBER.get(usage);
  return name && usage <= c("KB_KC_BASIC_MAX") && usage > 1 ? name : undefined;
}

const hex = (n: number): string => `RAW(0x${n.toString(16).padStart(4, "0")})`;

/** The id for a 16-bit keycode number; unnamed numbers come back as RAW(0x....). */
export function decodeKeycode(n: number): KeycodeId {
  if (!Number.isInteger(n) || n < 0 || n > 0xffff) throw new KeycodeError(`not a 16-bit keycode: ${n}`);
  const plain = BY_NUMBER.get(n);
  if (plain !== undefined) return plain;

  const hi = n & 0xff00;
  const lo = n & 0xff;
  if (n >= c("KB_KC_LAYER") && n <= c("KB_KC_LAYER_MAX")) {
    const op = LAYER_OP_NAMES[(n >> 8) & 0xf];
    return op ? `${op}(${lo})` : hex(n);
  }
  if (n >= BT_SEL_BASE && n < BT_SEL_BASE + BT_PROFILE_MAX) return `BT_SEL(${n - BT_SEL_BASE})`;
  if (n >= c("KB_KC_MACRO") && n <= c("KB_KC_MACRO_MAX")) return `M(${n - c("KB_KC_MACRO")})`;
  if (n >= c("KB_KC_TAP_DANCE") && n <= c("KB_KC_TAP_DANCE_MAX")) return `TD(${n - c("KB_KC_TAP_DANCE")})`;
  if (n >= c("KB_KC_OSM") && n <= c("KB_KC_OSM_MAX")) {
    const mod = MOD_NAMES[n & 0x1f];
    return mod ? `OSM(MOD_${mod})` : hex(n);
  }
  const key = keyName(lo);
  if (key && n >= c("KB_KC_LT") && n <= c("KB_KC_LT_MAX")) return `LT(${(hi >> 8) & 0xf},${key})`;
  if (key && n >= c("KB_KC_MT") && n <= c("KB_KC_MT_MAX")) {
    const mod = MOD_NAMES[(n >> 8) & 0x1f];
    return mod ? `${mod}_T(${key})` : hex(n);
  }
  if (key && n >= c("KB_KC_MODDED") && n <= c("KB_KC_MODDED_MAX")) {
    const mod = MOD_NAMES[(n >> 8) & 0x1f];
    return mod ? `${mod}(${key})` : hex(n);
  }
  return hex(n);
}
