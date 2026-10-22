<script lang="ts">
  import { BREEDS, type BoardSession } from "../app-state.svelte.ts";
  import BoneBattery from "./BoneBattery.svelte";
  import ConnectionChip from "./ConnectionChip.svelte";
  import Dachshund from "./Dachshund.svelte";

  let { session, selected, onselect }: { session: BoardSession; selected: boolean; onselect: () => void } = $props();
</script>

<button class="card" class:selected aria-pressed={selected} onclick={onselect}>
  <span class="avatar"><Dachshund width={58} /></span>
  <span class="body">
    <span class="name">
      {session.displayName}
      {#if session.unsaved}<span class="dot" title="Unsaved changes" aria-label="Unsaved changes"></span>{/if}
    </span>
    <span class="model">{session.board.info.model} · {BREEDS[session.board.info.model] ?? "Good dog"}</span>
    <span class="meta">
      <ConnectionChip connection={session.connection} compact />
      <BoneBattery battery={session.battery} width={40} />
    </span>
  </span>
</button>

<style>
  .card {
    display: flex;
    gap: 12px;
    align-items: center;
    width: 100%;
    padding: 12px;
    border-radius: var(--radius);
    border: 1px solid transparent;
    background: transparent;
    text-align: left;
    cursor: pointer;
    transition: background 120ms, border-color 120ms;
  }

  .card:hover {
    background: var(--surface-2);
  }

  .card.selected {
    background: var(--surface);
    border-color: var(--border);
    box-shadow: var(--shadow-sm);
  }

  .avatar {
    flex: none;
    display: grid;
    place-items: center;
    width: 64px;
    height: 48px;
    border-radius: var(--radius-sm);
    background: var(--avatar-bg);
  }

  .body {
    display: flex;
    flex-direction: column;
    gap: 2px;
    min-width: 0;
  }

  .name {
    display: flex;
    align-items: center;
    gap: 6px;
    font-weight: 700;
    font-size: 15px;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .dot {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    background: var(--accent);
  }

  .model {
    font-size: 12px;
    color: var(--text-muted);
  }

  .meta {
    display: flex;
    align-items: center;
    gap: 8px;
    margin-top: 4px;
  }
</style>
