/**
 * Advanced keys shared by every board: the defaults, slot counts and limits
 * of the firmware's tables (kb/features.h), and the checks the mock makes
 * the way the firmware does (kb_features_valid).
 */
import { encodeKeycode, KeycodeError } from "./keycode-codec.js";
import { isValidKeycode, keycodeInfo } from "./keycodes.js";
import { encodeMacros } from "./advanced-codec.js";
import { BoardError, type AdvancedKeys, type AdvancedOptions, type AdvancedTable, type KeycodeId, type MacroStep } from "./types.js";

/** Slots per table, and keys per combo or Leader sequence. */
export const ADVANCED_SLOTS: Readonly<Record<AdvancedTable, number>> = {
  combos: 16,
  tapDances: 16,
  keyOverrides: 16,
  leader: 16,
  tapHold: 16,
};
export const COMBO_KEYS = { min: 2, max: 4 } as const;
export const LEADER_KEYS = { min: 1, max: 4 } as const;
/** Bytes of macro storage on the board. */
export const MACRO_BUFFER_SIZE = 1024;

/** Inclusive limits the firmware enforces. */
export const ADVANCED_LIMITS = {
  tappingTermMs: { min: 50, max: 2000 },
  quickTapMs: { min: 0, max: 2000 },
  comboTimeoutMs: { min: 0, max: 1000 },
  tapDanceTermMs: { min: 0, max: 2000 },
  leaderTimeoutMs: { min: 100, max: 5000 },
  capsWordIdleMs: { min: 0, max: 60000 },
  autoShiftMs: { min: 50, max: 1000 },
  mouseIntervalMs: { min: 1, max: 255 },
  mouseSpeed: { min: 1, max: 127 },
  mouseTimeToMaxMs: { min: 0, max: 10000 },
  wheelIntervalMs: { min: 1, max: 255 },
  macroDelayMs: { min: 0, max: 65535 },
} as const;

export const DEFAULT_ADVANCED_OPTIONS: Readonly<AdvancedOptions> = {
  leaderTimeoutMs: 300,
  capsWordOnBothShifts: false,
  capsWordIdleMs: 5000,
  autoShift: false,
  autoShiftMs: 175,
  mouse: { intervalMs: 16, moveDelta: 4, maxSpeed: 20, timeToMaxMs: 1000, wheelIntervalMs: 80 },
};

/** Empty tables and default options, as after a factory reset. */
export function defaultAdvancedKeys(): AdvancedKeys {
  const n = (table: AdvancedTable) => Array.from({ length: ADVANCED_SLOTS[table] });
  return {
    options: structuredClone(DEFAULT_ADVANCED_OPTIONS) as AdvancedOptions,
    combos: n("combos").map(() => ({ keys: [], result: "KC_NO", timeoutMs: 0, layers: [] })),
    tapDances: n("tapDances").map(() => ({ tap: "KC_NO", hold: "KC_NO", doubleTap: "KC_NO", tripleTap: "KC_NO", termMs: 0 })),
    keyOverrides: n("keyOverrides").map(() => ({ triggerMods: 0, triggerKey: "KC_NO", replacement: "KC_NO", layers: [] })),
    leader: n("leader").map(() => ({ keys: [], result: "KC_NO" })),
    tapHold: n("tapHold").map(() => null),
  };
}

/** Whether a slot holds an entry (the board skips unused slots). */
export function isAdvancedEntryUsed<T extends AdvancedTable>(table: T, entry: AdvancedKeys[T][number]): boolean {
  switch (table) {
    case "combos":
    case "leader":
      return (entry as AdvancedKeys["combos" | "leader"][number]).result !== "KC_NO";
    case "keyOverrides":
      return (entry as AdvancedKeys["keyOverrides"][number]).replacement !== "KC_NO";
    case "tapDances": {
      const t = entry as AdvancedKeys["tapDances"][number];
      return [t.tap, t.hold, t.doubleTap, t.tripleTap].some((k) => k !== "KC_NO");
    }
    case "tapHold":
      return entry !== null;
  }
  return false;
}

/** The first unused slot of a table, or -1 when it's full. */
export function firstFreeAdvancedSlot<T extends AdvancedTable>(table: T, entries: readonly AdvancedKeys[T][number][]): number {
  for (let i = 0; i < ADVANCED_SLOTS[table]; i++) {
    const e = entries[i];
    if (e === undefined || !isAdvancedEntryUsed(table, e)) return i;
  }
  return -1;
}

