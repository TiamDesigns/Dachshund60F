/** The presets Doghouse offers: the built-in ones, plus the owner's own (kept in this browser or app). */

import { BUILT_IN_PRESETS, makePreset, parseCustomPresets, type Preset, type PresetValues } from "./presets.ts";

const KEY = "doghouse.presets";

class PresetStore {
  custom = $state<Preset[]>(load());

  get all(): Preset[] {
    return [...BUILT_IN_PRESETS, ...this.custom];
  }

  add(name: string, values: PresetValues): Preset {
    const preset = makePreset(name, values, this.all);
    this.custom = [...this.custom, preset];
    this.store();
    return preset;
  }

  remove(id: string): void {
    this.custom = this.custom.filter((p) => p.id !== id);
    this.store();
  }

  private store(): void {
    try {
      localStorage.setItem(KEY, JSON.stringify($state.snapshot(this.custom)));
    } catch {
      /* storage unavailable: they last until Doghouse closes */
    }
  }
}

function load(): Preset[] {
  try {
    return parseCustomPresets(localStorage.getItem(KEY));
  } catch {
    return [];
  }
}

export const presets = new PresetStore();
