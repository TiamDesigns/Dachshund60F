<script lang="ts">
  import { THEME_LABELS, theme } from "../theme.svelte.ts";
</script>

<button
  class="btn ghost small"
  onclick={() => theme.next()}
  title="Change theme"
  aria-label="Theme: {THEME_LABELS[theme.current]}. Change theme"
>
  <svg width="15" height="15" viewBox="0 0 16 16" aria-hidden="true">
    {#if theme.current === "dark"}
      <path d="M13 10.5A6 6 0 0 1 5.5 3a6 6 0 1 0 7.5 7.5z" fill="currentColor" />
    {:else if theme.current === "light"}
      <circle cx="8" cy="8" r="3.2" fill="currentColor" />
      <path d="M8 1v2M8 13v2M1 8h2M13 8h2M3 3l1.4 1.4M11.6 11.6 13 13M3 13l1.4-1.4M11.6 4.4 13 3" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" />
    {:else}
      <circle cx="8" cy="8" r="6" fill="none" stroke="currentColor" stroke-width="1.4" />
      <path d="M8 2a6 6 0 0 1 0 12z" fill="currentColor" />
    {/if}
  </svg>
  {THEME_LABELS[theme.current]}
</button>
