/**
 * Keycode catalog for the GUI palette: every action a key can be given,
 * with a short keycap label, a longer name and a category.
 *
 * Identifiers follow QMK/ZMK naming so they read familiarly, and match the
 * macro names of the firmware's 16-bit keycode model
 * (firmware/core/include/kb/keycodes.h; test/firmware-keymap.test.ts checks
 * every id here is defined there). The protocol (phase 5) maps ids to those
 * numbers.
 */

import type { KeycodeId } from "./types.js";

export type KeycodeCategory =
  | "basic"
  | "modifiers"
  | "navigation"
  | "function"
  | "media"
  | "layers"
  | "connection"
  | "mouse"
  | "macros"
  | "special";

export const KEYCODE_CATEGORIES: readonly { id: KeycodeCategory; label: string }[] = [
  { id: "basic", label: "Basic" },
  { id: "modifiers", label: "Modifiers" },
  { id: "navigation", label: "Navigation" },
  { id: "function", label: "Function" },
  { id: "media", label: "Media" },
  { id: "layers", label: "Layers" },
  { id: "connection", label: "Connection" },
  { id: "mouse", label: "Mouse" },
  { id: "macros", label: "Macros & dances" },
  { id: "special", label: "Special" },
];

export interface KeycodeInfo {
  id: KeycodeId;
  /** Short keycap text. */
  label: string;
  /** Shifted symbol shown above the label, if any. */
  shifted?: string;
  /** Full name for tooltips, search and screen readers. */
  name: string;
  category: KeycodeCategory;
}

export const NO_KEY: KeycodeId = "KC_NO";
export const TRANSPARENT: KeycodeId = "KC_TRNS";

function k(id: string, label: string, name: string, category: KeycodeCategory, shifted?: string): KeycodeInfo {
  return shifted === undefined ? { id, label, name, category } : { id, label, name, category, shifted };
}

const letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  .split("")
  .map((c) => k(`KC_${c}`, c, c, "basic"));

const digits = [
  ["1", "!"],
  ["2", "@"],
  ["3", "#"],
  ["4", "$"],
  ["5", "%"],
  ["6", "^"],
  ["7", "&"],
  ["8", "*"],
  ["9", "("],
  ["0", ")"],
].map(([d, s]) => k(`KC_${d}`, d!, `${d} ${s}`, "basic", s));

