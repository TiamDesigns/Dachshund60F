<script lang="ts">
  import { SETTINGS_LIMITS, type BoardSettings } from "@dachshund60f/host-lib";

  import type { BoardSession } from "../app-state.svelte.ts";

  let { session }: { session: BoardSession } = $props();

  const L = SETTINGS_LIMITS;
  const SLEEP_CHOICES = [
    { value: 0, label: "Never" },
    { value: 5, label: "5 minutes" },
    { value: 10, label: "10 minutes" },
    { value: 15, label: "15 minutes" },
    { value: 30, label: "30 minutes" },
    { value: 60, label: "1 hour" },
    { value: 120, label: "2 hours" },
  ];

  // Writable deriveds: sliders move smoothly while dragged, commit on change,
  // and snap back to the board's value when it changes (revert, factory reset).
  const s = $derived(session.settings!);
  let name = $derived(s.name);
  let debounce = $derived(s.debounceMs);
  let tapping = $derived(s.tappingTermMs);
  let brightness = $derived(s.indicatorBrightness);

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
  <section class="group">
    <h2>Name tag</h2>
    <label class="row">
      <span class="label">
        Board name
        <small>How Doghouse lists this board in your pack. Up to {L.nameMaxLength} characters.</small>
      </span>
      <input
        class="text"
        type="text"
        maxlength={L.nameMaxLength}
        placeholder={session.board.info.model}
        value={name}
        oninput={(e) => (name = e.currentTarget.value)}
        onchange={() => commit({ name: name.trim() })}
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
          max={L.debounceMs.max}
          value={debounce}
          oninput={(e) => (debounce = Number(e.currentTarget.value))}
          onchange={() => commit({ debounceMs: debounce })}
        />
        <output>{debounce} ms</output>
      </span>
    </label>
    <label class="row">
      <span class="label">
        Tapping term
        <small>Hold longer than this and a dual-role key acts as its hold action.</small>
      </span>
      <span class="control">
        <input
          type="range"
          min={L.tappingTermMs.min}
          max={L.tappingTermMs.max}
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
        {#each SLEEP_CHOICES as c (c.value)}
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

  <p class="note">Changes take effect right away. Use <strong>Save to board</strong> to keep them after the board powers off.</p>

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
