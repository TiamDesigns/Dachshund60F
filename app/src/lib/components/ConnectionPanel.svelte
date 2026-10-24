<script lang="ts">
  import { BoardError, type ConnectionStatus, type DonglePairing, type DongleStatus, type Output } from "@dachshund60f/host-lib";

  import { app, type BoardSession } from "../app-state.svelte.ts";

  let { session }: { session: BoardSession } = $props();

  /** How often to look again while the panel is open: pairing happens on the computer's side. */
  const POLL_MS = 2000;

  let status = $state<ConnectionStatus | null>(null);
  let dongle = $state<DongleStatus | null>(null);
  let working = $state(false);

  const board = $derived(session.board);
  const overBluetooth = $derived(board.info.connection === "ble");
  const overDongle = $derived(board.info.connection === "dongle");
  const hasDongle = $derived(!!board.getDongle && session.supports("dongle"));

  async function refresh(): Promise<void> {
    try {
      status = (await board.getConnection?.()) ?? null;
      dongle = hasDongle ? ((await board.getDongle?.()) ?? null) : null;
    } catch (err) {
      // The board went away (it shows up again by itself); anything else is worth a word.
      if (!(err instanceof BoardError && (err.code === "disconnected" || err.code === "unreachable"))) {
        app.notify(text(err), "error");
      }
    }
  }

  $effect(() => {
    void board;
    void refresh();
    const timer = setInterval(() => void refresh(), POLL_MS);
    return () => clearInterval(timer);
  });

  function text(err: unknown): string {
    return err instanceof Error ? err.message : String(err);
  }

  async function act(fn: () => Promise<void>, done?: string): Promise<void> {
    working = true;
    try {
      await fn();
      if (done) app.notify(done, "success");
    } catch (err) {
      app.notify(text(err), "error");
    } finally {
      working = false;
      await refresh();
    }
  }

  function prefer(output: Output) {
    void act(() => board.setOutput!(output));
  }

  const PREFER: { output: Output; label: string }[] = $derived([
    { output: "usb", label: "USB" },
    ...(hasDongle ? [{ output: "dongle" as const, label: "2.4 GHz" }] : []),
    { output: "ble", label: "Bluetooth" },
  ]);

  function pair(action: DonglePairing) {
    if (action === "forget") {
      const question = overDongle
        ? `Forget the dongle? It's how Doghouse reaches ${session.displayName} right now, so the connection drops until you pair them again.`
        : `Forget the dongle? ${session.displayName} stops sending keys to it until you pair them again.`;
      if (!confirm(question)) return;
    }
    const done = {
      start: `${session.displayName} is looking for a dongle. Press the dongle's button now.`,
      stop: undefined,
      forget: "Dongle forgotten.",
    }[action];
    void act(() => board.pairDongle!(action), done);
  }

  function dongleState(d: DongleStatus): string {
    if (d.pairing) return "Looking for a dongle in pairing mode…";
    if (!d.paired) return "No dongle paired";
    switch (d.link) {
      case "connected":
        return "Connected";
      case "in-range":
        return "In range, waiting for its computer";
      case "searching":
        return "Paired, but the dongle isn't answering (unplugged or out of range)";
      default:
        return "Radio asleep";
    }
  }

  function select(profile: number) {
    if (overBluetooth && profile !== status?.profile) {
      if (!confirm(`Doghouse reaches ${session.displayName} over Bluetooth host ${status!.profile + 1}. Switching drops this connection. Switch anyway?`)) return;
    }
    void act(() => board.selectProfile!(profile), `${session.displayName} is now on host ${profile + 1}.`);
  }

  function forget(profile: number) {
    const self = overBluetooth && profile === status?.profile;
    const question = self
      ? `Forget host ${profile + 1}? It's the computer Doghouse is using right now, so the connection drops, and the computer needs to remove ${session.displayName} from its Bluetooth list before pairing again.`
      : `Forget the computer paired on host ${profile + 1}? Remove ${session.displayName} from that computer's Bluetooth list too, then pair it again if you like.`;
    if (!confirm(question)) return;
    void act(() => board.clearProfile!(profile), `Host ${profile + 1} is free for a new computer.`);
  }

  const OUTPUT_NAMES = { usb: "USB", ble: "Bluetooth", dongle: "the 2.4 GHz dongle" } as const;

  function hostState(i: number, s: ConnectionStatus): string {
    if (i !== s.profile) return s.paired[i] ? "Paired" : "Free";
    switch (s.bluetooth) {
      case "connected":
        return "Connected";
      case "pairing":
        return "Pairing…";
      case "advertising":
        return s.paired[i] ? "Waiting for its computer" : "Ready to pair";
      default:
        return "Bluetooth off";
    }
  }

  const where = $derived.by(() => {
    if (!status) return "";
    if (!status.active) return "Nowhere yet: no computer is listening.";
    if (status.active === "ble") return `Bluetooth host ${status.profile + 1}`;
    return OUTPUT_NAMES[status.active];
  });
</script>

