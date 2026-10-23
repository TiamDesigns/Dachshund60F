<script lang="ts">
  import { HOST_LIB_VERSION } from "@dachshund60f/host-lib";

  import { BREEDS, type BoardSession } from "../app-state.svelte.ts";

  let { session }: { session: BoardSession } = $props();

  const info = $derived(session.board.info);
  const CONNECTIONS = { usb: "USB cable", dongle: "2.4 GHz dongle", ble: "Bluetooth" } as const;

  function enterBootloader() {
    if (confirm(`Restart ${session.displayName} into its bootloader? It stops typing until you copy firmware onto it or reset it.`)) {
      void session.enterBootloader();
    }
  }
</script>

<div class="about">
  <section class="card">
    <h2>Dog tag</h2>
    <dl>
      <dt>Model</dt>
      <dd>{info.model || "Unknown"} ({BREEDS[info.model] ?? "Good dog"})</dd>
      <dt>Serial number</dt>
      <dd class="mono">{info.id}</dd>
      <dt>Connected by</dt>
      <dd>{CONNECTIONS[info.connection]}</dd>
      <dt>Firmware</dt>
      <dd class="mono">{info.firmwareVersion}</dd>
      <dt>Protocol</dt>
      <dd class="mono">v{info.protocolVersion}</dd>
      <dt>Layers</dt>
      <dd>{info.layerCount}</dd>
      {#if info.capabilities}
        <dt>Features</dt>
        <dd>{info.capabilities.join(", ") || "None"}</dd>
      {/if}
      <dt>Battery</dt>
      <dd>{session.battery ? `${session.battery.percent}%${session.battery.charging ? ", charging" : ""}` : "Unknown"}</dd>
    </dl>
  </section>

  <section class="card">
    <h2>Firmware updates</h2>
    <p>Put the board in its bootloader, then copy the <code>.uf2</code> file onto the drive that appears. The board restarts with the new firmware.</p>
    {#if session.board.reboot && !session.problem}
      <button class="btn" onclick={enterBootloader} disabled={session.busy}>Restart into bootloader…</button>
    {:else}
      <p>To get there, hold Esc for 3 seconds or double-tap reset.</p>
    {/if}
  </section>

  <p class="foot">Doghouse 0.1.0 · host-lib {HOST_LIB_VERSION}</p>
</div>

<style>
  .about {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 18px;
    max-width: 900px;
  }

  .card {
    padding: 18px 20px;
    border-radius: var(--radius-lg);
    background: var(--surface);
    border: 1px solid var(--border);
  }

  h2 {
    font-size: 15px;
    margin-bottom: 10px;
  }

  .card .btn {
    margin-top: 12px;
  }

  dl {
    display: grid;
    grid-template-columns: max-content 1fr;
    gap: 8px 18px;
    margin: 0;
  }

  dt {
    color: var(--text-muted);
  }

  dd {
    margin: 0;
    font-weight: 600;
  }

  .mono,
  code {
    font-family: var(--font-mono);
    font-size: 13px;
  }

  p {
    margin: 0;
    color: var(--text-muted);
  }

  .foot {
    grid-column: 1 / -1;
    font-size: 12px;
    color: var(--text-faint);
  }
</style>
