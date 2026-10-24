<script lang="ts">
  import {
    APP_FLASH,
    encodeFirmwareInfo,
    HOST_LIB_VERSION,
    makeUf2,
    protocol,
    type MockBoard,
  } from "@dachshund60f/host-lib";

  import { app, BREEDS, type BoardSession } from "../app-state.svelte.ts";

  let { session }: { session: BoardSession } = $props();

  const info = $derived(session.board.info);
  const CONNECTIONS = { usb: "USB cable", dongle: "2.4 GHz dongle", ble: "Bluetooth" } as const;

  let fileInput = $state<HTMLInputElement>();
  let backupInput = $state<HTMLInputElement>();

  function pickedFirmware(e: Event & { currentTarget: HTMLInputElement }) {
    const file = e.currentTarget.files?.[0];
    e.currentTarget.value = "";
    if (file) void app.prepareUpdate(session, file);
  }

  function pickedBackup(e: Event & { currentTarget: HTMLInputElement }) {
    const file = e.currentTarget.files?.[0];
    e.currentTarget.value = "";
    if (file && confirm(`Replace ${session.displayName}'s keymap and settings with ${file.name}? This saves them to the board.`)) {
      void session.restoreBackup(file);
    }
  }

  // Demo boards: a made-up firmware file one minor version up, to try the update flow.
  const demoBoard = $derived(app.mock?.list().some((b: MockBoard) => b === session.board) ?? false);

  function pretendUpdate() {
    const [major, minor] = info.firmwareVersion.split(/[.-]/).map(Number);
    const version = `${major ?? 0}.${(minor ?? 0) + 1}.0`;
    const image = new Uint8Array(8192).fill(0xff);
    image.set(encodeFirmwareInfo({ protocolVersion: protocol.PROTOCOL_VERSION, version, model: info.model }), 1024);
    const file = new File([makeUf2(image, APP_FLASH.start) as Uint8Array<ArrayBuffer>], `dachshund60f-${version}.uf2`);
    void app.prepareUpdate(session, file);
  }

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
    {#if session.board.reboot && !session.problem && session.definition}
      <p>Pick a firmware <code>.uf2</code> file. Doghouse checks it's for this board, backs up the settings, installs it and puts anything it lost back.</p>
      <input bind:this={fileInput} type="file" accept=".uf2" hidden onchange={pickedFirmware} />
      <div class="buttons">
        <button class="btn primary" onclick={() => fileInput?.click()} disabled={session.busy || !!app.update}>Update firmware…</button>
        {#if demoBoard}
          <button class="btn" onclick={pretendUpdate} disabled={session.busy || !!app.update}>Try a pretend update</button>
        {:else}
          <button class="btn ghost" onclick={enterBootloader} disabled={session.busy}>Just restart into bootloader…</button>
        {/if}
      </div>
    {:else}
      <p>Hold Esc for 3 seconds or double-tap reset, then copy the <code>.uf2</code> file onto the drive that appears. The board restarts with the new firmware.</p>
    {/if}
  </section>

  {#if !session.problem && session.supports("persistence")}
    <section class="card">
      <h2>Backups</h2>
      <p>Doghouse backs up a board before each update. Restore one here (a <code>.json</code> backup file); only what differs is written, then saved.</p>
      <input bind:this={backupInput} type="file" accept=".json,application/json" hidden onchange={pickedBackup} />
      <div class="buttons">
        <button class="btn" onclick={() => backupInput?.click()} disabled={session.busy}>Restore a backup…</button>
      </div>
    </section>
  {/if}

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

  .buttons {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
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
