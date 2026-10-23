<script lang="ts">
  import {
    ADVANCED_LIMITS,
    ADVANCED_SLOTS,
    COMBO_KEYS,
    LEADER_KEYS,
    checkAdvancedEntry,
    checkAdvancedOptions,
    emptyAdvancedEntry,
    firstFreeAdvancedSlot,
    isAdvancedEntryUsed,
    type AdvancedKeys,
    type AdvancedOptions,
    type AdvancedTable,
    type KeyboardDefinition,
  } from "@dachshund60f/host-lib";

  import { keyChoices, MOD_BITS, newEntry, summary, TABLES } from "../advanced.ts";
  import type { BoardSession } from "../app-state.svelte.ts";
  import KeycodeField from "./KeycodeField.svelte";

  let { session }: { session: BoardSession } = $props();

  type Editing = { [T in AdvancedTable]: { table: T; index: number; draft: AdvancedKeys[T][number] } }[AdvancedTable];

  const adv = $derived(session.advanced!);
  const definition = $derived(session.definition as KeyboardDefinition);
  const layerCount = $derived(session.board.info.layerCount);
  const layers = $derived(Array.from({ length: layerCount }, (_, l) => l));
  const choices = $derived(keyChoices(definition, session.keymap!, layerCount));
  const ctx = $derived({ layerCount, rows: definition.matrix.rows, cols: definition.matrix.cols });
  const L = ADVANCED_LIMITS;

  function keyLabel(row: number, col: number): string {
    return choices.find((c) => c.row === row && c.col === col)?.label ?? `(${row}, ${col})`;
  }

  let editing = $state<Editing | null>(null);

  const problem = $derived.by(() => {
    if (!editing) return null;
    const { table, draft } = $state.snapshot(editing) as Editing;
    if (!isAdvancedEntryUsed(table, draft)) {
      return table === "keyOverrides" ? "Pick what the key sends instead." : "Pick what it sends.";
    }
    try {
      checkAdvancedEntry(table, draft, ctx);
      return null;
    } catch (err) {
      return err instanceof Error ? err.message : String(err);
    }
  });

  function used(table: AdvancedTable): number[] {
    const list = adv[table] as AdvancedKeys[AdvancedTable][number][];
    return list.flatMap((e, i) => (isAdvancedEntryUsed(table, e) ? [i] : []));
  }

  function startNew(table: AdvancedTable) {
    const index = firstFreeAdvancedSlot(table, adv[table] as AdvancedKeys[AdvancedTable][number][]);
    if (index < 0) return;
    const first = choices[0] ?? { row: 0, col: 0 };
    editing = { table, index, draft: newEntry(table, first) } as Editing;
  }

  function edit(table: AdvancedTable, index: number) {
    editing = { table, index, draft: $state.snapshot(adv[table][index]) } as Editing;
  }

  async function apply() {
    if (!editing || problem) return;
    const { table, index, draft } = $state.snapshot(editing) as Editing;
    if (await session.setAdvancedEntry(table, index, draft)) editing = null;
  }

  async function remove(table: AdvancedTable, index: number) {
    if (await session.setAdvancedEntry(table, index, emptyAdvancedEntry(table))) {
      if (editing?.table === table && editing.index === index) editing = null;
    }
  }

  function toggleLayer(list: number[], l: number): number[] {
    return list.includes(l) ? list.filter((x) => x !== l) : [...list, l].sort((a, b) => a - b);
  }

  // Options: committed one field at a time, checked here first so a bad number shows inline.
  let optionError = $state<string | null>(null);

  function setOption(patch: Partial<AdvancedOptions>) {
    const current = $state.snapshot(adv.options) as AdvancedOptions;
    const next = { ...current, ...patch, mouse: { ...current.mouse, ...patch.mouse } };
    try {
      checkAdvancedOptions(next);
      optionError = null;
    } catch (err) {
      optionError = err instanceof Error ? err.message : String(err);
      return;
    }
    void session.updateAdvancedOptions(patch);
  }

  function num(e: Event): number {
    return Number((e.currentTarget as HTMLInputElement).value);
  }
</script>

