<script lang="ts">
  /** Side-on dachshund silhouette; the board avatar. */
  let {
    width = 96,
    color = "var(--dog)",
    ear = "var(--dog-ear)",
    sleeping = false,
  }: { width?: number; color?: string; ear?: string; sleeping?: boolean } = $props();
</script>

<svg {width} height={width / 2} viewBox="0 0 128 64" aria-hidden="true" class:sleeping>
  <path d="M26 31C17 27 11 21 7 12" fill="none" stroke={color} stroke-width="5" stroke-linecap="round" />
  <rect x="38" y="40" width="7" height="14" rx="3" fill={ear} />
  <rect x="70" y="40" width="7" height="14" rx="3" fill={ear} />
  <rect x="20" y="24" width="76" height="22" rx="11" fill={color} />
  <rect x="27" y="40" width="8" height="16" rx="3" fill={color} />
  <rect x="80" y="40" width="8" height="16" rx="3" fill={color} />
  <path
    d="M82 32C86 19 93 12 104 12c8 0 14 5 19 9 3 2 2 7-2 7l-13 2c-8 2-14 6-18 10z"
    fill={color}
  />
  <path d="M99 14c-6 3-8 13-4 21 5 0 8-9 9-17z" fill={ear} />
  {#if sleeping}
    <path d="M107 19q3 2 6 0" fill="none" stroke="var(--surface)" stroke-width="1.6" stroke-linecap="round" />
  {:else}
    <circle cx="110" cy="18.5" r="1.9" fill="var(--surface)" />
  {/if}
  <circle cx="123" cy="23.5" r="2.3" fill={ear} />
</svg>

<style>
  svg {
    display: block;
    overflow: visible;
  }
</style>