<div class="connection">
  {#if !status}
    <p class="muted">Asking {session.displayName} how it's connected…</p>
  {:else}
    <section class="card">
      <h2>Where keys go</h2>
      <p class="now"><span class="dot" class:on={!!status.active}></span>{where}</p>
      <fieldset class="segmented" disabled={working}>
        <legend>Prefer</legend>
        {#each PREFER as p (p.output)}
          <label class:checked={status.preferred === p.output}>
            <input type="radio" name="prefer" checked={status.preferred === p.output} onchange={() => prefer(p.output)} />
            {p.label}
          </label>
        {/each}
      </fieldset>
      <p class="muted">
        {#if hasDongle}
          Keys go to the one you prefer while its computer is there. Otherwise they go to the first one ready of USB, the dongle and
          Bluetooth, so unplugging the cable carries on wirelessly. The <kbd>USB</kbd>, <kbd>2.4G</kbd> and <kbd>BT</kbd> keys do the same.
        {:else}
          Keys go to the one you prefer while its computer is there, and to the other one otherwise, so unplugging the cable carries on over
          Bluetooth. The <kbd>USB</kbd> and <kbd>BT</kbd> keys do the same.
        {/if}
        The board remembers this by itself.
      </p>
    </section>

    {#if dongle}
      <section class="card">
        <h2>2.4 GHz dongle</h2>
        <p class="now"><span class="dot" class:on={dongle.link === "connected"}></span>{dongleState(dongle)}</p>
        <div class="buttons">
          {#if dongle.pairing}
            <button class="btn small" onclick={() => pair("stop")} disabled={working}>Stop looking</button>
          {:else}
            <button class="btn small" onclick={() => pair("start")} disabled={working}>
              {dongle.paired ? "Pair another dongle" : "Pair a dongle"}
            </button>
          {/if}
          {#if dongle.paired}
            <button class="btn small ghost" onclick={() => pair("forget")} disabled={working}>Forget</button>
          {/if}
        </div>
        <p class="muted">
          To pair, plug the dongle in and press its button (it blinks blue), then press <strong>Pair a dongle</strong> or the
          <kbd>2.4G Pair</kbd> key within 30 seconds. The dongle blinks green when they find each other.
        </p>
      </section>
    {/if}

    <section class="card">
      <h2>Bluetooth hosts</h2>
      <ul class="hosts">
        {#each status.paired as paired, i (i)}
          {@const selected = i === status.profile}
          <li class:selected>
            <div class="host">
              <strong>Host {i + 1}</strong>
              <span class="state" class:live={selected && status.bluetooth === "connected"}>{hostState(i, status)}</span>
            </div>
            <div class="buttons">
              {#if !selected}
                <button class="btn small" onclick={() => select(i)} disabled={working}>Switch to</button>
              {/if}
              {#if paired}
                <button class="btn small ghost" onclick={() => forget(i)} disabled={working}>Forget</button>
              {/if}
            </div>
          </li>
        {/each}
      </ul>
      <p class="muted">
        To pair a computer, switch to a free host, then pick <strong>{board.info.model}</strong> in the computer's Bluetooth settings. Each host
        keeps one computer; <kbd>BT1</kbd>–<kbd>BT{status.paired.length}</kbd> switch between them and <kbd>BT Clr</kbd> forgets the current one.
      </p>
    </section>
  {/if}
</div>

<style>
  .connection {
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
    display: flex;
    flex-direction: column;
    gap: 12px;
  }

  h2 {
    font-size: 15px;
    margin: 0;
  }

  p {
    margin: 0;
  }

  .muted {
    color: var(--text-muted);
    font-size: 13px;
  }

  .now {
    display: flex;
    align-items: center;
    gap: 8px;
    font-weight: 600;
    font-size: 16px;
  }

  .dot {
    width: 10px;
    height: 10px;
    border-radius: 50%;
    background: var(--border-strong);
  }

  .dot.on {
    background: var(--sage);
  }

  .segmented {
    display: inline-flex;
    align-self: flex-start;
    margin: 0;
    padding: 3px;
    border: 1px solid var(--border);
    border-radius: 999px;
    background: var(--surface-2);
  }

  .segmented legend {
    position: absolute;
    width: 1px;
    height: 1px;
    overflow: hidden;
    clip-path: inset(50%);
  }

  .segmented label {
    padding: 5px 14px;
    border-radius: 999px;
    cursor: pointer;
    font-weight: 600;
    color: var(--text-muted);
  }

  .segmented label.checked {
    background: var(--surface);
    color: var(--text);
    box-shadow: 0 1px 2px rgb(0 0 0 / 0.12);
  }

  .segmented label:focus-within {
    outline: 2px solid var(--accent);
    outline-offset: 1px;
  }

  .segmented input {
    position: absolute;
    opacity: 0;
    pointer-events: none;
  }

  .hosts {
    list-style: none;
    margin: 0;
    padding: 0;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  .hosts li {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
    flex-wrap: wrap;
    padding: 10px 12px;
    border-radius: var(--radius-sm);
    border: 1px solid var(--border);
  }

  .hosts li.selected {
    border-color: var(--accent);
    background: var(--accent-soft);
  }

  .host {
    display: flex;
    flex-direction: column;
  }

  .state {
    font-size: 12px;
    color: var(--text-muted);
  }

  .state.live {
    color: var(--sage);
    font-weight: 600;
  }

  .buttons {
    display: flex;
    gap: 6px;
  }

  kbd {
    font-family: var(--font-mono);
    font-size: 12px;
    padding: 0 4px;
    border: 1px solid var(--border);
    border-radius: 4px;
    background: var(--surface-2);
  }
</style>
