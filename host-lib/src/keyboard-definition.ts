/**
 * Types and helpers for boards/<model>/keyboard.json, the physical layout the
 * GUI renders. The host-lib board tests validate every definition file.
 */

export interface MatrixPosition {
  row: number;
  col: number;
}

export interface DefinitionKey extends MatrixPosition {
  x: number;
  y: number;
  w?: number;
  h?: number;
  legend: string[];
  /** [layoutOptionIndex, choiceIndex]: present only for that choice. */
  option?: [number, number];
}

export interface LayoutOption {
  id: string;
  label: string;
  choices: string[];
  default: number;
}

export interface DefinitionEncoder {
  id: number;
  x: number;
  y: number;
  push?: MatrixPosition;
}

export interface DefinitionIndicator {
  id: number;
  role: "layer" | "battery" | "lock";
  label: string;
  level?: number;
  underKey?: MatrixPosition;
  at?: { x: number; y: number };
}

export interface KeyboardDefinition {
  schema: number;
  name: string;
  matrix: { rows: number; cols: number };
  keys: DefinitionKey[];
  layoutOptions: LayoutOption[];
  encoders: DefinitionEncoder[];
  indicators: DefinitionIndicator[];
}

/** Default choice for every layout option. */
export function defaultLayoutChoices(def: KeyboardDefinition): number[] {
  return def.layoutOptions.map((o) => o.default);
}

/** Keys present for one combination of layout option choices. */
export function visibleKeys(def: KeyboardDefinition, choices: readonly number[]): DefinitionKey[] {
  return def.keys.filter((k) => !k.option || choices[k.option[0]] === k.option[1]);
}

/** Whether (row, col) is a real switch position (any layout choice, or an encoder push). */
export function isMatrixPositionUsed(def: KeyboardDefinition, row: number, col: number): boolean {
  return (
    def.keys.some((k) => k.row === row && k.col === col) ||
    def.encoders.some((e) => e.push?.row === row && e.push?.col === col)
  );
}