const STATIC_KEYCODES: KeycodeInfo[] = [
  ...letters,
  ...digits,
  k("KC_MINS", "-", "Minus _", "basic", "_"),
  k("KC_EQL", "=", "Equals +", "basic", "+"),
  k("KC_LBRC", "[", "Left bracket {", "basic", "{"),
  k("KC_RBRC", "]", "Right bracket }", "basic", "}"),
  k("KC_BSLS", "\\", "Backslash |", "basic", "|"),
  k("KC_SCLN", ";", "Semicolon :", "basic", ":"),
  k("KC_QUOT", "'", "Quote \"", "basic", "\""),
  k("KC_GRV", "`", "Grave ~", "basic", "~"),
  k("KC_COMM", ",", "Comma <", "basic", "<"),
  k("KC_DOT", ".", "Period >", "basic", ">"),
  k("KC_SLSH", "/", "Slash ?", "basic", "?"),
  k("KC_SPC", "Space", "Space", "basic"),
  k("KC_ENT", "Enter", "Enter", "basic"),
  k("KC_ESC", "Esc", "Escape", "basic"),
  k("KC_BSPC", "Bksp", "Backspace", "basic"),
  k("KC_TAB", "Tab", "Tab", "basic"),
  k("KC_CAPS", "Caps", "Caps Lock", "basic"),
  k("KC_DEL", "Del", "Delete", "basic"),

  k("KC_LCTL", "Ctrl", "Left Control", "modifiers"),
  k("KC_LSFT", "Shift", "Left Shift", "modifiers"),
  k("KC_LALT", "Alt", "Left Alt", "modifiers"),
  k("KC_LGUI", "Win", "Left Windows / Command", "modifiers"),
  k("KC_RCTL", "Ctrl", "Right Control", "modifiers"),
  k("KC_RSFT", "Shift", "Right Shift", "modifiers"),
  k("KC_RALT", "AltGr", "Right Alt / AltGr", "modifiers"),
  k("KC_RGUI", "Win", "Right Windows / Command", "modifiers"),

  k("KC_LEFT", "←", "Left arrow", "navigation"),
  k("KC_DOWN", "↓", "Down arrow", "navigation"),
  k("KC_UP", "↑", "Up arrow", "navigation"),
  k("KC_RGHT", "→", "Right arrow", "navigation"),
  k("KC_HOME", "Home", "Home", "navigation"),
  k("KC_END", "End", "End", "navigation"),
  k("KC_PGUP", "PgUp", "Page Up", "navigation"),
  k("KC_PGDN", "PgDn", "Page Down", "navigation"),
  k("KC_INS", "Ins", "Insert", "navigation"),
  k("KC_PSCR", "PrtSc", "Print Screen", "navigation"),
  k("KC_SCRL", "ScrLk", "Scroll Lock", "navigation"),
  k("KC_PAUS", "Pause", "Pause / Break", "navigation"),
  k("KC_NUM", "NumLk", "Num Lock", "navigation"),
  k("KC_APP", "Menu", "Menu / Application", "navigation"),

  ...Array.from({ length: 24 }, (_, i) => k(`KC_F${i + 1}`, `F${i + 1}`, `F${i + 1}`, "function")),

  k("KC_MUTE", "Mute", "Mute", "media"),
  k("KC_VOLD", "Vol-", "Volume down", "media"),
  k("KC_VOLU", "Vol+", "Volume up", "media"),
  k("KC_MPLY", "Play", "Play / Pause", "media"),
  k("KC_MSTP", "Stop", "Stop", "media"),
  k("KC_MPRV", "Prev", "Previous track", "media"),
  k("KC_MNXT", "Next", "Next track", "media"),
  k("KC_BRID", "Bri-", "Screen brightness down", "media"),
  k("KC_BRIU", "Bri+", "Screen brightness up", "media"),
  k("KC_CALC", "Calc", "Calculator", "media"),
  k("KC_SLEP", "Sleep", "System sleep", "media"),

  k("OUT_USB", "USB", "Send keys over USB", "connection"),
  k("OUT_2G4", "2.4G", "Send keys over the 2.4 GHz dongle", "connection"),
  k("OUT_BLE", "BT", "Send keys over Bluetooth", "connection"),
  k("RF_PAIR", "2.4G Pair", "Pair with a 2.4 GHz dongle (press the dongle's button too)", "connection"),
  k("BT_CLR", "BT Clr", "Forget the current Bluetooth host", "connection"),

  k("MS_UP", "M↑", "Mouse up", "mouse"),
  k("MS_DOWN", "M↓", "Mouse down", "mouse"),
  k("MS_LEFT", "M←", "Mouse left", "mouse"),
  k("MS_RGHT", "M→", "Mouse right", "mouse"),
  k("MS_BTN1", "Click", "Mouse button 1 (left)", "mouse"),
  k("MS_BTN2", "RClick", "Mouse button 2 (right)", "mouse"),
  k("MS_BTN3", "MClick", "Mouse button 3 (middle)", "mouse"),
  k("MS_BTN4", "Back", "Mouse button 4 (back)", "mouse"),
  k("MS_BTN5", "Fwd", "Mouse button 5 (forward)", "mouse"),
  k("MS_WHLU", "Wh↑", "Scroll up", "mouse"),
  k("MS_WHLD", "Wh↓", "Scroll down", "mouse"),
  k("MS_WHLL", "Wh←", "Scroll left", "mouse"),
  k("MS_WHLR", "Wh→", "Scroll right", "mouse"),

  k("DM_REC1", "Rec 1", "Record dynamic macro 1", "macros"),
  k("DM_REC2", "Rec 2", "Record dynamic macro 2", "macros"),
  k("DM_RSTP", "Stop", "Stop recording", "macros"),
  k("DM_PLY1", "Play 1", "Play dynamic macro 1", "macros"),
  k("DM_PLY2", "Play 2", "Play dynamic macro 2", "macros"),

  k("KC_NO", "", "Nothing", "special"),
  k("KC_TRNS", "▽", "Transparent: use the key below on a lower layer", "special"),
  k("CW_TOGG", "CapsWd", "Caps Word: capitalise until the end of the word", "special"),
  k("QK_REP", "Repeat", "Repeat the last key", "special"),
  k("QK_LEAD", "Leader", "Leader key", "special"),
  k("AS_TOGG", "AutoSh", "Turn Auto Shift on or off", "special"),
  k("QK_BOOT", "Boot", "Reboot into the firmware updater", "special"),
  k("QK_RBT", "Reboot", "Reboot the keyboard", "special"),
  k("QK_SLEEP", "Sleep", "Put the keyboard into deep sleep (on battery); any key wakes it", "special"),
  k("NK_TOGG", "NKRO", "Turn N-key rollover on or off", "special"),
];

