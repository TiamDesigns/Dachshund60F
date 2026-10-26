/**
 * Board presets: named sets of settings the owner switches between, like
 * Everyday and Gaming. A preset holds every setting that changes how the
 * board feels (not its name or colours), plus Auto Shift and which
 * latency-costing features are switched off, so applying one always lands
 * in the same place. Doghouse keeps them, not the board, so they work for
 * every board in the pack.
 */

import type { AdvancedOptions, BoardSettings, DebounceAlgorithm } from "@dachshund60f/host-lib";

export type PresetSettings = Omit<BoardSettings, "name">;
export type FeaturesOff = AdvancedOptions["off"];

export interface PresetValues {
  settings: PresetSettings;
  autoShift: boolean;
  off: FeaturesOff;
}

export interface Preset extends PresetValues {
  id: string;
  name: string;
  /** One line on what it's for. */
  blurb: string;
  builtIn: boolean;
}

const ALL_ON: FeaturesOff = { tapHold: false, tapDance: false, combos: false };

/** The board's factory settings (host-lib's DEFAULT_SETTINGS). */
const FACTORY: PresetSettings = {
  debounceAlgorithm: "eager-press",
  debounceMs: 5,
  tappingTermMs: 200,
  nkro: true,
  sleepTimeoutMin: 15,
  indicatorBrightness: 40,
};

export const BUILT_IN_PRESETS: readonly Preset[] = [
  {
    id: "everyday",
    name: "Everyday",
    blurb: "The factory settings, with every trick switched on.",
    builtIn: true,
    settings: { ...FACTORY },
    autoShift: false,
    off: { ...ALL_ON },
  },
  {
    id: "gaming",
    name: "Gaming",
    blurb: "Lowest latency: every key acts the moment it goes down. No tap-hold, tap dance, combos or Auto Shift.",
    builtIn: true,
    settings: { ...FACTORY, debounceAlgorithm: "eager-press", debounceMs: 5, nkro: true, sleepTimeoutMin: 30 },
    autoShift: false,
    off: { tapHold: true, tapDance: true, combos: true },
  },
  {
    id: "typing",
    name: "Careful typing",
    blurb: "Fewer accidental holds for home-row mods, and switches that must settle before a press counts.",
    builtIn: true,
    settings: { ...FACTORY, debounceAlgorithm: "defer-per-key", debounceMs: 5, tappingTermMs: 230 },
    autoShift: false,
    off: { ...ALL_ON },
  },
  {
    id: "battery",
    name: "Battery saver",
    blurb: "Dim lights and a quick nap, for a long day away from the charger.",
    builtIn: true,
    settings: { ...FACTORY, indicatorBrightness: 10, sleepTimeoutMin: 5 },
    autoShift: false,
    off: { ...ALL_ON },
  },
];

const DEBOUNCE: readonly DebounceAlgorithm[] = ["defer-global", "defer-per-key", "eager-global", "eager-per-key", "eager-press"];

/** The board's values as a preset holds them; options is null for boards without advanced keys. */
export function currentValues(settings: BoardSettings, options: AdvancedOptions | null): PresetValues {
  const { name: _name, ...rest } = settings;
  return {
    settings: rest,
    autoShift: options?.autoShift ?? false,
    off: options ? { ...options.off } : { ...ALL_ON },
  };
}

/**
 * Whether the board is set as the preset says. Boards without advanced keys
 * compare on settings only; boards that can't switch features off ignore
 * those switches.
 */
export function matches(
  preset: PresetValues,
  settings: BoardSettings,
  options: AdvancedOptions | null,
  canSwitchOff = true,
): boolean {
  const now = currentValues(settings, options);
  const keys = Object.keys(preset.settings) as (keyof PresetSettings)[];
  if (keys.some((k) => preset.settings[k] !== now.settings[k])) return false;
  if (!options) return true;
  if (preset.autoShift !== now.autoShift) return false;
  if (!canSwitchOff) return true;
  return preset.off.tapHold === now.off.tapHold && preset.off.tapDance === now.off.tapDance && preset.off.combos === now.off.combos;
}

/**
 * The preset the board is set as: the one last picked (preferredId) while
 * the board still matches it, else the first that matches, or null for its
 * own mix.
 */
