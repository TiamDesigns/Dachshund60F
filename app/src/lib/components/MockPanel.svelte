<script lang="ts">
  import type { MockBoard, MockBoardManager } from "@dachshund60f/host-lib";

  /** Controls for the simulated boards, shown in demo mode. */
  let { mock, selectedId }: { mock: MockBoardManager; selectedId: string | null } = $props();

  const NAMES = ["Pretzel", "Frank", "Waffles", "Maple", "Schnitzel", "Hazel", "Mochi", "Peanut"];
  let open = $state(false);
  let battery = $state(60);
  let charging = $state(false);

  const selected = $derived(mock.list().find((b) => b.info.id === selectedId) as MockBoard | undefined);

  function plug(connection: "usb" | "dongle") {
    const name = NAMES[Math.floor(Math.random() * NAMES.length)]!;
    mock.plug("Dachshund60F", connection, {
      name,
      battery: { percent: 40 + Math.floor(Math.random() * 60), charging: connection === "usb" },
    });
  }

  function unplug() {
    if (selected) mock.unplug(selected.info.id);
  }

  function setBattery() {
    selected?.setBattery({ percent: battery, charging });
  }
</script>

<div class="mock">
  <button class="toggle" onclick={() => (open = !open)} aria-expanded={open}>
    <span class="badge">Simulated</span>
    Demo mode: these boards are pretend
    <span class="chev" class:open>▸</span>
  </button>

  {#if open}
    <div class="controls">
      <div class="row">
        <button class="btn small" onclick={() => plug("usb")}>Plug in via USB</button>
        <button class="btn small" onclick={() => plug("dongle")}>Plug in via dongle</button>
      </div>
      {#if selected}
        <div class="row">
          <button class="btn small" onclick={unplug}>Unplug {selected.info.id}</button>
        </div>
        <label class="field">
          Battery {battery}%
          <input type="range" min="0" max="100" bind:value={battery} onchange={setBattery} />
        </label>
        <label class="check">
          <input type="checkbox" bind:checked={charging} onchange={setBattery} /> Charging
        </label>
      {/if}
    </div>
  {/if}
</div>

<style>
  .mock {
    border: 1px dashed var(--border-strong);
    border-radius: var(--radius);
    background: var(--surface-2);
    font-size: 12px;
  }

  .toggle {
    display: flex;
    align-items: center;
    gap: 8px;
    width: 100%;
    padding: 8px 10px;
    border: 0;
    background: transparent;
    cursor: pointer;
    text-align: left;
    color: var(--text-muted);
  }

  .badge {
    padding: 1px 7px;
    border-radius: 999px;
    background: var(--ochre-soft);
    color: var(--ochre);
    font-weight: 700;
  }

  .chev {
    margin-left: auto;
    transition: transform 120ms;
  }

  .chev.open {
    transform: rotate(90deg);
  }

  .controls {
    display: flex;
    flex-direction: column;
    gap: 8px;
    padding: 0 10px 10px;
  }

  .row {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
  }

  .field {
    display: flex;
    flex-direction: column;
    gap: 2px;
    color: var(--text-muted);
  }

  .check {
    display: flex;
    gap: 6px;
    align-items: center;
    color: var(--text-muted);
  }

  input[type="range"] {
    accent-color: var(--accent);
  }
</style>
