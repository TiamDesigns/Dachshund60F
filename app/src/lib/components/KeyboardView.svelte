<script lang="ts" module>
  export type Target = { kind: "key"; row: number; col: number } | { kind: "encoder"; id: number };
  export const KEYCODE_MIME = "application/x-doghouse-keycode";
</script>

<script lang="ts">
  import {
    keycodeInfo,
    type BatteryStatus,
    type EncoderAction,
    type KeyboardDefinition,
    type KeycodeId,
  } from "@dachshund60f/host-lib";

  import { boardGeometry, fitUnit } from "../geometry.ts";
  import { resolveKey, TRANSPARENT } from "../keymap.ts";

  let {
    definition,
    keys,
    encoders,
    layer,
    layerCount,
    layerNames,
    choices,
    target,
    battery,
    onselect,
    ondropkeycode,
  }: {
    definition: KeyboardDefinition;
    keys: KeycodeId[][][];
    encoders: EncoderAction[][];
    layer: number;
    layerCount: number;
    layerNames: string[];
    choices: number[];
    target: Target | null;
    battery: BatteryStatus | null;
    onselect: (target: Target) => void;
    ondropkeycode: (row: number, col: number, keycode: KeycodeId) => void;
  } = $props();

  const GAP = 5;
  let width = $state(900);
  const unit = $derived(fitUnit(definition, width - 48, 24, 62));
  const geo = $derived(boardGeometry(definition, choices, unit, GAP));
  let dropTarget = $state<string | null>(null);

  function isSelected(row: number, col: number): boolean {
    return target?.kind === "key" && target.row === row && target.col === col;
  }

  function face(row: number, col: number) {
    const r = resolveKey(keys, layer, row, col);
    const info = keycodeInfo(r.effective, layerCount);
    const transparent = r.own === TRANSPARENT;
    const label = info?.label ?? r.effective;
    return {
      label,
      shifted: info?.shifted,
      name: info?.name ?? r.effective,
      transparent,
      fromLayer: r.fromLayer,
      empty: r.effective === "KC_NO",
      own: r.own,
      small: label.length > 4,
    };
  }

  function encoderSummary(id: number): string {
    const e = encoders[layer]?.[id];
    if (!e) return "";
    const cw = keycodeInfo(e.cw, layerCount)?.name ?? e.cw;
    const ccw = keycodeInfo(e.ccw, layerCount)?.name ?? e.ccw;
    return `Roll forward: ${cw}. Roll back: ${ccw}.`;
  }

  function ledLit(level: number | undefined): boolean {
    if (!battery || level === undefined) return false;
    return battery.percent > (level - 1) * 25;
  }

  function onDrop(event: DragEvent, row: number, col: number) {
    event.preventDefault();
    dropTarget = null;
    const kc = event.dataTransfer?.getData(KEYCODE_MIME);
    if (kc) ondropkeycode(row, col, kc);
  }

  function onDragOver(event: DragEvent, id: string) {
    if (event.dataTransfer?.types.includes(KEYCODE_MIME)) {
      event.preventDefault();
      event.dataTransfer.dropEffect = "copy";
      dropTarget = id;
    }
  }
</script>

