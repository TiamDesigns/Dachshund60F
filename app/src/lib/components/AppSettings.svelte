<script lang="ts">
  /** Doghouse's own settings (not a board's): theme, version and updates. */
  import { HOST_LIB_VERSION, protocol } from "@dachshund60f/host-lib";

  import { app } from "../app-state.svelte.ts";
  import { RELEASES_URL } from "../app-version.ts";
  import { THEME_LABELS, THEMES, theme } from "../theme.svelte.ts";

  let { onclose }: { onclose: () => void } = $props();

  let dialog = $state<HTMLDialogElement>();
  $effect(() => {
    dialog?.showModal();
  });

  const status = $derived(app.appUpdate);
  const newer = $derived(app.newerApp);

  function time(at: Date): string {
    return at.toLocaleTimeString(undefined, { hour: "numeric", minute: "2-digit" });
  }

  function backdrop(e: MouseEvent) {
    // A click on the dialog's own box (outside its content) is the backdrop.
    if (e.target === dialog) dialog?.close();
  }
</script>

<dialog bind:this={dialog} aria-labelledby="app-settings-title" onclose={onclose} onclick={backdrop}>
  <div class="content">
    <header>
      <h2 id="app-settings-title">Doghouse settings</h2>
      <button class="btn ghost small" onclick={() => dialog?.close()} aria-label="Close">✕</button>
    </header>

    <section aria-labelledby="look-title">
      <h3 id="look-title">Look</h3>
      <div class="themes" role="radiogroup" aria-labelledby="look-title">
        {#each THEMES as t (t)}
          <label class="theme" class:on={theme.current === t}>
            <input type="radio" name="theme" value={t} checked={theme.current === t} onchange={() => theme.set(t)} />
            {THEME_LABELS[t]}
          </label>
        {/each}
      </div>
    </section>

    <section aria-labelledby="version-title">
      <h3 id="version-title">Version</h3>
      <dl>
        <dt>Doghouse</dt>
        <dd><strong class="mono">{__DOGHOUSE_VERSION__}</strong> {app.desktop ? "(desktop app)" : "(in the browser)"}</dd>
        <dt>Host library</dt>
        <dd class="mono">{HOST_LIB_VERSION}</dd>
        <dt>Config protocol</dt>
        <dd class="mono">v{protocol.PROTOCOL_VERSION}</dd>
      </dl>
    </section>

    <section aria-labelledby="updates-title">
      <h3 id="updates-title">Updates</h3>
      {#if app.desktop}
        <div class="status" role="status" aria-live="polite">
          {#if status.state === "idle"}
            <p>Doghouse looks for a newer version when it starts and every few hours.</p>
          {:else if status.state === "checking"}
            <p>Looking for a newer Doghouse…</p>
          {:else if status.state === "failed"}
            <p class="bad"><strong>Couldn't check for updates.</strong> {status.error}</p>
            <p class="when">Tried at {time(status.at)}.</p>
          {:else if newer}
            <p class="good"><strong>Doghouse {newer.kind === "install" ? newer.update.version : newer.version} is out.</strong></p>
            {#if newer.kind === "download"}<p>{newer.reason}</p>{/if}
            <p class="when">Checked at {time(status.at)}.</p>
          {:else}
            <p><strong>You have the newest Doghouse.</strong></p>
            <p class="when">Checked at {time(status.at)}.</p>
          {/if}
        </div>
        <div class="buttons">
          {#if newer}
            <button class="btn primary" disabled={app.appUpdating !== false} onclick={() => app.installAppUpdate()}>
              {#if newer.kind === "download"}
                Download from GitHub
              {:else if app.appUpdating === false}
                Install and restart
              {:else if app.appUpdating === null}
                Installing…
              {:else}
                Downloading {Math.round(app.appUpdating * 100)}%
              {/if}
            </button>
          {/if}
          <button class={newer ? "btn" : "btn primary"} disabled={status.state === "checking" || app.appUpdating !== false} onclick={() => app.checkForAppUpdate()}>
            {status.state === "checking" ? "Checking…" : "Check for updates"}
          </button>
        </div>
      {:else}
        <p>In a browser, Doghouse is the version this page serves; reload to get the newest. The desktop app is on <a href={RELEASES_URL} target="_blank" rel="noreferrer">GitHub</a>.</p>
      {/if}
    </section>
  </div>
</dialog>

<style>
  dialog {
    width: min(460px, calc(100vw - 32px));
    padding: 0;
    border: 1px solid var(--border);
    border-radius: var(--radius-lg);
    background: var(--surface);
    color: var(--text);
    box-shadow: var(--shadow-lg);
  }

  dialog::backdrop {
    background: rgb(28 21 17 / 0.45);
  }

  .content {
    display: flex;
    flex-direction: column;
    gap: 20px;
    padding: 20px 22px 22px;
  }

  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
  }

  h2 {
    font-size: 20px;
  }

  section {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }

  h3 {
    font-size: 12px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: var(--text-muted);
  }

  p {
    margin: 0;
  }

  .themes {
    display: flex;
    gap: 6px;
    flex-wrap: wrap;
  }

  .theme {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 7px 14px;
    border-radius: 999px;
    border: 1px solid var(--border-strong);
    font-weight: 600;
    cursor: pointer;
  }

  .theme.on {
    border-color: var(--accent);
    background: var(--accent-soft);
  }

  .theme input {
    accent-color: var(--accent);
    margin: 0;
  }

  .theme:has(input:focus-visible) {
    outline: 2px solid var(--accent);
    outline-offset: 2px;
  }

  dl {
    display: grid;
    grid-template-columns: max-content 1fr;
    gap: 6px 16px;
    margin: 0;
  }

  dt {
    color: var(--text-muted);
  }

  dd {
    margin: 0;
  }

  .mono {
    font-family: var(--font-mono);
  }

  .status {
    display: flex;
    flex-direction: column;
    gap: 4px;
    padding: 12px 14px;
    border-radius: var(--radius);
    background: var(--surface-2);
  }

  .good strong {
    color: var(--sage);
  }

  .bad strong {
    color: var(--danger);
  }

  .when {
    font-size: 12px;
    color: var(--text-muted);
  }

  .buttons {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
  }

  a {
    color: var(--accent);
    font-weight: 600;
  }
</style>
