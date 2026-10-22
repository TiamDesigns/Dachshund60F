/**
 * Keymap helpers for the editor, independent of any board connection.
 */

import type { KeycodeId } from "@dachshund60f/host-lib";

export const TRANSPARENT: KeycodeId = "KC_TRNS";
export const NO_KEY: KeycodeId = "KC_NO";

export interface ResolvedKey {
  /** What this layer has at the position (may be KC_TRNS). */
  own: KeycodeId;
  /** What actually happens, after falling through transparent keys. */
  effective: KeycodeId;
  /** Layer the effective action comes from. */
  fromLayer: number;
}

/**
 * Resolve a key on a layer, falling through KC_TRNS to lower layers as the
 * firmware does when only that layer and the base layer are active.
 */
export function resolveKey(keys: KeycodeId[][][], layer: number, row: number, col: number): ResolvedKey {
  const own = keys[layer]?.[row]?.[col] ?? NO_KEY;
  for (let l = layer; l >= 0; l--) {
    const kc = keys[l]?.[row]?.[col] ?? NO_KEY;
    if (kc !== TRANSPARENT) {
      return { own, effective: kc, fromLayer: l };
    }
  }
  return { own, effective: NO_KEY, fromLayer: 0 };
}

/** Number of positions on a layer that differ from transparent and no-op. */
export function countAssigned(layer: KeycodeId[][]): number {
  return layer.flat().filter((kc) => kc !== TRANSPARENT && kc !== NO_KEY).length;
}