<div class="wrap" bind:clientWidth={width}>
  <div
    class="case"
    style="--layer-color: var(--layer-{layer % 4}); width: {geo.width + unit * 0.7}px; padding: {unit * 0.35}px"
  >
    <div class="plate" style="width: {geo.width}px; height: {geo.height}px">
      {#each geo.keys as k (`${k.key.row},${k.key.col},${k.key.x}`)}
        {@const f = face(k.key.row, k.key.col)}
        {@const id = `${k.key.row},${k.key.col}`}
        <button
          class="key"
          class:selected={isSelected(k.key.row, k.key.col)}
          class:transparent={f.transparent}
          class:empty={f.empty}
          class:assigned={layer > 0 && !f.transparent}
          class:drop={dropTarget === id}
          style="left: {k.x}px; top: {k.y}px; width: {k.w}px; height: {k.h}px; font-size: {unit * 0.25}px"
          aria-label="{k.key.legend.join(' ')} key: {f.empty ? 'does nothing' : f.name}{f.transparent
            ? `, from ${layerNames[f.fromLayer]}`
            : ''}"
          title={f.transparent ? `${f.name} (falls through from ${layerNames[f.fromLayer]})` : f.name}
          draggable="true"
          ondragstart={(e) => e.dataTransfer?.setData(KEYCODE_MIME, f.own)}
          ondragover={(e) => onDragOver(e, id)}
          ondragleave={() => (dropTarget = null)}
          ondrop={(e) => onDrop(e, k.key.row, k.key.col)}
          onclick={() => onselect({ kind: "key", row: k.key.row, col: k.key.col })}
        >
          <span class="cap">
            {#if f.shifted}<span class="shifted">{f.shifted}</span>{/if}
            <span class="label" class:small={f.small}>{f.label}</span>
            {#if f.transparent}<span class="trns" aria-hidden="true">▽</span>{/if}
          </span>
        </button>
      {/each}

      {#each geo.encoders as e (e.id)}
        <button
          class="knob"
          class:selected={target?.kind === "encoder" && target.id === e.id}
          style="left: {e.cx - e.r}px; top: {e.cy - e.r}px; width: {e.r * 2}px; height: {e.r * 2}px"
          aria-label="Roller: {encoderSummary(e.id)}"
          title={encoderSummary(e.id)}
          onclick={() => onselect({ kind: "encoder", id: e.id })}
        >
          <svg viewBox="0 0 40 40" aria-hidden="true">
            <circle cx="20" cy="20" r="18" class="ring" />
            <circle cx="20" cy="20" r="12" class="hub" />
            <path d="M14 18a7 7 0 0 1 12-3m0 0v-4m0 4h-4M26 22a7 7 0 0 1-12 3m0 0v4m0-4h4" class="arrows" />
          </svg>
        </button>
      {/each}

      {#each geo.indicators as d (d.indicator.id)}
        <span
          class="led {d.indicator.role}"
          class:lit={d.indicator.role === "layer" || ledLit(d.indicator.level)}
          style="left: {d.cx}px; top: {d.cy}px; --size: {unit * 0.16}px"
          title={d.indicator.label}
        ></span>
      {/each}
    </div>
  </div>
</div>

<style>
  .wrap {
    width: 100%;
    display: flex;
    /* safe: never push the left edge out of reach when the board is wider */
    justify-content: safe center;
    overflow-x: auto;
    padding-bottom: 14px;
  }

  .case {
    border-radius: 18px;
    background:
      linear-gradient(180deg, rgb(255 255 255 / 0.08), transparent 30%),
      linear-gradient(160deg, var(--case), var(--case-edge));
    box-shadow:
      var(--shadow-lg),
      inset 0 1px 0 rgb(255 255 255 / 0.15);
  }

  .plate {
    position: relative;
  }

  .key {
    position: absolute;
    padding: 0;
    border: 0;
    border-radius: 7px;
    background: var(--key-shadow);
    cursor: pointer;
    transition: transform 80ms;
  }

  .cap {
    position: absolute;
    inset: 0 0 4px 0;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    border-radius: 7px;
    background: linear-gradient(180deg, var(--key-face), var(--key-edge));
    color: var(--key-legend);
    box-shadow: inset 0 -2px 0 rgb(0 0 0 / 0.04);
    line-height: 1.1;
    overflow: hidden;
  }

  .key.assigned .cap {
    box-shadow: inset 0 3px 0 var(--layer-color);
  }

  .key:hover .cap {
    filter: brightness(1.04);
  }

  .key:active {
    transform: translateY(1px);
  }

  .key.selected .cap,
  .key.drop .cap {
    outline: 3px solid var(--accent);
    outline-offset: -1px;
  }

  .key.drop .cap {
    outline-style: dashed;
  }

  .shifted {
    font-size: 0.75em;
    color: var(--key-sub);
  }

  .label {
    font-weight: 650;
  }

  .label.small {
    font-size: 0.8em;
  }

  .key.transparent .label,
  .key.transparent .shifted {
    color: var(--key-trns);
  }

  .trns {
    position: absolute;
    right: 4px;
    bottom: 1px;
    font-size: 0.7em;
    color: var(--key-trns);
  }

  .key.empty .cap {
    background: var(--key-edge);
  }

  .knob {
    position: absolute;
    padding: 0;
    border: 0;
    border-radius: 50%;
    background: transparent;
    cursor: pointer;
  }

  .knob svg {
    width: 100%;
    height: 100%;
    display: block;
  }

  .ring {
    fill: var(--key-shadow);
    stroke: var(--key-edge);
    stroke-width: 3;
    stroke-dasharray: 2 2.2;
  }

  .hub {
    fill: var(--key-face);
  }

  .arrows {
    fill: none;
    stroke: var(--key-sub);
    stroke-width: 1.8;
    stroke-linecap: round;
    stroke-linejoin: round;
  }

  .knob.selected .ring {
    stroke: var(--accent);
    stroke-dasharray: none;
  }

  .led {
    position: absolute;
    width: var(--size);
    height: var(--size);
    margin: calc(var(--size) / -2) 0 0 calc(var(--size) / -2);
    border-radius: 50%;
    background: var(--plate);
    box-shadow: inset 0 0 0 1px rgb(0 0 0 / 0.3);
  }

  .led.lit.layer {
    background: var(--layer-color);
    box-shadow: 0 0 8px var(--layer-color);
  }

  .led.lit.battery {
    background: var(--ochre);
    box-shadow: 0 0 6px var(--ochre);
  }
</style>