export function activePreset(
  presets: readonly Preset[],
  settings: BoardSettings,
  options: AdvancedOptions | null,
  canSwitchOff = true,
  preferredId: string | null = null,
): Preset | null {
  const fits = (p: Preset) => matches(p, settings, options, canSwitchOff);
  const preferred = presets.find((p) => p.id === preferredId);
  if (preferred && fits(preferred)) return preferred;
  return presets.find(fits) ?? null;
}

/** A new custom preset from the board's current values. */
export function makePreset(name: string, values: PresetValues, existing: readonly Preset[]): Preset {
  let id = `custom-${slug(name) || "preset"}`;
  for (let n = 2; existing.some((p) => p.id === id); n++) id = `custom-${slug(name) || "preset"}-${n}`;
  return {
    id,
    name: name.trim(),
    blurb: describe(values),
    builtIn: false,
    settings: { ...values.settings },
    autoShift: values.autoShift,
    off: { ...values.off },
  };
}

function slug(name: string): string {
  return name
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-|-$/g, "");
}

/** A short summary of a preset's values, for custom presets. */
export function describe(v: PresetValues): string {
  const off = [v.off.tapHold && "tap-hold", v.off.tapDance && "tap dance", v.off.combos && "combos"].filter(Boolean);
  const parts = [
    `${v.settings.debounceMs} ms ${v.settings.debounceAlgorithm.startsWith("eager") ? "eager" : "settled"} debounce`,
    `${v.settings.tappingTermMs} ms tapping term`,
    v.settings.nkro ? "NKRO" : "6KRO",
    v.settings.sleepTimeoutMin ? `naps after ${v.settings.sleepTimeoutMin} min` : "never naps",
    `lights ${v.settings.indicatorBrightness}%`,
  ];
  if (v.autoShift) parts.push("Auto Shift");
  if (off.length) parts.push(`no ${off.join(", ")}`);
  return parts.join(", ") + ".";
}

/** Custom presets read back from storage; anything malformed is dropped. */
export function parseCustomPresets(text: string | null): Preset[] {
  if (!text) return [];
  let data: unknown;
  try {
    data = JSON.parse(text);
  } catch {
    return [];
  }
  if (!Array.isArray(data)) return [];
  const out: Preset[] = [];
  for (const item of data) {
    const p = validPreset(item);
    if (p && !out.some((q) => q.id === p.id) && !BUILT_IN_PRESETS.some((b) => b.id === p.id)) out.push(p);
  }
  return out;
}

function validPreset(x: unknown): Preset | null {
  if (!x || typeof x !== "object") return null;
  const p = x as Record<string, unknown>;
  const s = p.settings as Record<string, unknown> | undefined;
  const off = p.off as Record<string, unknown> | undefined;
  const int = (v: unknown, min: number, max: number) => typeof v === "number" && Number.isInteger(v) && v >= min && v <= max;
  if (
    typeof p.id !== "string" ||
    typeof p.name !== "string" ||
    !p.name.trim() ||
    !s ||
    !DEBOUNCE.includes(s.debounceAlgorithm as DebounceAlgorithm) ||
    !int(s.debounceMs, 0, 100) ||
    !int(s.tappingTermMs, 50, 2000) ||
    typeof s.nkro !== "boolean" ||
    !int(s.sleepTimeoutMin, 0, 240) ||
    !int(s.indicatorBrightness, 0, 100) ||
    typeof p.autoShift !== "boolean" ||
    !off ||
    typeof off.tapHold !== "boolean" ||
    typeof off.tapDance !== "boolean" ||
    typeof off.combos !== "boolean"
  ) {
    return null;
  }
  const values: PresetValues = {
    settings: {
      debounceAlgorithm: s.debounceAlgorithm as DebounceAlgorithm,
      debounceMs: s.debounceMs as number,
      tappingTermMs: s.tappingTermMs as number,
      nkro: s.nkro,
      sleepTimeoutMin: s.sleepTimeoutMin as number,
      indicatorBrightness: s.indicatorBrightness as number,
    },
    autoShift: p.autoShift,
    off: { tapHold: off.tapHold, tapDance: off.tapDance, combos: off.combos },
  };
  return {
    id: p.id,
    name: p.name.trim(),
    blurb: typeof p.blurb === "string" ? p.blurb : describe(values),
    builtIn: false,
    ...values,
  };
}
