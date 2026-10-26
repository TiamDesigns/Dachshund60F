<script lang="ts">
  import { defaultLayoutChoices, keycodeInfo, type KeycodeId } from "@dachshund60f/host-lib";

  import type { BoardSession } from "../app-state.svelte.ts";
  import { countAssigned, NO_KEY, resolveKey, TRANSPARENT } from "../keymap.ts";
  import KeyboardView, { type Target } from "./KeyboardView.svelte";
  import KeycodePalette from "./KeycodePalette.svelte";

  let { session }: { session: BoardSession } = $props();

  // The editor is only shown once the keymap has loaded, so the board is known.
  const def = $derived(session.definition!);
  const layerCount = $derived(session.board.info.layerCount);
  const layerNames = $derived(
    Array.from({ length: layerCount }, (_, i) => (i === 0 ? "Base" : i === 1 ? "Fn" : `Layer ${i}`)),
  );

  let layer = $state(0);
  // Writable derived: starts at the board's default layout, then follows the user's pick.
  let choices = $derived(defaultLayoutChoices(def));
  let target = $state<Target | null>({ kind: "key", row: 0, col: 0 });
  /** Which roller direction the palette assigns to. */
  let rollDir = $state<"cw" | "ccw">("cw");

  const keymap = $derived(session.keymap!);

  const selectedKey = $derived.by(() => {
    const t = target;
    if (t?.kind !== "key") return null;
    const r = resolveKey(keymap.keys, layer, t.row, t.col);
    const physical = def.keys.find((k) => k.row === t.row && k.col === t.col);
    const isPush = def.encoders.some((e) => e.push?.row === t.row && e.push?.col === t.col);
    return { ...r, row: t.row, col: t.col, legend: physical?.legend.join(" ") ?? (isPush ? "Roller press" : "") };
  });

  const selectedEncoder = $derived(target?.kind === "encoder" ? keymap.encoders[layer]?.[target.id] : undefined);

  const current = $derived(
    selectedKey ? selectedKey.own : selectedEncoder ? selectedEncoder[rollDir] : null,
  );

  function info(kc: KeycodeId) {
    return keycodeInfo(kc, layerCount);
  }

  function pick(kc: KeycodeId) {
    if (target?.kind === "key") {
      void session.setKey(layer, target.row, target.col, kc);
    } else if (target?.kind === "encoder" && selectedEncoder) {
      void session.setEncoder(layer, target.id, { ...selectedEncoder, [rollDir]: kc });
    }
  }

  function clearSelected() {
    pick(layer === 0 ? NO_KEY : TRANSPARENT);
  }

  function selectRollerPress() {
    const push = def.encoders.find((e) => target?.kind === "encoder" && e.id === target.id)?.push;
    if (push) target = { kind: "key", row: push.row, col: push.col };
  }

  function onKeydown(e: KeyboardEvent) {
    const inField = e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement;
    const mod = e.ctrlKey || e.metaKey;
    if (mod && e.key.toLowerCase() === "s") {
      e.preventDefault();
      if (session.unsaved) void session.save();
    } else if (inField) {
      return;
    } else if (mod && e.key.toLowerCase() === "z" && !e.shiftKey) {
      e.preventDefault();
      void session.undo();
    } else if (mod && (e.key.toLowerCase() === "y" || (e.key.toLowerCase() === "z" && e.shiftKey))) {
      e.preventDefault();
      void session.redo();
    } else if ((e.key === "Delete" || e.key === "Backspace") && target) {
      e.preventDefault();
      clearSelected();
    }
  }
</script>

<svelte:window onkeydown={onKeydown} />

