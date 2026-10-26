<script lang="ts">
  /**
   * An "i" button beside a feature: what it does, and its pros and cons.
   * A span with the button role, not a <button>, so a surrounding <label>
   * still labels its own control (a button is labelable and would win).
   */
  import { FEATURE_INFO, type FeatureInfoId } from "../feature-info.ts";

  let { id }: { id: FeatureInfoId } = $props();

  const info = $derived(FEATURE_INFO[id]);
  const popId = `info-${Math.random().toString(36).slice(2, 9)}`;
  let open = $state(false);
  let root = $state<HTMLSpanElement>();

  function toggle(e: Event) {
    // Inside a label, a click would also flip its switch.
    e.preventDefault();
    e.stopPropagation();
    open = !open;
  }

  function key(e: KeyboardEvent) {
    if (e.key === "Enter" || e.key === " ") toggle(e);
  }

  $effect(() => {
    if (!open) return;
    const outside = (e: PointerEvent) => {
      if (root && !root.contains(e.target as Node)) open = false;
    };
    const escape = (e: KeyboardEvent) => {
      if (e.key === "Escape") open = false;
    };
    document.addEventListener("pointerdown", outside);
    document.addEventListener("keydown", escape);
    return () => {
      document.removeEventListener("pointerdown", outside);
      document.removeEventListener("keydown", escape);
    };
  });
</script>

<span class="info" bind:this={root}>
  <span
    class="i"
    class:open
    role="button"
    tabindex="0"
    aria-label="About {info.title}"
    aria-expanded={open}
    aria-controls={popId}
    onclick={toggle}
    onkeydown={key}
  >
    <svg width="14" height="14" viewBox="0 0 16 16" aria-hidden="true">
      <circle cx="8" cy="8" r="7" fill="none" stroke="currentColor" stroke-width="1.5" />
      <circle cx="8" cy="4.9" r="1" fill="currentColor" />
      <path d="M8 7.3v4.4" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" />
    </svg>
  </span>
  {#if open}
    <!-- Clicks here mustn't reach a surrounding label either. -->
    <!-- svelte-ignore a11y_click_events_have_key_events, a11y_no_noninteractive_element_interactions -->
    <span class="pop" id={popId} role="note" onclick={(e) => e.preventDefault()}>
      <strong class="title">{info.title}</strong>
      <span class="what">{info.what}</span>
      <span class="list pros">
        <span class="head">Good for</span>
        {#each info.pros as p (p)}<span class="item">{p}</span>{/each}
      </span>
      <span class="list cons">
        <span class="head">Watch out for</span>
        {#each info.cons as c (c)}<span class="item">{c}</span>{/each}
      </span>
    </span>
  {/if}
</span>

<style>
  .info {
    position: relative;
    display: inline-flex;
    vertical-align: middle;
    margin-left: 4px;
  }

  .i {
    display: inline-grid;
    place-items: center;
    width: 22px;
    height: 22px;
    border-radius: 50%;
    background: transparent;
    color: var(--text-faint);
    cursor: pointer;
  }

  .i:hover,
  .i.open,
  .i:focus-visible {
    outline: none;
    color: var(--accent);
    background: var(--accent-soft);
  }

  .pop {
    position: absolute;
    z-index: 30;
    top: calc(100% + 6px);
    left: -10px;
    width: min(340px, calc(100vw - 48px));
    display: flex;
    flex-direction: column;
    gap: 8px;
    padding: 12px 14px;
    border-radius: var(--radius);
    border: 1px solid var(--border);
    background: var(--surface);
    box-shadow: var(--shadow-lg);
    color: var(--text);
    font-size: 13px;
    font-weight: 400;
    line-height: 1.45;
    text-align: left;
    white-space: normal;
    cursor: auto;
  }

  .title {
    font-size: 14px;
  }

  .what {
    color: var(--text-muted);
  }

  .list {
    display: flex;
    flex-direction: column;
    gap: 3px;
  }

  .head {
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.06em;
    text-transform: uppercase;
  }

  .pros .head {
    color: var(--sage);
  }

  .cons .head {
    color: var(--accent);
  }

  .item {
    position: relative;
    padding-left: 14px;
  }

  .item::before {
    position: absolute;
    left: 0;
    font-weight: 700;
  }

  .pros .item::before {
    content: "+";
    color: var(--sage);
  }

  .cons .item::before {
    content: "−";
    color: var(--accent);
  }
</style>
