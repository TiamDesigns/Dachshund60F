import { test } from "node:test";
import assert from "node:assert/strict";

import type { AdvancedOptions, BoardSettings } from "@dachshund60f/host-lib";

import { activePreset, BUILT_IN_PRESETS, currentValues, makePreset, matches, parseCustomPresets } from "./presets.ts";

const factory: BoardSettings = {
  name: "Fritz",
  debounceAlgorithm: "eager-press",
  debounceMs: 5,
  tappingTermMs: 200,
  nkro: true,
  sleepTimeoutMin: 15,
  indicatorBrightness: 40,
};

const options: AdvancedOptions = {
  leaderTimeoutMs: 300,
  capsWordOnBothShifts: false,
  capsWordIdleMs: 5000,
  autoShift: false,
  autoShiftMs: 175,
  mouse: { intervalMs: 16, moveDelta: 4, maxSpeed: 20, timeToMaxMs: 1000, wheelIntervalMs: 80 },
  off: { tapHold: false, tapDance: false, combos: false },
};

const preset = (id: string) => BUILT_IN_PRESETS.find((p) => p.id === id)!;

test("a factory-fresh board is on Everyday", () => {
  assert.equal(activePreset(BUILT_IN_PRESETS, factory, options)?.id, "everyday");
});

test("Gaming switches off every feature that waits, and Auto Shift", () => {
  const gaming = preset("gaming");
  assert.deepEqual(gaming.off, { tapHold: true, tapDance: true, combos: true });
  assert.equal(gaming.autoShift, false);
  assert.ok(gaming.settings.debounceAlgorithm.startsWith("eager"));
  assert.equal(gaming.settings.nkro, true);

  const applied = { ...factory, ...gaming.settings };
  assert.equal(activePreset(BUILT_IN_PRESETS, applied, { ...options, off: gaming.off })?.id, "gaming");
  // Tap-hold back on: no longer Gaming, but the board's own mix.
  assert.equal(activePreset(BUILT_IN_PRESETS, applied, { ...options, off: { ...gaming.off, tapHold: false } }), null);
});

test("presets ignore the board's name and what it can't do", () => {
  assert.ok(matches(preset("everyday"), { ...factory, name: "Other" }, options));
  // No advanced keys: settings alone decide.
  assert.ok(matches(preset("gaming"), { ...factory, ...preset("gaming").settings }, null));
  // Can't switch features off: those switches don't count.
  assert.ok(matches(preset("gaming"), { ...factory, ...preset("gaming").settings }, options, false));
  assert.ok(!matches(preset("gaming"), { ...factory, ...preset("gaming").settings }, options, true));
});

test("built-in presets differ from each other", () => {
  for (const a of BUILT_IN_PRESETS) {
    for (const b of BUILT_IN_PRESETS) {
      if (a !== b) assert.ok(!matches(a, { ...factory, ...b.settings }, { ...options, autoShift: b.autoShift, off: b.off }, false), `${a.id} vs ${b.id}`);
    }
  }
});

test("a custom preset snapshots the board and gets a unique id", () => {
  const values = currentValues({ ...factory, tappingTermMs: 180 }, { ...options, autoShift: true });
  const first = makePreset("  My setup ", values, BUILT_IN_PRESETS);
  assert.equal(first.name, "My setup");
  assert.equal(first.id, "custom-my-setup");
  assert.equal(first.settings.tappingTermMs, 180);
  assert.equal(first.autoShift, true);
  assert.equal("name" in first.settings, false);
  assert.match(first.blurb, /180 ms tapping term/);
  assert.equal(makePreset("My setup", values, [first]).id, "custom-my-setup-2");
});

test("the preset last picked wins over an identical one", () => {
  const gaming = preset("gaming");
  const mine = makePreset("Mine", { settings: gaming.settings, autoShift: false, off: gaming.off }, BUILT_IN_PRESETS);
  const all = [...BUILT_IN_PRESETS, mine];
  const applied = { ...factory, ...gaming.settings };
  const opts = { ...options, off: gaming.off };
  assert.equal(activePreset(all, applied, opts)?.id, "gaming");
  assert.equal(activePreset(all, applied, opts, true, mine.id)?.id, mine.id);
  // Picked, then changed: falls back to whatever matches now.
  assert.equal(activePreset(all, factory, options, true, mine.id)?.id, "everyday");
});

test("stored presets round-trip, and malformed ones are dropped", () => {
  const p = makePreset("Mine", currentValues(factory, options), []);
  assert.deepEqual(parseCustomPresets(JSON.stringify([p])), [p]);
  assert.deepEqual(parseCustomPresets(null), []);
  assert.deepEqual(parseCustomPresets("not json"), []);
  assert.deepEqual(parseCustomPresets(JSON.stringify({ id: "x" })), []);
  const bad = [
    { ...p, settings: { ...p.settings, debounceMs: 500 } },
    { ...p, name: " " },
    { ...p, off: { tapHold: "yes" } },
    { ...p, id: "gaming" }, // can't shadow a built-in
  ];
  assert.deepEqual(parseCustomPresets(JSON.stringify([...bad, p, p])), [p]);
});
