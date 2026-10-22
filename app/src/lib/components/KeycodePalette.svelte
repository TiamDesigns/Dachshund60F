<script lang="ts">
  import { allKeycodes, KEYCODE_CATEGORIES, type KeycodeCategory, type KeycodeId } from "@dachshund60f/host-lib";

  import { KEYCODE_MIME } from "./KeyboardView.svelte";

  let {
    layerCount,
    current,
    disabled,
    onpick,
  }: { layerCount: number; current: KeycodeId | null; disabled: boolean; onpick: (keycode: KeycodeId) => void } =
    $props();

  let query = $state("");
  let category = $state<KeycodeCategory | "all">("all");

  const all = $derived(allKeycodes(layerCount));
  const matches = $derived.by(() => {
    const q = query.trim().toLowerCase();
    return all.filter(
      (k) =>
        (category === "all" || k.category === category) &&
        (!q || k.label.toLowerCase().includes(q) || k.name.toLowerCase().includes(q) || k.id.toLowerCase().includes(q)),
    );
  });
  const groups = $derived(
    KEYCODE_CATEGORIES.map((c) => ({ ...c, items: matches.filter((k) => k.category === c.id) })).filter(
      (g) => g.items.length > 0,
    ),
  );
</script>

<div class="palette" class:disabled>
  <div class="bar">
    <label class="search">
      <svg width="15" height="15" viewBox="0 0 16 16" aria-hidden="true">
        <circle cx="7" cy="7" r="4.5" fill="none" stroke="currentColor" stroke-width="1.6" />
        <path d="M10.5 10.5 14 14" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" />
      </svg>
      <span class="visually-hidden">Search keys</span>
      <input type="search" placeholder="Fetch a key: try “vol”, “F13” or “layer”" bind:value={query} />
    </label>
    <div class="cats" role="radiogroup" aria-label="Key category">
      <button role="radio" aria-checked={category === "all"} class:on={category === "all"} onclick={() => (category = "all")}>
        All
      </button>
      {#each KEYCODE_CATEGORIES as c (c.id)}
        <button role="radio" aria-checked={category === c.id} class:on={category === c.id} onclick={() => (category = c.id)}>
          {c.label}
        </button>
      {/each}
    </div>
  </div>

  <div class="groups">
    {#each groups as g (g.id)}
      <section>
        <h3>{g.label}</h3>
        <div class="grid">
          {#each g.items as k (k.id)}
            <button
              class="kc"
              class:current={k.id === current}
              class:wide={k.label.length > 3}
              title={`${k.name} (${k.id})`}
              aria-label={k.name}
              {disabled}
              draggable="true"
              ondragstart={(e) => e.dataTransfer?.setData(KEYCODE_MIME, k.id)}
              onclick={() => onpick(k.id)}
            >
              {#if k.shifted}<span class="shifted">{k.shifted}</span>{/if}
              <span>{k.label || "∅"}</span>
            </button>
          {/each}
        </div>
      </section>
    {:else}
      <p class="none">No key by that name. Try a shorter word.</p>
    {/each}
  </div>
</div>

<style>
  .palette {
    display: flex;
    flex-direction: column;
    gap: 12px;
    padding: 16px;
    border-radius: var(--radius-lg);
    background: var(--surface);
    border: 1px solid var(--border);
  }

  .bar {
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
    align-items: center;
  }

  .search {
    display: flex;
    align-items: center;
    gap: 8px;
    flex: 1 1 260px;
    padding: 7px 12px;
    border-radius: 999px;
    border: 1px solid var(--border-strong);
    background: var(--bg);
    color: var(--text-muted);
  }

  .search input {
    flex: 1;
    border: 0;
    outline: 0;
    background: transparent;
    font: inherit;
    color: var(--text);
  }

  .cats {
    display: flex;
    flex-wrap: wrap;
    gap: 4px;
  }

  .cats button {
    padding: 4px 11px;
    border-radius: 999px;
    border: 1px solid var(--border);
    background: transparent;
    cursor: pointer;
    font-size: 12px;
    font-weight: 600;
    color: var(--text-muted);
  }

  .cats button.on {
    background: var(--text);
    border-color: var(--text);
    color: var(--surface);
  }

  .groups {
    display: flex;
    flex-direction: column;
    gap: 14px;
    max-height: 330px;
    overflow-y: auto;
    padding-right: 4px;
  }

  h3 {
    font-size: 11px;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: var(--text-faint);
    margin-bottom: 6px;
  }

  .grid {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
  }

  .kc {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-width: 44px;
    height: 44px;
    padding: 0 8px;
    border-radius: 7px;
    border: 1px solid var(--key-edge);
    border-bottom-width: 4px;
    border-bottom-color: var(--key-shadow);
    background: var(--key-face);
    color: var(--key-legend);
    font-size: 13px;
    font-weight: 650;
    line-height: 1.1;
    cursor: grab;
  }

  .kc.wide {
    font-size: 12px;
  }

  .kc:hover:not(:disabled) {
    border-color: var(--accent);
    border-bottom-color: var(--accent-hover);
  }

  .kc.current {
    background: var(--accent-soft);
    border-color: var(--accent);
  }

  .shifted {
    font-size: 10px;
    color: var(--key-sub);
  }

  .disabled .kc {
    cursor: not-allowed;
  }

  .none {
    color: var(--text-muted);
    margin: 8px 0;
  }
</style>
