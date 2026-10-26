<script lang="ts">
  import { BREEDS, type BoardSession } from "../app-state.svelte.ts";
  import BoneBattery from "./BoneBattery.svelte";
  import ConnectionChip from "./ConnectionChip.svelte";
  import Dachshund from "./Dachshund.svelte";

  let { session }: { session: BoardSession } = $props();

  const info = $derived(session.board.info);
  const def = $derived(session.definition);
  const keyCount = $derived(
    def ? new Set(def.keys.map((k) => `${k.row},${k.col}`)).size + def.encoders.length : 0,
  );
  const canSave = $derived(session.supports("persistence") && !session.problem);

  function batteryWords(): string {
    const b = session.battery;
    if (!b) return "Battery unknown";
    if (b.charging) return `Charging, ${b.percent}%`;
    if (b.percent <= 15) return `Hungry: ${b.percent}% left`;
    return `${b.percent}% battery`;
  }
</script>

<header class="header">
  <div class="art">
    <Dachshund width={150} />
  </div>

  <div class="ident">
    <h1>{session.displayName}</h1>
    <p class="sub">
      <span>{info.model || "Unknown model"} · {BREEDS[info.model] ?? "Good dog"}</span>
      {#if keyCount}<span>· {keyCount} {keyCount === 1 ? "switch" : "switches"}</span>{/if}
      <span class="serial-line">Serial <span class="serial">{info.id}</span></span>
    </p>
    <div class="status">
      <ConnectionChip connection={info.connection} />
      <span class="battery">
        <BoneBattery battery={session.battery} width={56} showText={false} />
        {batteryWords()}
      </span>
    </div>
  </div>

  <div class="actions">
    {#if session.unsaved}
      <span class="unsaved" title="Edits are live on the board but will be lost when it powers off">
        <svg width="14" height="14" viewBox="0 0 24 24" aria-hidden="true">
          <ellipse cx="12" cy="16" rx="5" ry="4" fill="currentColor" />
          <circle cx="6" cy="10" r="2.2" fill="currentColor" />
          <circle cx="10" cy="6.5" r="2.2" fill="currentColor" />
          <circle cx="14" cy="6.5" r="2.2" fill="currentColor" />
          <circle cx="18" cy="10" r="2.2" fill="currentColor" />
        </svg>
        Unsaved changes
      </span>
    {/if}
    {#if session.supports("identify") && !session.problem}
      <button class="btn ghost" onclick={() => session.identify()} title="Flash this board's lights">Identify</button>
    {/if}
    {#if canSave}
      <button class="btn" onclick={() => session.revert()} disabled={!session.unsaved || session.busy}>Revert</button>
      <button class="btn primary" onclick={() => session.save()} disabled={!session.unsaved || session.busy}>
        Save to board
      </button>
    {/if}
  </div>
</header>

<style>
  .header {
    display: flex;
    align-items: center;
    gap: 24px;
    flex-wrap: wrap;
  }

  .art {
    flex: none;
    display: grid;
    place-items: center;
    width: 180px;
    height: 104px;
    border-radius: var(--radius-lg);
    /* A rug for the dog to stand on. */
    background:
      radial-gradient(ellipse 46% 13% at 50% 80%, var(--rug) 0 98%, transparent 100%),
      var(--avatar-bg);
    border: 1px solid var(--border);
  }

  .ident {
    flex: 1;
    min-width: 240px;
  }

  h1 {
    font-size: 30px;
    font-weight: 750;
  }

  .sub {
    display: flex;
    flex-wrap: wrap;
    column-gap: 0.3em;
    margin: 2px 0 10px;
    color: var(--text-muted);
  }

  /* Wrap between the parts, never inside one (the serial number especially). */
  .sub > span {
    white-space: nowrap;
  }

  .serial-line {
    flex-basis: 100%;
    font-size: 13px;
  }

  .serial {
    font-family: var(--font-mono);
    font-size: 13px;
  }

  .status {
    display: flex;
    align-items: center;
    gap: 14px;
    flex-wrap: wrap;
  }

  .battery {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    font-weight: 600;
    color: var(--text-muted);
  }

  .actions {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
  }

  .unsaved {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 4px 10px;
    border-radius: 999px;
    background: var(--accent-soft);
    color: var(--accent-hover);
    font-weight: 600;
    font-size: 12px;
  }
</style>
