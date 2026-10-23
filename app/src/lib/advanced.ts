/**
 * Helpers for the Advanced and Macros tabs: what each table is called,
 * one-line summaries, and new entries to start editing from. Which slots are
 * used, and what the board accepts, come from host-lib (advanced.ts).
 */

import {
  isMatrixPositionUsed,
  keycodeInfo,
  type AdvancedKeys,
  type AdvancedTable,
  type KeyboardDefinition,
  type KeycodeId,
  type Keymap,
  type MacroStep,
} from "@dachshund60f/host-lib";

export type Entry<T extends AdvancedTable = AdvancedTable> = AdvancedKeys[T][number];

export const TABLES: readonly { id: AdvancedTable; title: string; noun: string; blurb: string }[] = [
  {
    id: "combos",
    title: "Combos",
    noun: "combo",
    blurb: "Press keys together to send something else, like J + K for Esc.",
  },
  {
    id: "tapDances",
    title: "Tap dances",
    noun: "tap dance",
    blurb: "One key, up to four jobs: tap, hold, double tap and triple tap. Put TD(n) on a key to use one.",
  },
  {
    id: "keyOverrides",
    title: "Key overrides",
    noun: "override",
    blurb: "Change what a key sends with certain modifiers held, like Shift + Backspace for Delete.",
  },
  {
    id: "leader",
    title: "Leader sequences",
    noun: "sequence",
    blurb: "Tap the Leader key, then a short sequence, to send a key or shortcut.",
  },
  {
    id: "tapHold",
    title: "Tap-hold rules",
    noun: "rule",
    blurb: "Fine-tune one layer-tap or mod-tap key: its own tapping term, and how it decides.",
  },
];

/** Modifier bits of a key override's trigger (either side counts). */
export const MOD_BITS: readonly { bit: number; label: string }[] = [
  { bit: 0x01, label: "Ctrl" },
  { bit: 0x02, label: "Shift" },
  { bit: 0x04, label: "Alt" },
  { bit: 0x08, label: "GUI" },
];

/** A starting point for a new entry; pos is a key to begin with, if any. */
export function newEntry<T extends AdvancedTable>(table: T, pos: { row: number; col: number }): Entry<T> {
  switch (table) {
    case "tapDances":
      return { tap: "KC_ESC", hold: "KC_NO", doubleTap: "KC_CAPS", tripleTap: "KC_NO", termMs: 0 } as Entry<T>;
    case "keyOverrides":
      return { triggerMods: 0x02, triggerKey: "KC_BSPC", replacement: "KC_DEL", layers: [] } as Entry<T>;
    case "leader":
      return { keys: ["KC_E"], result: "KC_NO" } as Entry<T>;
    case "tapHold":
      return {
        row: pos.row,
        col: pos.col,
        permissiveHold: false,
        holdOnOtherKeyPress: false,
        retroTap: false,
        tappingTermMs: 200,
        quickTapMs: 0,
      } as Entry<T>;
    default:
      return { keys: [pos], result: "KC_NO", timeoutMs: 0, layers: [] } as Entry<T>;
  }
}

export interface KeyChoice {
  row: number;
  col: number;
  /** The base-layer legend and position, e.g. "A (2, 1)". */
  label: string;
}

function keyName(id: KeycodeId | undefined, layerCount: number): string {
  if (!id || id === "KC_NO") return "unassigned";
  return keycodeInfo(id, layerCount)?.label || id;
}

/** Every switch on the board, named by what it types on the base layer. */
export function keyChoices(definition: KeyboardDefinition, keymap: Keymap, layerCount: number): KeyChoice[] {
  const out: KeyChoice[] = [];
  for (let row = 0; row < definition.matrix.rows; row++) {
    for (let col = 0; col < definition.matrix.cols; col++) {
      if (!isMatrixPositionUsed(definition, row, col)) continue;
      out.push({ row, col, label: `${keyName(keymap.keys[0]?.[row]?.[col], layerCount)} (${row}, ${col})` });
    }
  }
  return out;
}

export function keycodeLabel(id: KeycodeId, layerCount: number): string {
  const info = keycodeInfo(id, layerCount);
  return info ? info.name : id;
}

function layerText(layers: readonly number[]): string {
  return layers.length === 0 ? "" : ` on layer${layers.length > 1 ? "s" : ""} ${layers.join(", ")}`;
}

function modsText(bits: number): string {
  return MOD_BITS.filter((m) => bits & m.bit)
    .map((m) => m.label)
    .join(" + ");
}

/** One line describing an entry, for the table lists. */
export function summary<T extends AdvancedTable>(
  table: T,
  entry: Entry<T>,
  ctx: { layerCount: number; keyLabel: (row: number, col: number) => string },
): string {
  const kc = (id: KeycodeId) => keycodeLabel(id, ctx.layerCount);
  switch (table) {
    case "combos": {
      const c = entry as Entry<"combos">;
      return `${c.keys.map((k) => ctx.keyLabel(k.row, k.col)).join(" + ")} → ${kc(c.result)}${layerText(c.layers)}`;
    }
    case "tapDances": {
      const t = entry as Entry<"tapDances">;
      const parts: [string, KeycodeId][] = [
        ["tap", t.tap],
        ["hold", t.hold],
        ["double", t.doubleTap],
        ["triple", t.tripleTap],
      ];
      return parts
        .filter(([, id]) => id !== "KC_NO")
        .map(([what, id]) => `${what}: ${kc(id)}`)
        .join(", ");
    }
    case "keyOverrides": {
      const o = entry as Entry<"keyOverrides">;
      return `${modsText(o.triggerMods)} + ${kc(o.triggerKey)} → ${kc(o.replacement)}${layerText(o.layers)}`;
    }
    case "leader": {
      const s = entry as Entry<"leader">;
      return `Leader, ${s.keys.map(kc).join(", ")} → ${kc(s.result)}`;
    }
    case "tapHold": {
      const r = entry as Entry<"tapHold">;
      if (!r) return "";
      const how = [
        r.permissiveHold && "permissive hold",
        r.holdOnOtherKeyPress && "hold on other key",
        r.retroTap && "retro tap",
        r.quickTapMs > 0 && `quick tap ${r.quickTapMs} ms`,
      ].filter(Boolean);
      return `${ctx.keyLabel(r.row, r.col)}: ${r.tappingTermMs} ms${how.length ? `, ${how.join(", ")}` : ""}`;
    }
  }
  return "";
}

export function newStep(type: MacroStep["type"]): MacroStep {
  switch (type) {
    case "text":
      return { type, text: "" };
    case "delay":
      return { type, ms: 100 };
    default:
      return { type, keycode: type === "tap" ? "KC_ENT" : "KC_LSFT" };
  }
}
