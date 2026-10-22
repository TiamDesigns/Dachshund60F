/**
 * Pixel geometry for drawing a board from its keyboard definition
 * (boards/<model>/keyboard.json, positions in key units).
 */

import type {
  DefinitionIndicator,
  DefinitionKey,
  KeyboardDefinition,
} from "@dachshund60f/host-lib";

export interface Rect {
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface KeyRect extends Rect {
  key: DefinitionKey;
}

export interface EncoderShape {
  id: number;
  cx: number;
  cy: number;
  r: number;
  push?: { row: number; col: number };
}

export interface IndicatorDot {
  indicator: DefinitionIndicator;
  cx: number;
  cy: number;
}

export interface BoardGeometry {
  keys: KeyRect[];
  encoders: EncoderShape[];
  /** Free-standing LEDs; LEDs under a key are drawn on the key instead. */
  indicators: IndicatorDot[];
  width: number;
  height: number;
}

/** Board drawn at `unit` px per key unit, with `gap` px between keycaps. */
export function boardGeometry(
  def: KeyboardDefinition,
  choices: readonly number[],
  unit: number,
  gap: number,
): BoardGeometry {
  const keys = def.keys
    .filter((k) => !k.option || choices[k.option[0]] === k.option[1])
    .map((key) => ({
      key,
      x: key.x * unit + gap / 2,
      y: key.y * unit + gap / 2,
      w: (key.w ?? 1) * unit - gap,
      h: (key.h ?? 1) * unit - gap,
    }));

  const encoders = def.encoders.map((e) => ({
    id: e.id,
    cx: (e.x + 0.5) * unit,
    cy: (e.y + 0.5) * unit,
    r: unit * 0.42,
    ...(e.push ? { push: e.push } : {}),
  }));

  const indicators = def.indicators
    .filter((i) => i.at)
    .map((indicator) => ({ indicator, cx: indicator.at!.x * unit, cy: indicator.at!.y * unit }));

  const right = Math.max(
    0,
    ...keys.map((k) => k.x + k.w + gap / 2),
    ...encoders.map((e) => e.cx + unit / 2),
    ...indicators.map((d) => d.cx + unit * 0.15),
  );
  const bottom = Math.max(
    0,
    ...keys.map((k) => k.y + k.h + gap / 2),
    ...encoders.map((e) => e.cy + unit / 2),
    ...indicators.map((d) => d.cy + unit * 0.15),
  );

  return { keys, encoders, indicators, width: right, height: bottom };
}

/** Largest unit (px) that fits the board into `width` px, clamped to [min, max]. */
export function fitUnit(def: KeyboardDefinition, width: number, min = 30, max = 60): number {
  const atOne = boardGeometry(def, def.layoutOptions.map((o) => o.default), 1, 0);
  if (atOne.width <= 0) return max;
  return Math.max(min, Math.min(max, Math.floor(width / atOne.width)));
}
