<script lang="ts">
  import type { ConnectionKind } from "@dachshund60f/host-lib";

  let { connection, compact = false }: { connection: ConnectionKind; compact?: boolean } = $props();

  const LABELS: Record<ConnectionKind, { short: string; long: string }> = {
    usb: { short: "USB", long: "USB cable" },
    dongle: { short: "Dongle", long: "2.4 GHz dongle" },
    ble: { short: "Bluetooth", long: "Bluetooth" },
  };

  const text = $derived(compact ? LABELS[connection].short : LABELS[connection].long);
</script>

<span class="chip {connection}" title={`Connected by ${LABELS[connection].long}`}>
  <svg width="14" height="14" viewBox="0 0 16 16" aria-hidden="true">
    {#if connection === "usb"}
      <path d="M5 1h6v5H5z M4 6h8v3a4 4 0 0 1-8 0z M8 13v3" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round" />
    {:else if connection === "dongle"}
      <path d="M8 9.5a1.5 1.5 0 1 0 0-.01M5 7a4.2 4.2 0 0 1 6 0M2.8 4.8a7.4 7.4 0 0 1 10.4 0" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" />
      <path d="M6 12h4v3H6z" fill="currentColor" />
    {:else}
      <path d="M4.5 4.5l7 7L8 15V1l3.5 3.5-7 7" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" />
    {/if}
  </svg>
  {text}
</span>

<style>
  .chip {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    padding: 2px 9px 2px 7px;
    border-radius: 999px;
    font-size: 12px;
    font-weight: 600;
    white-space: nowrap;
  }

  .usb {
    background: var(--accent-soft);
    color: var(--accent-hover);
  }

  .dongle {
    background: var(--sage-soft);
    color: var(--sage);
  }

  .ble {
    background: var(--ochre-soft);
    color: var(--ochre);
  }
</style>