const BY_ID = new Map(STATIC_KEYCODES.map((info) => [info.id, info]));

const LAYER_ACTIONS = {
  MO: { label: "MO", name: "Hold for layer" },
  TG: { label: "TG", name: "Toggle layer" },
  TO: { label: "TO", name: "Switch to layer" },
  OSL: { label: "OSL", name: "Next key on layer" },
} as const;

type LayerAction = keyof typeof LAYER_ACTIONS;

/** Bluetooth host profiles reachable with BT_SEL(n). */
export const BLE_PROFILE_COUNT = 3;

/** Macros M(n) and tap dances TD(n) the firmware has slots for (kb/features.h). */
export const MACRO_COUNT = 32;
export const TAP_DANCE_COUNT = 16;

const PARAM = /^([A-Z_]+)\((\d+)\)$/;

/** Layer and profile keycodes for a board with layerCount layers. */
export function parameterisedKeycodes(layerCount: number): KeycodeInfo[] {
  const out: KeycodeInfo[] = [];
  for (const action of Object.keys(LAYER_ACTIONS) as LayerAction[]) {
    for (let n = 0; n < layerCount; n++) {
      out.push(describe(`${action}(${n})`, layerCount)!);
    }
  }
  for (let n = 0; n < BLE_PROFILE_COUNT; n++) {
    out.push(describe(`BT_SEL(${n})`, layerCount)!);
  }
  for (let n = 0; n < MACRO_COUNT; n++) out.push(describe(`M(${n})`, layerCount)!);
  for (let n = 0; n < TAP_DANCE_COUNT; n++) out.push(describe(`TD(${n})`, layerCount)!);
  return out;
}

/** Every keycode valid on a board with layerCount layers, in palette order. */
export function allKeycodes(layerCount: number): KeycodeInfo[] {
  return [...STATIC_KEYCODES, ...parameterisedKeycodes(layerCount)];
}

function describe(id: KeycodeId, layerCount: number): KeycodeInfo | undefined {
  const fixed = BY_ID.get(id);
  if (fixed) return fixed;

  const m = PARAM.exec(id);
  if (!m) return undefined;
  const fn = m[1]!;
  const n = Number(m[2]);

  if (fn in LAYER_ACTIONS) {
    if (n >= layerCount) return undefined;
    const a = LAYER_ACTIONS[fn as LayerAction];
    return { id, label: `${a.label} ${n}`, name: `${a.name} ${n}`, category: "layers" };
  }
  if (fn === "BT_SEL" && n < BLE_PROFILE_COUNT) {
    return { id, label: `BT${n + 1}`, name: `Bluetooth host ${n + 1}`, category: "connection" };
  }
  if (fn === "M" && n < MACRO_COUNT) return { id, label: `M${n}`, name: `Macro ${n}`, category: "macros" };
  if (fn === "TD" && n < TAP_DANCE_COUNT) return { id, label: `TD${n}`, name: `Tap dance ${n}`, category: "macros" };
  return undefined;
}

/** Details for a keycode, or undefined if it is not valid for this board. */
export function keycodeInfo(id: KeycodeId, layerCount: number): KeycodeInfo | undefined {
  return describe(id, layerCount);
}

export function isValidKeycode(id: KeycodeId, layerCount: number): boolean {
  return describe(id, layerCount) !== undefined;
}

/** Layer a keycode switches to, if it is a layer key. */
export function keycodeLayer(id: KeycodeId): number | undefined {
  const m = PARAM.exec(id);
  return m && m[1]! in LAYER_ACTIONS ? Number(m[2]) : undefined;
}
