/** The checks the firmware makes on global settings (kb_settings_valid), shared by the mock and HidBoard. */
import { BoardError, DEBOUNCE_ALGORITHMS, SETTINGS_LIMITS, type BoardSettings } from "./types.js";

function inRange(value: number, limits: { min: number; max: number }): boolean {
  return Number.isInteger(value) && value >= limits.min && value <= limits.max;
}

/** UTF-8 bytes in a board name (the firmware's limit is in bytes). */
export function nameBytes(name: string): number {
  return new TextEncoder().encode(name).length;
}

/** Throw "invalid-value" unless the firmware would take these settings. */
export function checkSettings(s: BoardSettings): void {
  const L = SETTINGS_LIMITS;
  if (typeof s.name !== "string" || nameBytes(s.name) > L.nameMaxLength || s.name.includes("\0")) {
    throw new BoardError("invalid-value", `the name must be at most ${L.nameMaxLength} bytes`);
  }
  if (!DEBOUNCE_ALGORITHMS.includes(s.debounceAlgorithm)) {
    throw new BoardError("invalid-value", `unknown debounce algorithm ${String(s.debounceAlgorithm)}`);
  }
  if (!inRange(s.debounceMs, L.debounceMs)) throw new BoardError("invalid-value", "debounce out of range");
  if (!inRange(s.tappingTermMs, L.tappingTermMs)) throw new BoardError("invalid-value", "tapping term out of range");
  if (!inRange(s.sleepTimeoutMin, L.sleepTimeoutMin)) throw new BoardError("invalid-value", "sleep timeout out of range");
  if (!inRange(s.indicatorBrightness, L.indicatorBrightness)) {
    throw new BoardError("invalid-value", "indicator brightness out of range");
  }
  if (typeof s.nkro !== "boolean") throw new BoardError("invalid-value", "nkro must be true or false");
}
