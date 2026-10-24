<script lang="ts">
  import { app } from "./lib/app-state.svelte.ts";
  import AboutPanel from "./lib/components/AboutPanel.svelte";
  import AdvancedPanel from "./lib/components/AdvancedPanel.svelte";
  import BoardCard from "./lib/components/BoardCard.svelte";
  import BoardHeader from "./lib/components/BoardHeader.svelte";
  import Dachshund from "./lib/components/Dachshund.svelte";
  import KeymapEditor from "./lib/components/KeymapEditor.svelte";
  import Logo from "./lib/components/Logo.svelte";
  import MacrosPanel from "./lib/components/MacrosPanel.svelte";
  import MockPanel from "./lib/components/MockPanel.svelte";
  import SettingsPanel from "./lib/components/SettingsPanel.svelte";
  import ThemeToggle from "./lib/components/ThemeToggle.svelte";
  import Toasts from "./lib/components/Toasts.svelte";
  import UpdateDialog from "./lib/components/UpdateDialog.svelte";

  type Tab = "keys" | "advanced" | "macros" | "settings" | "about";

  let tab = $state<Tab>("keys");
  const session = $derived(app.selected);
  // Advanced and Macros only for boards whose firmware has them.
  const TABS = $derived(
    (
      [
        { id: "keys", label: "Keys", show: true },
        { id: "advanced", label: "Advanced", show: !!session?.advanced },
        { id: "macros", label: "Macros", show: !!session?.macros },
        { id: "settings", label: "Settings", show: true },
        { id: "about", label: "About", show: true },
      ] satisfies { id: Tab; label: string; show: boolean }[]
    ).filter((t) => t.show),
  );
  $effect(() => {
    if (!TABS.some((t) => t.id === tab)) tab = "keys";
  });
</script>

<div class="shell">
  <aside class="sidebar">
    <Logo />

    <h2 class="pack-title">
      Your pack
      <span class="count">{app.sessions.length}</span>
    </h2>

    <nav class="pack" aria-label="Connected boards">
      {#each app.sessions as s (s.id)}
        <BoardCard session={s} selected={s.id === app.selectedId} onselect={() => app.select(s.id)} />
      {:else}
        <div class="empty-pack">
          <Dachshund width={80} sleeping color="var(--border-strong)" ear="var(--text-faint)" />
          <p>The kennel is empty. Plug in a board or its dongle.</p>
        </div>
      {/each}
    </nav>

    {#if app.hid}
      <button class="btn primary connect" onclick={() => app.connect()}>Connect a board…</button>
    {:else}
      <p class="no-hid">This browser can't reach USB devices. Open Doghouse in Chrome or Edge to edit a real board; until then you're looking at demo boards.</p>
    {/if}

    <div class="sidebar-foot">
      {#if app.mock}
        <MockPanel mock={app.mock} selectedId={app.selectedId} />
      {/if}
      <ThemeToggle />
    </div>
  </aside>

  <main class="main">
    {#if session}
      {#key session.id}
        <BoardHeader {session} />

        <div class="tabs" role="tablist" aria-label="Board sections">
          {#each TABS as t (t.id)}
            <button
              role="tab"
              id="tab-{t.id}"
              aria-selected={tab === t.id}
              aria-controls="panel-{t.id}"
              class:active={tab === t.id}
              onclick={() => (tab = t.id)}
            >
              {t.label}
            </button>
          {/each}
        </div>

        <div class="panel" role="tabpanel" id="panel-{tab}" aria-labelledby="tab-{tab}">
          {#if session.problem}
            <div class="problem">
              <p>{session.problem}</p>
              {#if tab === "about"}<AboutPanel {session} />{/if}
            </div>
          {:else if !session.keymap}
            <p class="loading">Fetching…</p>
          {:else if tab === "keys"}
            <KeymapEditor {session} />
          {:else if tab === "advanced" && session.advanced}
            <AdvancedPanel {session} />
          {:else if tab === "macros" && session.macros}
            <MacrosPanel {session} />
          {:else if tab === "settings"}
            <SettingsPanel {session} />
          {:else}
            <AboutPanel {session} />
          {/if}
        </div>
      {/key}
    {:else}
      <div class="welcome">
        <Dachshund width={260} sleeping />
        <h1>Nobody's home yet</h1>
        <p>Connect a Doghouse board by USB, or plug in its 2.4 GHz dongle, and it will trot in here.</p>
        {#if app.hid}
          <p>The first time, click <strong>Connect a board…</strong> and pick it; after that Doghouse finds it on its own.</p>
        {/if}
        {#if !app.mock}
          <p><a href="?demo">Try it with demo boards</a></p>
        {/if}
      </div>
    {/if}
  </main>

  {#if app.update}
    <UpdateDialog update={app.update} onclose={() => app.closeUpdate()} />
  {/if}
  <Toasts toasts={app.toasts} />
</div>

<style>
  .shell {
    display: grid;
    grid-template-columns: 300px 1fr;
    height: 100%;
  }

  .sidebar {
    display: flex;
    flex-direction: column;
    gap: 18px;
    padding: 22px 16px 16px;
    background: var(--surface-2);
    border-right: 1px solid var(--border);
    min-height: 0;
  }

  .pack-title {
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 12px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: var(--text-muted);
    margin: 6px 4px -8px;
  }

  .count {
    padding: 0 7px;
    border-radius: 999px;
    background: var(--surface-3);
    color: var(--text);
    letter-spacing: 0;
  }

  .pack {
    display: flex;
    flex-direction: column;
    gap: 4px;
    overflow-y: auto;
    flex: 1;
    min-height: 0;
  }

  .connect {
    width: 100%;
  }

  .welcome a {
    color: var(--accent);
    font-weight: 600;
  }

  .welcome a:hover {
    color: var(--accent-hover);
  }

  .no-hid {
    font-size: 12px;
    color: var(--text-muted);
    margin: 0 4px;
  }

  .problem {
    display: flex;
    flex-direction: column;
    gap: 18px;
  }

  .problem > p {
    padding: 14px 18px;
    border-radius: var(--radius-lg);
    background: var(--surface);
    border: 1px solid var(--border-strong);
  }

  .empty-pack {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
    padding: 24px 12px;
    text-align: center;
    color: var(--text-muted);
  }

  .sidebar-foot {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }

  .main {
    overflow-y: auto;
    padding: 28px 36px 48px;
    min-width: 0;
  }

  .tabs {
    display: flex;
    gap: 4px;
    margin: 22px 0 18px;
    border-bottom: 1px solid var(--border);
  }

  .tabs button {
    padding: 9px 16px;
    border: 0;
    border-bottom: 3px solid transparent;
    background: transparent;
    font-weight: 600;
    color: var(--text-muted);
    cursor: pointer;
    margin-bottom: -1px;
  }

  .tabs button:hover {
    color: var(--text);
  }

  .tabs button.active {
    color: var(--text);
    border-bottom-color: var(--accent);
  }

  .loading {
    color: var(--text-muted);
  }

  .welcome {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 12px;
    height: 100%;
    text-align: center;
    color: var(--text-muted);
  }

  .welcome h1 {
    color: var(--text);
    font-size: 26px;
  }

  @media (max-width: 760px) {
    .shell {
      grid-template-columns: 1fr;
      grid-template-rows: auto 1fr;
    }

    .sidebar {
      border-right: 0;
      border-bottom: 1px solid var(--border);
    }

    .main {
      padding: 20px 16px 40px;
    }
  }
</style>
