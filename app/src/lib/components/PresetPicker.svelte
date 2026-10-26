<script lang="ts">
  import type { BoardSession } from "../app-state.svelte.ts";
  import { presets } from "../presets.svelte.ts";
  import { activePreset, currentValues, type Preset } from "../presets.ts";
  import InfoTip from "./InfoTip.svelte";

  let { session }: { session: BoardSession } = $props();

  const canSwitchOff = $derived(session.supports("featureSwitches"));
  const options = $derived(session.advanced?.options ?? null);
  let picked = $state<string | null>(null);
  const active = $derived(
    session.settings ? activePreset(presets.all, session.settings, options, canSwitchOff, picked) : null,
  );

  let naming = $state(false);
  let newName = $state("");
  const trimmed = $derived(newName.trim());
  const taken = $derived(presets.all.some((p) => p.name.toLowerCase() === trimmed.toLowerCase()));

  function needsSwitches(p: Preset): boolean {
    return !!options && !canSwitchOff && (p.off.tapHold || p.off.tapDance || p.off.combos);
  }

  function choose(p: Preset) {
    if (session.busy || p.id === active?.id) return;
    picked = p.id;
    void session.applyPreset(p);
  }

  function saveNew(e: SubmitEvent) {
    e.preventDefault();
    if (!session.settings || !trimmed || taken) return;
    picked = presets.add(trimmed, currentValues($state.snapshot(session.settings), options ? $state.snapshot(options) : null)).id;
    naming = false;
    newName = "";
  }

  function remove(p: Preset) {
    if (confirm(`Delete the preset “${p.name}”? Boards keep their settings.`)) presets.remove(p.id);
  }
</script>

<section class="group">
  <h2>Presets<InfoTip id="presets" /></h2>
  <p class="hint">
    {#if active}
      {session.displayName} is set up as <strong>{active.name}</strong>.
    {:else}
      {session.displayName} has its own mix of settings. Save it as a preset to come back to it later.
    {/if}
  </p>

  <div class="presets" role="radiogroup" aria-label="Presets">
    {#each presets.all as p (p.id)}
      {@const on = p.id === active?.id}
      <div class="card" class:on>
        <button
          class="pick"
          role="radio"
          aria-checked={on}
          disabled={session.busy && !on}
          onclick={() => choose(p)}
        >
          <span class="top">
            <span class="name">{p.name}</span>
            {#if on}<span class="badge">On</span>{/if}
          </span>
          <span class="blurb">{p.blurb}</span>
          {#if needsSwitches(p)}
            <span class="warn">This board's firmware can't switch tap-hold, tap dance or combos off yet. Update it for the full preset.</span>
          {/if}
        </button>
        {#if !p.builtIn}
          <button class="del" aria-label="Delete the preset {p.name}" title="Delete" onclick={() => remove(p)}>×</button>
        {/if}
      </div>
    {/each}
  </div>

  {#if naming}
    <form class="new" onsubmit={saveNew}>
      <!-- svelte-ignore a11y_autofocus -->
      <input class="text" type="text" maxlength="32" placeholder="Preset name" aria-label="Preset name" bind:value={newName} autofocus />
      <button class="btn primary" type="submit" disabled={!trimmed || taken}>Save preset</button>
      <button class="btn ghost" type="button" onclick={() => ((naming = false), (newName = ""))}>Cancel</button>
      {#if taken}<small class="error">There's already a preset called that.</small>{/if}
    </form>
  {:else}
    <div class="new">
      <button class="btn" onclick={() => (naming = true)} disabled={!session.settings}>Save current settings as a preset…</button>
      <small>Switching applies at once; use <strong>Save to board</strong> to keep it after a power-off.</small>
    </div>
  {/if}
</section>

<style>
  .group {
    padding: 18px 20px;
    border-radius: var(--radius-lg);
    background: var(--surface);
    border: 1px solid var(--border);
  }

  h2 {
    font-size: 15px;
    margin-bottom: 4px;
    display: flex;
    align-items: center;
  }

  .hint {
    margin: 0 0 12px;
    color: var(--text-muted);
  }

  .presets {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(190px, 1fr));
    gap: 10px;
  }

  .card {
    position: relative;
    display: flex;
  }

  .pick {
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 4px;
    padding: 12px 14px;
    border-radius: var(--radius);
    border: 1px solid var(--border);
    background: var(--bg);
    color: var(--text);
    font: inherit;
    text-align: left;
    cursor: pointer;
    transition:
      border-color 120ms,
      background 120ms;
  }

  .pick:hover:not(:disabled) {
    border-color: var(--border-strong);
  }

  .pick:disabled {
    cursor: progress;
    opacity: 0.7;
  }

  .card.on .pick {
    border-color: var(--accent);
    background: var(--accent-soft);
    box-shadow: inset 0 0 0 1px var(--accent);
  }

  .top {
    display: flex;
    align-items: center;
    gap: 8px;
    padding-right: 18px;
  }

  .name {
    font-weight: 700;
  }

  .badge {
    padding: 1px 8px;
    border-radius: 999px;
    background: var(--accent);
    color: var(--accent-text);
    font-size: 11px;
    font-weight: 700;
  }

  .blurb {
    font-size: 12.5px;
    color: var(--text-muted);
  }

  .warn {
    font-size: 12px;
    color: var(--danger);
  }

  .del {
    position: absolute;
    top: 6px;
    right: 6px;
    width: 24px;
    height: 24px;
    border: 0;
    border-radius: 50%;
    background: transparent;
    color: var(--text-faint);
    font-size: 18px;
    line-height: 1;
    cursor: pointer;
  }

  .del:hover {
    color: var(--danger);
    background: var(--danger-soft);
  }

  .new {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 10px;
    margin-top: 14px;
  }

  .new small {
    color: var(--text-muted);
  }

  .new small.error {
    color: var(--danger);
  }

  .text {
    font: inherit;
    padding: 7px 10px;
    border-radius: var(--radius-sm);
    border: 1px solid var(--border-strong);
    background: var(--bg);
    color: var(--text);
    min-width: 200px;
  }
</style>
