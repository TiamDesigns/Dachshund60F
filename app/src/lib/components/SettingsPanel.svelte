<script lang="ts">
  import {
    DEBOUNCE_ALGORITHMS,
    DEFAULT_SETTINGS,
    nameBytes,
    SETTINGS_LIMITS,
    type BoardSettings,
    type DebounceAlgorithm,
    type IndicatorColours,
  } from "@dachshund60f/host-lib";

  const DEFAULT_VIEW: BoardSettings = { ...DEFAULT_SETTINGS };

  import type { BoardSession } from "../app-state.svelte.ts";

  let { session }: { session: BoardSession } = $props();

  const L = SETTINGS_LIMITS;
  // The board takes 0-100 ms and 50-2000 ms; the sliders cover the useful part (and any value already set).
  const DEBOUNCE_SLIDER_MAX = 30;
  const TAPPING_SLIDER = { min: 100, max: 1000 };

  const DEBOUNCE_LABELS: Record<DebounceAlgorithm, string> = {
    "eager-press": "Eager on press",
    "eager-per-key": "Eager, per key",
    "eager-global": "Eager, whole board",
    "defer-per-key": "Wait to settle, per key",
    "defer-global": "Wait to settle, whole board",
  };

  const SLEEP_CHOICES = [
    { value: 0, label: "Never" },
    { value: 5, label: "5 minutes" },
    { value: 10, label: "10 minutes" },
    { value: 15, label: "15 minutes" },
    { value: 30, label: "30 minutes" },
    { value: 60, label: "1 hour" },
    { value: 120, label: "2 hours" },
    { value: 240, label: "4 hours" },
  ];

  // Writable deriveds: sliders move smoothly while dragged, commit on change,
  // and snap back to the board's value when it changes (revert, factory reset).
  // Firmware without the settings capability has none to show.
  const s = $derived(session.settings ?? DEFAULT_VIEW);
  let name = $derived(s.name);
  let debounce = $derived(s.debounceMs);
  let tapping = $derived(s.tappingTermMs);
  let brightness = $derived(s.indicatorBrightness);
  const sleepChoices = $derived(
    SLEEP_CHOICES.some((c) => c.value === s.sleepTimeoutMin)
      ? SLEEP_CHOICES
      : [...SLEEP_CHOICES, { value: s.sleepTimeoutMin, label: `${s.sleepTimeoutMin} minutes` }].sort((a, b) => a.value - b.value),
  );
  const nameTooLong = $derived(nameBytes(name.trim()) > L.nameMaxLength);

  // Which indicator colours this board can show, from its keyboard definition (all of them if unknown).
  const lights = $derived.by(() => {
    const leds = session.definition?.indicators;
    const lock = (word: string) => !leds || leds.some((i) => i.role === "lock" && i.label.includes(word));
    return {
      layer: !leds || leds.some((i) => i.role === "layer"),
      caps: lock("Caps"),
      num: lock("Num"),
      scroll: lock("Scroll"),
      battery: !leds || leds.some((i) => i.role === "battery"),
    };
  });
  const layerNames = $derived(
    Array.from({ length: session.board.info.layerCount }, (_, i) => (i === 0 ? "Base" : i === 1 ? "Fn" : `Layer ${i}`)),
  );
  let lowPercent = $derived(session.indicators?.batteryLowPercent ?? 15);

  function setColour(key: "caps" | "num" | "scroll" | "battery" | "batteryLow", value: string) {
    void session.updateIndicators({ [key]: value } as Partial<IndicatorColours>);
  }

  function setLayerColour(layer: number, value: string) {
    if (!session.indicators) return;
    const layers = [...session.indicators.layers];
    layers[layer] = value;
    void session.updateIndicators({ layers });
  }

  function commit(patch: Partial<BoardSettings>) {
    void session.updateSettings(patch);
  }

  function resetToFactory() {
    if (confirm(`Reset ${session.displayName} to factory settings? This replaces its keymap and settings.`)) {
      void session.factoryReset();
    }
  }
</script>