/** What an unused slot holds; set it to clear a slot. */
export function emptyAdvancedEntry<T extends AdvancedTable>(table: T): AdvancedKeys[T][number] {
  return defaultAdvancedKeys()[table][0] as AdvancedKeys[T][number];
}

/** Bytes the macros take on the board, or null if a step can't be stored at all. */
export function macroBytes(macros: readonly MacroStep[][]): number | null {
  try {
    return encodeMacros(macros).length;
  } catch (err) {
    if (err instanceof KeycodeError) return null;
    throw err;
  }
}

/** What an entry needs to know about the board to be checked. */
export interface AdvancedContext {
  layerCount: number;
  /** Matrix size; positions are checked against it, as the firmware does. */
  rows: number;
  cols: number;
}

function fail(message: string): never {
  throw new BoardError("invalid-value", message);
}

function inRange(v: number, limits: { min: number; max: number }, what: string): void {
  if (!Number.isInteger(v) || v < limits.min || v > limits.max) fail(`${what} must be ${limits.min}-${limits.max}`);
}

function action(id: KeycodeId, ctx: AdvancedContext, what: string, forbid: RegExp | null = null): void {
  if (!isBoardKeycode(id, ctx.layerCount) || id === "KC_TRNS") fail(`${what}: ${id} can't be used here`);
  if (forbid?.test(id)) fail(`${what}: ${id} can't be used here`);
}

