<script lang="ts">
  import {
    ADVANCED_LIMITS,
    checkMacros,
    MACRO_BUFFER_SIZE,
    MACRO_COUNT,
    macroBytes,
    type MacroStep,
  } from "@dachshund60f/host-lib";

  import { newStep } from "../advanced.ts";
  import type { BoardSession } from "../app-state.svelte.ts";
  import KeycodeField from "./KeycodeField.svelte";

  let { session }: { session: BoardSession } = $props();

  const STEP_TYPES: { type: MacroStep["type"]; label: string }[] = [
    { type: "text", label: "Type text" },
    { type: "tap", label: "Tap key" },
    { type: "down", label: "Hold key" },
    { type: "up", label: "Release key" },
    { type: "delay", label: "Pause" },
  ];

  const layerCount = $derived(session.board.info.layerCount);

  // Macros are edited here and sent to the board together, since they share one buffer.
  let draft = $state<MacroStep[][]>([]);
  $effect(() => {
    draft = $state.snapshot(session.macros ?? []) as MacroStep[][];
  });

  const changed = $derived(JSON.stringify(draft) !== JSON.stringify(session.macros ?? []));
  const bytes = $derived(macroBytes(draft));
  const problem = $derived.by(() => {
    try {
      checkMacros($state.snapshot(draft) as MacroStep[][], { layerCount, rows: 0, cols: 0 });
      return null;
    } catch (err) {
      return err instanceof Error ? err.message : String(err);
    }
  });

  function move(steps: MacroStep[], i: number, by: number) {
    const j = i + by;
    if (j < 0 || j >= steps.length) return;
    [steps[i], steps[j]] = [steps[j]!, steps[i]!];
  }

  function changeType(steps: MacroStep[], i: number, type: MacroStep["type"]) {
    steps[i] = newStep(type);
  }

  async function send() {
    await session.setMacros($state.snapshot(draft) as MacroStep[][]);
  }
</script>

<div class="macros">
  <section class="group intro">
    <div>
      <h2>Macros</h2>
      <p class="blurb">Put M0, M1 and so on on a key (Keys tab, “Macros &amp; dances”) and it plays its macro: typed text, key taps, held keys and pauses.</p>
    </div>
    <div class="meter" title="Macro storage on the board">
      <meter min="0" max={MACRO_BUFFER_SIZE} value={bytes ?? MACRO_BUFFER_SIZE} high={MACRO_BUFFER_SIZE * 0.9}></meter>
      <small>{bytes ?? "?"} of {MACRO_BUFFER_SIZE} bytes</small>
    </div>
  </section>

  {#each draft as steps, m (m)}
    <section class="group">
      <header>
        <h3><code>M{m}</code> {steps.length ? `${steps.length} step${steps.length > 1 ? "s" : ""}` : "empty"}</h3>
        <button class="btn ghost small" disabled={!steps.length} onclick={() => (draft[m] = [])}>Clear</button>
      </header>

      <ol class="steps">
        {#each steps as step, i (i)}
          <li>
            <select value={step.type} aria-label="Step type" onchange={(e) => changeType(steps, i, e.currentTarget.value as MacroStep["type"])}>
              {#each STEP_TYPES as t (t.type)}
                <option value={t.type}>{t.label}</option>
              {/each}
            </select>
            <span class="value">
              {#if step.type === "text"}
                <input class="text" type="text" placeholder="Hello!" aria-label="Text to type" bind:value={step.text} />
              {:else if step.type === "delay"}
                <input type="number" min={ADVANCED_LIMITS.macroDelayMs.min} max={ADVANCED_LIMITS.macroDelayMs.max} step="10"
                  aria-label="Pause in milliseconds" bind:value={step.ms} /> ms
              {:else}
                <KeycodeField label="Key" compact value={step.keycode} {layerCount} allowNone={false} onchange={(kc) => (step.keycode = kc)} />
              {/if}
            </span>
            <span class="tools">
              <button class="btn ghost small" aria-label="Move up" disabled={i === 0} onclick={() => move(steps, i, -1)}>↑</button>
              <button class="btn ghost small" aria-label="Move down" disabled={i === steps.length - 1} onclick={() => move(steps, i, 1)}>↓</button>
              <button class="btn ghost small" aria-label="Remove step" onclick={() => steps.splice(i, 1)}>✕</button>
            </span>
          </li>
        {/each}
      </ol>

      <div class="add">
        {#each STEP_TYPES as t (t.type)}
          <button class="btn small" onclick={() => steps.push(newStep(t.type))}>+ {t.label}</button>
        {/each}
      </div>
    </section>
  {/each}

  <div class="bar">
    <button class="btn" disabled={draft.length >= MACRO_COUNT} onclick={() => draft.push([])}>Add macro M{draft.length}</button>
    <span class="spacer"></span>
    {#if problem}<p class="error" role="alert">{problem}</p>{/if}
    <button class="btn ghost" disabled={!changed} onclick={() => (draft = $state.snapshot(session.macros ?? []) as MacroStep[][])}>Discard</button>
    <button class="btn primary" disabled={!changed || !!problem || session.busy} onclick={send}>Send to board</button>
  </div>

  <p class="note">Sent macros work right away. Use <strong>Save to board</strong> to keep them after the board powers off. Text is typed as on a US layout.</p>
</div>

<style>
  .macros {
    display: flex;
    flex-direction: column;
    gap: 16px;
    max-width: 900px;
  }

  .group {
    padding: 16px 20px;
    border-radius: var(--radius-lg);
    background: var(--surface);
    border: 1px solid var(--border);
  }

  .intro,
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
    display: flex;
    align-items: center;
    gap: 8px;
    font-weight: 600;
    color: var(--text-muted);
  }

  code {
    font-family: var(--font-mono);
    font-size: 13px;
    padding: 1px 7px;
    border-radius: var(--radius-sm);
    background: var(--ochre-soft);
    color: var(--text);
  }

  .blurb,
  .note {
    margin: 0;
    color: var(--text-muted);
  }

  .meter {
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    gap: 2px;
    flex: none;
  }

  meter {
    width: 180px;
  }

  small {
    color: var(--text-muted);
  }

  .steps {
    margin: 10px 0;
    padding-left: 22px;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }

  .steps li {
    padding-left: 4px;
  }

  .steps li > * {
    vertical-align: middle;
  }

  .value {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    margin: 0 8px;
  }

  .tools {
    display: inline-flex;
    gap: 2px;
  }

  select,
  input[type="number"],
  .text {
    font: inherit;
    padding: 6px 9px;
    border-radius: var(--radius-sm);
    border: 1px solid var(--border-strong);
    background: var(--bg);
    color: var(--text);
  }

  .text {
    min-width: 260px;
  }

  input[type="number"] {
    width: 90px;
  }

  .add {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
  }

  .bar {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
  }

  .spacer {
    flex: 1;
  }

  .error {
    margin: 0;
    color: var(--danger);
    font-weight: 600;
  }
</style>
