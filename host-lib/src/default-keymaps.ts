/**
 * Default keymaps per board model, as [row][col] grids over the full matrix.
 * Unused matrix positions are KC_NO.
 *
 * The mock board uses these. They must match the firmware's compiled-in
 * default keymap (firmware/src/default_keymap.c); test/firmware-keymap.test.ts
 * compares the two.
 */

import type { EncoderAction, Keymap, KeycodeId } from "./types.js";

const ___ = "KC_TRNS";
const XXX = "KC_NO";

/* Dachshund60F: 6x15 matrix. See boards/Dachshund60F/README.md for positions. */
const DACHSHUND60F_BASE: KeycodeId[][] = [
  ["KC_ESC", "KC_F1", "KC_F2", "KC_F3", "KC_F4", "KC_F5", "KC_F6", "KC_F7", "KC_F8", "KC_F9", "KC_F10", "KC_F11", "KC_F12", "KC_DEL", "KC_MUTE"],
  ["KC_GRV", "KC_1", "KC_2", "KC_3", "KC_4", "KC_5", "KC_6", "KC_7", "KC_8", "KC_9", "KC_0", "KC_MINS", "KC_EQL", "KC_BSPC", XXX],
  ["KC_TAB", "KC_Q", "KC_W", "KC_E", "KC_R", "KC_T", "KC_Y", "KC_U", "KC_I", "KC_O", "KC_P", "KC_LBRC", "KC_RBRC", "KC_BSLS", XXX],
  ["KC_CAPS", "KC_A", "KC_S", "KC_D", "KC_F", "KC_G", "KC_H", "KC_J", "KC_K", "KC_L", "KC_SCLN", "KC_QUOT", XXX, "KC_ENT", XXX],
  ["KC_LSFT", "KC_Z", "KC_X", "KC_C", "KC_V", "KC_B", "KC_N", "KC_M", "KC_COMM", "KC_DOT", "KC_SLSH", XXX, "KC_RSFT", "KC_UP", XXX],
  ["KC_LCTL", "KC_LGUI", XXX, "KC_LALT", "MO(1)", XXX, "KC_SPC", XXX, "KC_RALT", XXX, "MO(1)", "KC_RCTL", "KC_LEFT", "KC_DOWN", "KC_RGHT"],
];

const DACHSHUND60F_FN: KeycodeId[][] = [
  ["QK_BOOT", "KC_BRID", "KC_BRIU", ___, ___, "KC_MPRV", "KC_MPLY", "KC_MNXT", ___, ___, "KC_MUTE", "KC_VOLD", "KC_VOLU", "KC_INS", "KC_MPLY"],
  [___, "BT_SEL(0)", "BT_SEL(1)", "BT_SEL(2)", "OUT_USB", "OUT_2G4", "OUT_BLE", ___, ___, ___, "BT_CLR", ___, ___, "KC_DEL", XXX],
  [___, ___, ___, ___, ___, ___, ___, ___, ___, ___, "KC_PSCR", "KC_SCRL", "KC_PAUS", ___, XXX],
  [___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, XXX, ___, XXX],
  [___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, XXX, ___, "KC_PGUP", XXX],
  [___, ___, XXX, ___, ___, XXX, ___, XXX, ___, XXX, ___, ___, "KC_HOME", "KC_PGDN", "KC_END"],
];

function transparentLayer(like: KeycodeId[][]): KeycodeId[][] {
  return like.map((row) => row.map((kc) => (kc === XXX ? XXX : ___)));
}

const DACHSHUND60F_ENCODERS: EncoderAction[][] = [
  [{ cw: "KC_VOLU", ccw: "KC_VOLD" }],
  [{ cw: "KC_MNXT", ccw: "KC_MPRV" }],
  [{ cw: ___, ccw: ___ }],
  [{ cw: ___, ccw: ___ }],
];

const DEFAULTS: Record<string, () => Keymap> = {
  Dachshund60F: () => ({
    keys: [
      DACHSHUND60F_BASE,
      DACHSHUND60F_FN,
      transparentLayer(DACHSHUND60F_BASE),
      transparentLayer(DACHSHUND60F_BASE),
    ].map((layer) => layer.map((row) => [...row])),
    encoders: DACHSHUND60F_ENCODERS.map((layer) => layer.map((e) => ({ ...e }))),
  }),
};

/** A fresh copy of the default keymap for a model, or undefined if unknown. */
export function defaultKeymap(model: string): Keymap | undefined {
  return DEFAULTS[model]?.();
}