/** Tap dances, layer-taps and mod-taps: keys that decide tap or hold themselves. */
const TAP_HOLD_KEYS = /^(TD|LT|[LR](CTL|SFT|ALT|GUI)_T)\(/;

function position(row: number, col: number, ctx: AdvancedContext): void {
  if (!Number.isInteger(row) || !Number.isInteger(col) || row < 0 || row >= ctx.rows || col < 0 || col >= ctx.cols) {
    fail(`matrix position (${row}, ${col}) out of range`);
  }
}

/** A modifier-wrapped key or mod-tap, e.g. LSFT(KC_1) or LCTL_T(KC_A). */
const MODDED = /^[LR](CTL|SFT|ALT|GUI)(_T)?\(KC_\w+\)$/;

/** A keycode the board takes: one from the catalogue, or a modded basic key. */
export function isBoardKeycode(id: KeycodeId, layerCount: number): boolean {
  if (isValidKeycode(id, layerCount)) return true;
  if (!MODDED.test(id)) return false;
  try {
    encodeKeycode(id);
    return true;
  } catch (err) {
    if (err instanceof KeycodeError) return false;
    throw err;
  }
}

/**
 * A keycode that just sends keys (a basic, modded, media or system key):
 * what macro steps and key-override replacements may use.
 */
export function sendsKey(id: KeycodeId, layerCount: number): boolean {
  if (/^[LR](CTL|SFT|ALT|GUI)\(/.test(id)) return isBoardKeycode(id, layerCount);
  const info = keycodeInfo(id, layerCount);
  return !!info && id !== "KC_NO" && ["basic", "modifiers", "navigation", "function", "media"].includes(info.category);
}

/** A key that types one HID usage (letters, digits, symbols, F-keys, navigation, modifiers). */
export function isBasicKey(id: KeycodeId): boolean {
  try {
    const n = encodeKeycode(id);
    return n >= 0x04 && n <= 0xe7;
  } catch (err) {
    if (err instanceof KeycodeError) return false;
    throw err;
  }
}

function layers(list: readonly number[], ctx: AdvancedContext): void {
  for (const l of list) {
    if (!Number.isInteger(l) || l < 0 || l >= ctx.layerCount) fail(`layer ${l} out of range`);
  }
}

/** Throws BoardError("invalid-value") for an entry the firmware would refuse. */
export function checkAdvancedEntry<T extends AdvancedTable>(
  table: T,
  value: AdvancedKeys[T][number],
  ctx: AdvancedContext,
): void {
  switch (table) {
    case "combos": {
      const c = value as AdvancedKeys["combos"][number];
      if (c.result === "KC_NO") return;
      if (c.keys.length < COMBO_KEYS.min || c.keys.length > COMBO_KEYS.max) fail(`a combo has ${COMBO_KEYS.min}-${COMBO_KEYS.max} keys`);
      const seen = new Set<string>();
      for (const k of c.keys) {
        position(k.row, k.col, ctx);
        if (seen.has(`${k.row},${k.col}`)) fail("a combo can't use a key twice");
        seen.add(`${k.row},${k.col}`);
      }
      action(c.result, ctx, "combo result", /^TD\(/);
      inRange(c.timeoutMs, ADVANCED_LIMITS.comboTimeoutMs, "combo timeout");
      layers(c.layers, ctx);
      return;
    }
    case "tapDances": {
      const t = value as AdvancedKeys["tapDances"][number];
      const acts: [KeycodeId, string][] = [[t.tap, "tap"], [t.hold, "hold"], [t.doubleTap, "double tap"], [t.tripleTap, "triple tap"]];
      for (const [id, what] of acts) if (id !== "KC_NO") action(id, ctx, what, TAP_HOLD_KEYS);
      inRange(t.termMs, ADVANCED_LIMITS.tapDanceTermMs, "tap dance term");
      return;
    }
    case "keyOverrides": {
      const o = value as AdvancedKeys["keyOverrides"][number];
      if (o.replacement === "KC_NO") return;
      if (!Number.isInteger(o.triggerMods) || o.triggerMods <= 0 || o.triggerMods > 0xff) fail("pick at least one modifier");
      if (!isBasicKey(o.triggerKey)) fail(`${o.triggerKey} can't trigger an override`);
      if (!sendsKey(o.replacement, ctx.layerCount)) fail(`an override sends a key, not ${o.replacement}`);
      layers(o.layers, ctx);
      return;
    }
    case "leader": {
      const s = value as AdvancedKeys["leader"][number];
      if (s.result === "KC_NO") return;
      if (s.keys.length < LEADER_KEYS.min || s.keys.length > LEADER_KEYS.max) fail(`a sequence has ${LEADER_KEYS.min}-${LEADER_KEYS.max} keys`);
      for (const k of s.keys) if (!isBasicKey(k)) fail(`${k} can't be part of a sequence`);
      action(s.result, ctx, "sequence result");
      return;
    }
    case "tapHold": {
      const r = value as AdvancedKeys["tapHold"][number];
      if (r === null) return;
      position(r.row, r.col, ctx);
      inRange(r.tappingTermMs, ADVANCED_LIMITS.tappingTermMs, "tapping term");
      inRange(r.quickTapMs, ADVANCED_LIMITS.quickTapMs, "quick tap");
      return;
    }
  }
}

export function checkAdvancedOptions(o: AdvancedOptions): void {
  const L = ADVANCED_LIMITS;
  inRange(o.leaderTimeoutMs, L.leaderTimeoutMs, "Leader timeout");
  inRange(o.capsWordIdleMs, L.capsWordIdleMs, "Caps Word timeout");
  inRange(o.autoShiftMs, L.autoShiftMs, "Auto Shift time");
  inRange(o.mouse.intervalMs, L.mouseIntervalMs, "mouse interval");
  inRange(o.mouse.maxSpeed, L.mouseSpeed, "mouse top speed");
  inRange(o.mouse.moveDelta, { min: 1, max: o.mouse.maxSpeed }, "mouse start speed");
  inRange(o.mouse.timeToMaxMs, L.mouseTimeToMaxMs, "mouse acceleration time");
  inRange(o.mouse.wheelIntervalMs, L.wheelIntervalMs, "scroll interval");
}

/** Throws BoardError for macros the firmware would refuse or can't fit. */
export function checkMacros(macros: readonly MacroStep[][], ctx: AdvancedContext): void {
  let bytes: Uint8Array;
  try {
    bytes = encodeMacros(macros);
  } catch (err) {
    if (err instanceof KeycodeError) fail(err.message);
    throw err;
  }
  if (bytes.length > MACRO_BUFFER_SIZE) {
    throw new BoardError("storage-full", `the macros take ${bytes.length} bytes; the board has room for ${MACRO_BUFFER_SIZE}`);
  }
  for (const steps of macros) {
    for (const step of steps) {
      if (step.type === "tap" || step.type === "down" || step.type === "up") {
        if (!sendsKey(step.keycode, ctx.layerCount)) fail(`a macro step can't press ${step.keycode}`);
      }
    }
  }
}