{#snippet layerPicker(selected: number[], onpick: (next: number[]) => void)}
  <fieldset class="layers">
    <legend>Layers <small>{selected.length ? "" : "(all)"}</small></legend>
    {#each layers as l (l)}
      <label class="chip" style="--chip: var(--layer-{l % 4})">
        <input type="checkbox" checked={selected.includes(l)} onchange={() => onpick(toggleLayer(selected, l))} />
        {l}
      </label>
    {/each}
  </fieldset>
{/snippet}

{#snippet keySelect(value: { row: number; col: number }, label: string, onpick: (pos: { row: number; col: number }) => void)}
  <label class="field">
    <span class="flabel">{label}</span>
    <select
      value={`${value.row},${value.col}`}
      onchange={(e) => {
        const [row, col] = e.currentTarget.value.split(",").map(Number);
        onpick({ row: row!, col: col! });
      }}
    >
      {#each choices as c (`${c.row},${c.col}`)}
        <option value={`${c.row},${c.col}`}>{c.label}</option>
      {/each}
    </select>
  </label>
{/snippet}

<div class="advanced">
  <section class="group">
    <h2>Tricks and timing</h2>
    <div class="options">
      <label class="opt">
        <span>Leader timeout <small>per key of a sequence</small></span>
        <span class="num">
          <input type="number" min={L.leaderTimeoutMs.min} max={L.leaderTimeoutMs.max} step="10"
            value={adv.options.leaderTimeoutMs} onchange={(e) => setOption({ leaderTimeoutMs: num(e) })} /> ms
        </span>
      </label>
      <label class="opt">
        <span>Caps Word on both Shifts <small>tap both Shifts to type one word in capitals</small></span>
        <input class="switch" type="checkbox" checked={adv.options.capsWordOnBothShifts}
          onchange={(e) => setOption({ capsWordOnBothShifts: e.currentTarget.checked })} />
      </label>
      <label class="opt">
        <span>Caps Word idle timeout <small>0 = never</small></span>
        <span class="num">
          <input type="number" min={L.capsWordIdleMs.min} max={L.capsWordIdleMs.max} step="500"
            value={adv.options.capsWordIdleMs} onchange={(e) => setOption({ capsWordIdleMs: num(e) })} /> ms
        </span>
      </label>
      <label class="opt">
        <span>Auto Shift <small>hold a key a little longer to get it shifted (AS_TOGG flips it)</small></span>
        <input class="switch" type="checkbox" checked={adv.options.autoShift}
          onchange={(e) => setOption({ autoShift: e.currentTarget.checked })} />
      </label>
      <label class="opt">
        <span>Auto Shift hold time</span>
        <span class="num">
          <input type="number" min={L.autoShiftMs.min} max={L.autoShiftMs.max} step="5"
            value={adv.options.autoShiftMs} onchange={(e) => setOption({ autoShiftMs: num(e) })} /> ms
        </span>
      </label>
      <label class="opt">
        <span>Mouse start speed <small>pixels per step</small></span>
        <span class="num">
          <input type="number" min="1" max={adv.options.mouse.maxSpeed}
            value={adv.options.mouse.moveDelta} onchange={(e) => setOption({ mouse: { ...adv.options.mouse, moveDelta: num(e) } })} />
        </span>
      </label>
      <label class="opt">
        <span>Mouse top speed</span>
        <span class="num">
          <input type="number" min={L.mouseSpeed.min} max={L.mouseSpeed.max}
            value={adv.options.mouse.maxSpeed} onchange={(e) => setOption({ mouse: { ...adv.options.mouse, maxSpeed: num(e) } })} />
        </span>
      </label>
      <label class="opt">
        <span>Mouse time to top speed</span>
        <span class="num">
          <input type="number" min={L.mouseTimeToMaxMs.min} max={L.mouseTimeToMaxMs.max} step="50"
            value={adv.options.mouse.timeToMaxMs} onchange={(e) => setOption({ mouse: { ...adv.options.mouse, timeToMaxMs: num(e) } })} /> ms
        </span>
      </label>
      <label class="opt">
        <span>Mouse step interval</span>
        <span class="num">
          <input type="number" min={L.mouseIntervalMs.min} max={L.mouseIntervalMs.max}
            value={adv.options.mouse.intervalMs} onchange={(e) => setOption({ mouse: { ...adv.options.mouse, intervalMs: num(e) } })} /> ms
        </span>
      </label>
      <label class="opt">
        <span>Scroll step interval</span>
        <span class="num">
          <input type="number" min={L.wheelIntervalMs.min} max={L.wheelIntervalMs.max}
            value={adv.options.mouse.wheelIntervalMs} onchange={(e) => setOption({ mouse: { ...adv.options.mouse, wheelIntervalMs: num(e) } })} /> ms
        </span>
      </label>
    </div>
    {#if optionError}<p class="error" role="alert">{optionError}</p>{/if}
  </section>

  {#each TABLES as t (t.id)}
    {@const slots = used(t.id)}
    <section class="group">
      <header>
        <div>
          <h2>{t.title} <span class="count">{slots.length}/{ADVANCED_SLOTS[t.id]}</span></h2>
          <p class="blurb">{t.blurb}</p>
        </div>
        <button class="btn small" disabled={session.busy || slots.length >= ADVANCED_SLOTS[t.id]} onclick={() => startNew(t.id)}>
          Add {t.noun}
        </button>
      </header>

      {#if slots.length}
        <ul class="entries">
          {#each slots as i (i)}
            <li class:editing={editing?.table === t.id && editing.index === i}>
              {#if t.id === "tapDances"}<code>TD({i})</code>{/if}
              <span class="sum">{summary(t.id, adv[t.id][i]!, { layerCount, keyLabel })}</span>
              <button class="btn ghost small" onclick={() => edit(t.id, i)}>Edit</button>
              <button class="btn ghost small" disabled={session.busy} onclick={() => remove(t.id, i)}>Remove</button>
            </li>
          {/each}
        </ul>
      {/if}

      {#if editing && editing.table === t.id}
        <div class="editor">
          <h3>{isAdvancedEntryUsed(t.id, adv[t.id][editing.index]!) ? "Edit" : "New"} {t.noun}{t.id === "tapDances" ? ` TD(${editing.index})` : ""}</h3>

          {#if editing.table === "combos"}
            {@const d = editing.draft}
            <div class="fields">
              {#each d.keys as k, n (n)}
                {@render keySelect(k, `Key ${n + 1}`, (pos) => (d.keys[n] = pos))}
              {/each}
              <div class="keybtns">
                <button class="btn ghost small" disabled={d.keys.length >= COMBO_KEYS.max}
                  onclick={() => d.keys.push(choices.find((c) => !d.keys.some((k) => k.row === c.row && k.col === c.col)) ?? { row: 0, col: 0 })}>+ key</button>
                <button class="btn ghost small" disabled={d.keys.length <= 1} onclick={() => d.keys.pop()}>− key</button>
              </div>
            </div>
            <div class="fields">
              <KeycodeField label="Sends" value={d.result} {layerCount} allowNone={false} onchange={(kc) => (d.result = kc)} />
              <label class="field">
                <span class="flabel">Window (0 = 50 ms)</span>
                <input type="number" min={L.comboTimeoutMs.min} max={L.comboTimeoutMs.max} bind:value={d.timeoutMs} />
              </label>
              {@render layerPicker(d.layers, (next) => (d.layers = next))}
            </div>
          {:else if editing.table === "tapDances"}
            {@const d = editing.draft}
            <div class="fields">
              <KeycodeField label="Tap" value={d.tap} {layerCount} onchange={(kc) => (d.tap = kc)} />
              <KeycodeField label="Hold" value={d.hold} {layerCount} onchange={(kc) => (d.hold = kc)} />
              <KeycodeField label="Double tap" value={d.doubleTap} {layerCount} onchange={(kc) => (d.doubleTap = kc)} />
              <KeycodeField label="Triple tap" value={d.tripleTap} {layerCount} onchange={(kc) => (d.tripleTap = kc)} />
              <label class="field">
                <span class="flabel">Term (0 = tapping term)</span>
                <input type="number" min={L.tapDanceTermMs.min} max={L.tapDanceTermMs.max} step="10" bind:value={d.termMs} />
              </label>
            </div>
          {:else if editing.table === "keyOverrides"}
            {@const d = editing.draft}
            <div class="fields">
              <fieldset class="layers">
                <legend>Held modifiers</legend>
                {#each MOD_BITS as m (m.bit)}
                  <label class="chip">
                    <input type="checkbox" checked={(d.triggerMods & m.bit) !== 0} onchange={() => (d.triggerMods ^= m.bit)} />
                    {m.label}
                  </label>
                {/each}
              </fieldset>
              <KeycodeField label="Key" value={d.triggerKey} {layerCount} allowNone={false} onchange={(kc) => (d.triggerKey = kc)} />
              <KeycodeField label="Sends instead" value={d.replacement} {layerCount} allowNone={false} onchange={(kc) => (d.replacement = kc)} />
              {@render layerPicker(d.layers, (next) => (d.layers = next))}
            </div>
          {:else if editing.table === "leader"}
            {@const d = editing.draft}
            <div class="fields">
              {#each d.keys as k, n (n)}
                <KeycodeField label={`Key ${n + 1}`} value={k} {layerCount} allowNone={false} onchange={(kc) => (d.keys[n] = kc)} />
              {/each}
              <div class="keybtns">
                <button class="btn ghost small" disabled={d.keys.length >= LEADER_KEYS.max} onclick={() => d.keys.push("KC_A")}>+ key</button>
                <button class="btn ghost small" disabled={d.keys.length <= LEADER_KEYS.min} onclick={() => d.keys.pop()}>− key</button>
              </div>
            </div>
            <div class="fields">
              <KeycodeField label="Sends" value={d.result} {layerCount} allowNone={false} onchange={(kc) => (d.result = kc)} />
            </div>
          {:else if editing.table === "tapHold" && editing.draft}
            {@const d = editing.draft}
            <div class="fields">
              {@render keySelect(d, "Key", (pos) => {
                d.row = pos.row;
                d.col = pos.col;
              })}
              <label class="field">
                <span class="flabel">Tapping term</span>
                <input type="number" min={L.tappingTermMs.min} max={L.tappingTermMs.max} step="10" bind:value={d.tappingTermMs} />
              </label>
              <label class="field">
                <span class="flabel">Quick tap (0 = off)</span>
                <input type="number" min={L.quickTapMs.min} max={L.quickTapMs.max} step="10" bind:value={d.quickTapMs} />
              </label>
            </div>
            <div class="fields checks">
              <label><input type="checkbox" bind:checked={d.permissiveHold} /> Permissive hold <small>another key tapped inside it means hold</small></label>
              <label><input type="checkbox" bind:checked={d.holdOnOtherKeyPress} /> Hold on other key press <small>any other key pressed means hold</small></label>
              <label><input type="checkbox" bind:checked={d.retroTap} /> Retro tap <small>held past the term with nothing else pressed still taps</small></label>
            </div>
          {/if}

          {#if problem}<p class="error" role="alert">{problem}</p>{/if}
          <div class="actions">
            <button class="btn ghost" onclick={() => (editing = null)}>Cancel</button>
            <button class="btn primary" disabled={!!problem || session.busy} onclick={apply}>Apply</button>
          </div>
        </div>
      {/if}
    </section>
  {/each}

  <p class="note">Changes take effect right away. Use <strong>Save to board</strong> to keep them after the board powers off.</p>
</div>

<style>
  .advanced {
    display: flex;
    flex-direction: column;
    gap: 18px;
    max-width: 900px;
  }

  .group {
    padding: 18px 20px;
    border-radius: var(--radius-lg);
    background: var(--surface);
    border: 1px solid var(--border);
  }

  header {
    display: flex;
    justify-content: space-between;
    align-items: flex-start;
    gap: 16px;
  }

  h2 {
    font-size: 15px;
    margin-bottom: 4px;
  }

  h3 {
    font-size: 14px;
  }

  .count {
    font-size: 12px;
    font-weight: 600;
    padding: 0 7px;
    border-radius: 999px;
    background: var(--surface-3);
    color: var(--text-muted);
  }

  .blurb,
  .note {
    margin: 0;
    color: var(--text-muted);
  }

  small {
    font-weight: 400;
    color: var(--text-muted);
  }

  .options {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(320px, 1fr));
    gap: 4px 28px;
    margin-top: 8px;
  }

  .opt {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 12px;
    padding: 8px 0;
    border-top: 1px solid var(--surface-3);
  }

  .opt > span:first-child {
    display: flex;
    flex-direction: column;
    font-weight: 600;
  }

  .num {
    flex: none;
    color: var(--text-muted);
  }

  input[type="number"],
  select {
    font: inherit;
    padding: 6px 9px;
    border-radius: var(--radius-sm);
    border: 1px solid var(--border-strong);
    background: var(--bg);
    color: var(--text);
  }

  input[type="number"] {
    width: 90px;
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

  .entries {
    list-style: none;
    margin: 12px 0 0;
    padding: 0;
  }

  .entries li {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 7px 10px;
    border-top: 1px solid var(--surface-3);
  }

  .entries li.editing {
    background: var(--accent-soft);
    border-radius: var(--radius-sm);
  }

  .sum {
    flex: 1;
    min-width: 0;
  }

  code {
    font-family: var(--font-mono);
    font-size: 12px;
    padding: 1px 6px;
    border-radius: var(--radius-sm);
    background: var(--ochre-soft);
  }

  .editor {
    display: flex;
    flex-direction: column;
    gap: 12px;
    margin-top: 14px;
    padding: 14px 16px;
    border-radius: var(--radius);
    background: var(--surface-2);
    border: 1px solid var(--border);
  }

  .fields {
    display: flex;
    flex-wrap: wrap;
    align-items: flex-start;
    gap: 12px 16px;
  }

  .field {
    display: flex;
    flex-direction: column;
    gap: 3px;
  }

  .flabel,
  legend {
    font-size: 12px;
    font-weight: 600;
    color: var(--text-muted);
  }

  .keybtns {
    display: flex;
    gap: 6px;
    align-self: center;
  }

  .layers {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
    border: 0;
    padding: 0;
    margin: 0;
  }

  .chip {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    padding: 3px 9px;
    border-radius: 999px;
    border: 1px solid var(--border-strong);
    background: var(--bg);
    font-size: 13px;
    cursor: pointer;
  }

  .chip:has(input:checked) {
    border-color: var(--chip, var(--accent));
    box-shadow: inset 0 0 0 1px var(--chip, var(--accent));
  }

  .checks {
    flex-direction: column;
    gap: 6px;
  }

  .error {
    margin: 0;
    color: var(--danger);
    font-weight: 600;
  }

  .actions {
    display: flex;
    justify-content: flex-end;
    gap: 8px;
  }
</style>
