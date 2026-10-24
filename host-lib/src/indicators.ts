/**
 * Indicator colours: defaults matching the firmware (kb/indicators.c), the
 * checks it makes, and the "#rrggbb" <-> 0x00RRGGBB conversion the protocol
 * uses. Shared by the mock and HidBoard.
 */
import { BoardError, type Colour, type IndicatorColours } from "./types.js";

const LAYER_DEFAULTS = ["#000000", "#ff8c00", "#00c8a0", "#aa3cff"];

/** The firmware's defaults: base layer off, then amber, teal and violet (repeating). */
export function defaultIndicatorColours(layerCount: number): IndicatorColours {
  return {
    layers: Array.from({ length: layerCount }, (_, i) =>
      i < LAYER_DEFAULTS.length ? LAYER_DEFAULTS[i]! : LAYER_DEFAULTS[1 + ((i - 1) % 3)]!,
    ),
    caps: "#ffdcb4",
    num: "#ffdcb4",
    scroll: "#ffdcb4",
    battery: "#00ff3c",
    batteryLow: "#ff1e00",
    batteryLowPercent: 15,
  };
}

export function isColour(value: unknown): value is Colour {
  return typeof value === "string" && /^#[0-9a-f]{6}$/.test(value);
}

export function colourToNumber(colour: Colour): number {
  if (!isColour(colour)) throw new BoardError("invalid-value", `${String(colour)} isn't a #rrggbb colour`);
  return Number.parseInt(colour.slice(1), 16);
}

export function numberToColour(value: number): Colour {
  return `#${(value & 0xffffff).toString(16).padStart(6, "0")}`;
}

/** Throw "invalid-value" unless every field is one the firmware would take. */
export function checkIndicatorColours(c: IndicatorColours, layerCount: number): void {
  for (const key of ["caps", "num", "scroll", "battery", "batteryLow"] as const) {
    if (!isColour(c[key])) throw new BoardError("invalid-value", `${key} must be a #rrggbb colour`);
  }
  if (!Array.isArray(c.layers) || c.layers.length !== layerCount || !c.layers.every(isColour)) {
    throw new BoardError("invalid-value", `layers must be ${layerCount} #rrggbb colours`);
  }
  const p = c.batteryLowPercent;
  if (!Number.isInteger(p) || p < 0 || p > 100) {
    throw new BoardError("invalid-value", "the low-battery level must be 0-100 %");
  }
}