<div class="editor">
  <div class="toolbar">
    <div class="layers" role="tablist" aria-label="Layers">
      {#each layerNames as name, i (i)}
        <button
          role="tab"
          aria-selected={layer === i}
          class:active={layer === i}
          style="--c: var(--layer-{i % 4})"
          onclick={() => (layer = i)}
        >
          <span class="swatch"></span>
          {name}
          <span class="n" title="Keys set on this layer">{countAssigned(keymap.keys[i] ?? [])}</span>
        </button>
      {/each}
    </div>

    <div class="tools">
      {#each def.layoutOptions as opt, oi (opt.id)}
        <label class="option">
          {opt.label}
          <select
            value={choices[oi]}
            onchange={(e) => {
              const v = Number(e.currentTarget.value);
              choices = choices.map((c, i) => (i === oi ? v : c));
            }}
          >
            {#each opt.choices as c, ci (ci)}
              <option value={ci}>{c}</option>
            {/each}
          </select>
        </label>
      {/each}
      <button class="btn small" onclick={() => session.undo()} disabled={!session.canUndo} title="Undo (Ctrl+Z)">
        ↶ Undo
      </button>
      <button class="btn small" onclick={() => session.redo()} disabled={!session.canRedo} title="Redo (Ctrl+Y)">
        ↷ Redo
      </button>
    </div>
  </div>

  <KeyboardView
    definition={def}
    keys={keymap.keys}
    encoders={keymap.encoders}
    {layer}
    {layerCount}
    {layerNames}
    {choices}
    {target}
    battery={session.battery}
    onselect={(t) => (target = t)}
    ondropkeycode={(row, col, kc) => {
      target = { kind: "key", row, col };
      void session.setKey(layer, row, col, kc);
    }}
  />

  <div class="inspector" aria-live="polite">
    {#if selectedKey}
      {@const own = info(selectedKey.own)}
      {@const eff = info(selectedKey.effective)}
      <div class="what">
        <span class="where">{layerNames[layer]} · {selectedKey.legend || `row ${selectedKey.row}, col ${selectedKey.col}`}</span>
        <span class="does">
          {#if selectedKey.own === TRANSPARENT}
            Transparent: does <strong>{eff?.name ?? selectedKey.effective}</strong> from {layerNames[selectedKey.fromLayer]}
          {:else if selectedKey.own === NO_KEY}
            Does nothing
          {:else}
            <strong>{own?.name ?? selectedKey.own}</strong>
          {/if}
        </span>
      </div>
      <button class="btn small" onclick={clearSelected} disabled={selectedKey.own === (layer === 0 ? NO_KEY : TRANSPARENT)}>
        {layer === 0 ? "Clear key" : "Make transparent"}
      </button>
    {:else if selectedEncoder}
      <div class="what">
        <span class="where">{layerNames[layer]} · Roller</span>
        <span class="does">Pick a direction, then a key.</span>
      </div>
      <div class="roll" role="radiogroup" aria-label="Roller direction">
        <button role="radio" aria-checked={rollDir === "cw"} class:on={rollDir === "cw"} onclick={() => (rollDir = "cw")}>
          Roll forward: <strong>{info(selectedEncoder.cw)?.label || "∅"}</strong>
        </button>
        <button role="radio" aria-checked={rollDir === "ccw"} class:on={rollDir === "ccw"} onclick={() => (rollDir = "ccw")}>
          Roll back: <strong>{info(selectedEncoder.ccw)?.label || "∅"}</strong>
        </button>
        <button onclick={selectRollerPress}>Press…</button>
      </div>
    {:else if target?.kind === "encoder"}
      <div class="what">
        <span class="where">{layerNames[layer]} · Roller</span>
        <span class="does">This board's firmware can't change the roller yet; that arrives with encoder support.</span>
      </div>
      <button class="btn small" onclick={selectRollerPress}>Press…</button>
    {:else}
      <span class="hint">Click a key or the roller to change what it does.</span>
    {/if}
    <span class="tip">Tip: drag keys from the palette onto the board, or between keys to copy.</span>
  </div>

  <KeycodePalette {layerCount} {current} disabled={!target || session.busy} onpick={pick} />
</div>

<style>
  .editor {
    display: flex;
    flex-direction: column;
    gap: 18px;
  }

  .toolbar {
    display: flex;
    flex-wrap: wrap;
    gap: 12px;
    align-items: center;
    justify-content: space-between;
  }

  .layers {
    display: flex;
    gap: 6px;
    flex-wrap: wrap;
  }

  .layers button {
    display: inline-flex;
    align-items: center;
    gap: 7px;
    padding: 6px 12px;
    border-radius: 999px;
    border: 1px solid var(--border);
    background: var(--surface);
    font-weight: 600;
    cursor: pointer;
  }

  .layers button.active {
    border-color: var(--c);
    box-shadow: 0 0 0 1px var(--c);
  }

  .swatch {
    width: 10px;
    height: 10px;
    border-radius: 50%;
    background: var(--c);
  }

  .n {
    font-size: 12px;
    color: var(--text-faint);
    font-variant-numeric: tabular-nums;
  }

  .tools {
    display: flex;
    gap: 8px;
    align-items: center;
    flex-wrap: wrap;
  }

  .option {
    display: inline-flex;
    gap: 6px;
    align-items: center;
    font-size: 12px;
    color: var(--text-muted);
  }

  select {
    font: inherit;
    padding: 4px 8px;
    border-radius: var(--radius-sm);
    border: 1px solid var(--border-strong);
    background: var(--surface);
    color: var(--text);
  }

  .inspector {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 12px 16px;
    min-height: 44px;
    padding: 10px 16px;
    border-radius: var(--radius);
    background: var(--surface-2);
  }

  .what {
    display: flex;
    flex-direction: column;
  }

  .where {
    font-size: 12px;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: var(--text-faint);
  }

  .roll {
    display: flex;
    gap: 6px;
    flex-wrap: wrap;
  }

  .roll button {
    padding: 5px 12px;
    border-radius: 999px;
    border: 1px solid var(--border-strong);
    background: var(--surface);
    cursor: pointer;
  }

  .roll button.on {
    border-color: var(--accent);
    background: var(--accent-soft);
  }

  .hint {
    color: var(--text-muted);
  }

  .tip {
    margin-left: auto;
    font-size: 12px;
    color: var(--text-faint);
  }
</style>
