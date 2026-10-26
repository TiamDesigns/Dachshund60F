<script lang="ts" module>
  let nextId = 0;
</script>

<script lang="ts">
  import { allKeycodes, isBoardKeycode, keycodeInfo, type KeycodeId } from "@dachshund60f/host-lib";

  // A keycode typed or picked from the catalogue; modded keys like LSFT(KC_1) can be typed.
  let {
    value,
    layerCount,
    label,
    allowNone = true,
    compact = false,
    onchange,
  }: {
    value: KeycodeId;
    layerCount: number;
    label: string;
    allowNone?: boolean;
    /** One line: no visible label or hint (the hint becomes the tooltip). */
    compact?: boolean;
    onchange: (keycode: KeycodeId) => void;
  } = $props();

  const listId = `keycodes-${nextId++}`;
  const choices = $derived(allKeycodes(layerCount).filter((k) => allowNone || k.id !== "KC_NO"));
  let text = $derived(value);
  const valid = $derived(isBoardKeycode(text.trim(), layerCount) && (allowNone || text.trim() !== "KC_NO"));
  const name = $derived(valid ? (keycodeInfo(text.trim(), layerCount)?.name ?? "") : "");

  function commit() {
    const id = text.trim();
    if (valid && id !== value) onchange(id);
  }
</script>

<label class="field" class:compact>
  {#if !compact}<span class="label">{label}</span>{/if}
  <input
    type="text"
    list={listId}
    spellcheck="false"
    autocomplete="off"
    class:invalid={!valid}
    aria-invalid={!valid}
    aria-label={compact ? label : undefined}
    title={compact ? (valid ? name : "Not a key this board knows") : undefined}
    value={text}
    oninput={(e) => (text = e.currentTarget.value)}
    onchange={commit}
  />
  {#if !compact}<small>{valid ? name || " " : "Not a key this board knows"}</small>{/if}
  <datalist id={listId}>
    {#each choices as k (k.id)}
      <option value={k.id}>{k.name}</option>
    {/each}
  </datalist>
</label>

<style>
  .field {
    display: flex;
    flex-direction: column;
    gap: 3px;
    min-width: 150px;
  }

  .compact {
    min-width: 0;
  }

  .label {
    font-size: 12px;
    font-weight: 600;
    color: var(--text-muted);
  }

  input {
    font: inherit;
    font-family: var(--font-mono);
    font-size: 13px;
    padding: 6px 9px;
    border-radius: var(--radius-sm);
    border: 1px solid var(--border-strong);
    background: var(--bg);
    color: var(--text);
  }

  input.invalid {
    border-color: var(--danger);
  }

  small {
    font-size: 12px;
    color: var(--text-faint);
    min-height: 1.3em;
  }

  input.invalid + small {
    color: var(--danger);
  }
</style>