<div class="settings">
  {#if session.settings}
  <section class="group">
    <h2>Name tag</h2>
    <label class="row">
      <span class="label">
        Board name
        <small class:error={nameTooLong}>
          How Doghouse lists this board in your pack. Up to {L.nameMaxLength} letters; accents and emoji take up more than one.
        </small>
      </span>
      <input
        class="text"
        type="text"
        maxlength={L.nameMaxLength}
        placeholder={session.board.info.model}
        aria-invalid={nameTooLong}
        value={name}
        oninput={(e) => (name = e.currentTarget.value)}
        onchange={() => {
          if (!nameTooLong) commit({ name: name.trim() });
        }}
      />
    </label>
  </section>

  <section class="group">
    <h2>Typing feel</h2>
    <label class="row">
      <span class="label">
        Debounce
        <small>How long a switch must settle before a press counts. Raise it if keys double-type.</small>
      </span>
      <span class="control">
        <input
          type="range"
          min={L.debounceMs.min}
          max={Math.max(DEBOUNCE_SLIDER_MAX, s.debounceMs)}
          value={debounce}
          oninput={(e) => (debounce = Number(e.currentTarget.value))}
          onchange={() => commit({ debounceMs: debounce })}
        />
        <output>{debounce} ms</output>
      </span>
    </label>
    <label class="row">
      <span class="label">
        Debounce style
        <small>Eager sends a press the moment a switch closes; waiting to settle ignores noise first, but adds the debounce time.</small>
      </span>
      <select value={s.debounceAlgorithm} onchange={(e) => commit({ debounceAlgorithm: e.currentTarget.value as DebounceAlgorithm })}>
        {#each DEBOUNCE_ALGORITHMS as a (a)}
          <option value={a}>{DEBOUNCE_LABELS[a]}</option>
        {/each}
      </select>
    </label>
    <label class="row">
      <span class="label">
        Tapping term
        <small>Hold longer than this and a dual-role key acts as its hold action.</small>
      </span>
      <span class="control">
        <input
          type="range"
          min={Math.min(TAPPING_SLIDER.min, s.tappingTermMs)}
          max={Math.max(TAPPING_SLIDER.max, s.tappingTermMs)}
          step="10"
          value={tapping}
          oninput={(e) => (tapping = Number(e.currentTarget.value))}
          onchange={() => commit({ tappingTermMs: tapping })}
        />
        <output>{tapping} ms</output>
      </span>
    </label>
    <label class="row">
      <span class="label">
        N-key rollover
        <small>Report every key held at once. Turn off for old BIOSes and KVM switches.</small>
      </span>
      <input class="switch" type="checkbox" checked={s.nkro} onchange={(e) => commit({ nkro: e.currentTarget.checked })} />
    </label>
  </section>

  <section class="group">
    <h2>Naps and lights</h2>
    <label class="row">
      <span class="label">
        Nap after
        <small>Idle time before the board sleeps on battery. Any key wakes it.</small>
      </span>
      <select value={s.sleepTimeoutMin} onchange={(e) => commit({ sleepTimeoutMin: Number(e.currentTarget.value) })}>
        {#each sleepChoices as c (c.value)}
          <option value={c.value}>{c.label}</option>
        {/each}
      </select>
    </label>
    <label class="row">
      <span class="label">
        Indicator brightness
        <small>Layer, battery and Caps Lock lights. Lower is kinder to the battery.</small>
      </span>
      <span class="control">
        <input
          type="range"
          min={L.indicatorBrightness.min}
          max={L.indicatorBrightness.max}
          step="5"
          value={brightness}
          oninput={(e) => (brightness = Number(e.currentTarget.value))}
          onchange={() => commit({ indicatorBrightness: brightness })}
        />
        <output>{brightness}%</output>
      </span>
    </label>
  </section>

  {#if session.indicators}
    {@const c = session.indicators}
    <section class="group">
      <h2>Light colours</h2>
      {#if lights.layer}
        <div class="row">
          <span class="label">
            Layer light
            <small>Its colour on each layer. Black keeps it off, as it is on Base by default.</small>
          </span>
          <span class="swatches">
            {#each layerNames as layerName, i (i)}
              <label class="swatch">
                <input type="color" value={c.layers[i]} onchange={(e) => setLayerColour(i, e.currentTarget.value)} />
                <span>{layerName}</span>
              </label>
            {/each}
          </span>
        </div>
      {/if}
      {#if lights.caps || lights.num || lights.scroll}
        <div class="row">
          <span class="label">
            Lock lights
            <small>Shown while the computer has the lock on.</small>
          </span>
          <span class="swatches">
            {#if lights.caps}
              <label class="swatch"><input type="color" value={c.caps} onchange={(e) => setColour("caps", e.currentTarget.value)} /><span>Caps Lock</span></label>
            {/if}
            {#if lights.num}
              <label class="swatch"><input type="color" value={c.num} onchange={(e) => setColour("num", e.currentTarget.value)} /><span>Num Lock</span></label>
            {/if}
            {#if lights.scroll}
              <label class="swatch"><input type="color" value={c.scroll} onchange={(e) => setColour("scroll", e.currentTarget.value)} /><span>Scroll Lock</span></label>
            {/if}
          </span>
        </div>
      {/if}
      {#if lights.battery}
        <div class="row">
          <span class="label">
            Battery gauge
            <small>Its usual colour, and the colour of the whole gauge once the battery runs low.</small>
          </span>
          <span class="swatches">
            <label class="swatch"><input type="color" value={c.battery} onchange={(e) => setColour("battery", e.currentTarget.value)} /><span>Charged</span></label>
            <label class="swatch"><input type="color" value={c.batteryLow} onchange={(e) => setColour("batteryLow", e.currentTarget.value)} /><span>Low</span></label>
          </span>
        </div>
        <label class="row">
          <span class="label">
            Low battery at
            <small>The gauge turns to its low colour at or below this level.</small>
          </span>
          <span class="control">
            <input
              type="range"
              min="0"
              max={Math.max(50, c.batteryLowPercent)}
              step="5"
              value={lowPercent}
              oninput={(e) => (lowPercent = Number(e.currentTarget.value))}
              onchange={() => void session.updateIndicators({ batteryLowPercent: lowPercent })}
            />
            <output>{lowPercent}%</output>
          </span>
        </label>
      {/if}
    </section>
  {/if}

  <p class="note">Changes take effect right away. Use <strong>Save to board</strong> to keep them after the board powers off.</p>
  {:else}
  <p class="note">This board's firmware can't change its settings from Doghouse yet. Its keymap can still be edited and saved.</p>
  {/if}

  {#if session.supports("persistence")}
  <section class="group danger">
    <h2>Fresh start</h2>
    <div class="row">
      <span class="label">
        Factory reset
        <small>Puts back the default keymap and settings, and saves them.</small>
      </span>
      <button class="btn" onclick={resetToFactory}>Reset…</button>
    </div>
  </section>
  {/if}
</div>

<style>
  .settings {
    display: flex;
    flex-direction: column;
    gap: 18px;
    max-width: 760px;
  }

  .group {
    padding: 18px 20px;
    border-radius: var(--radius-lg);
    background: var(--surface);
    border: 1px solid var(--border);
  }

  h2 {
    font-size: 15px;
    margin-bottom: 6px;
  }

  .row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 24px;
    padding: 12px 0;
    border-top: 1px solid var(--surface-3);
  }

  .row:first-of-type {
    border-top: 0;
  }

  .label {
    display: flex;
    flex-direction: column;
    font-weight: 600;
  }

  small {
    font-weight: 400;
    color: var(--text-muted);
  }

  .control {
    display: flex;
    align-items: center;
    gap: 10px;
    flex: none;
  }

  output {
    min-width: 6ch;
    text-align: right;
    font-variant-numeric: tabular-nums;
    font-weight: 600;
  }

  input[type="range"] {
    width: 200px;
    accent-color: var(--accent);
  }

  .text,
  select {
    font: inherit;
    padding: 7px 10px;
    border-radius: var(--radius-sm);
    border: 1px solid var(--border-strong);
    background: var(--bg);
    color: var(--text);
    min-width: 200px;
  }

  .switch {
    appearance: none;
    flex: none;
    width: 42px;
    height: 24px;
    border-radius: 999px;
    background: var(--surface-3);
    position: relative;
    cursor: pointer;
    transition: background 150ms;
  }

  .switch::after {
    content: "";
    position: absolute;
    top: 3px;
    left: 3px;
    width: 18px;
    height: 18px;
    border-radius: 50%;
    background: var(--surface);
    box-shadow: var(--shadow-sm);
    transition: transform 150ms;
  }

  .switch:checked {
    background: var(--sage);
  }

  .switch:checked::after {
    transform: translateX(18px);
  }

  .note {
    margin: 0;
    color: var(--text-muted);
  }

  small.error {
    color: var(--danger);
  }

  .swatches {
    display: flex;
    flex-wrap: wrap;
    justify-content: flex-end;
    gap: 12px;
  }

  .swatch {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
    font-size: 12px;
    color: var(--text-muted);
    cursor: pointer;
  }

  .swatch input {
    width: 40px;
    height: 32px;
    padding: 2px;
    border-radius: var(--radius-sm);
    border: 1px solid var(--border-strong);
    background: var(--bg);
    cursor: pointer;
  }

  .danger {
    border-color: var(--danger-soft);
  }

  @media (max-width: 640px) {
    .row {
      flex-direction: column;
      align-items: stretch;
    }
  }
</style>
