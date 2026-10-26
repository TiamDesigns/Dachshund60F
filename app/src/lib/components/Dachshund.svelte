<script lang="ts">
  /** Side-on dachshund silhouette; the board avatar. Same coat in light and dark themes. */
  let {
    width = 96,
    color = "var(--dog)",
    ear = "var(--dog-ear)",
    sleeping = false,
  }: { width?: number; color?: string; ear?: string; sleeping?: boolean } = $props();

  // The far legs sit in the body's shadow: always a shade darker than the coat, whatever it is.
  const shadow = $derived(`color-mix(in srgb, ${color} 62%, #1e120a)`);
</script>

<svg {width} height={width / 2} viewBox="0 0 128 64" aria-hidden="true" class:sleeping>
  <path d="M26 31C17 27 11 21 7 12" fill="none" stroke={color} stroke-width="5" stroke-linecap="round" />
  <rect x="38" y="40" width="7" height="14" rx="3" fill={shadow} />
  <rect x="70" y="40" width="7" height="14" rx="3" fill={shadow} />
  <rect x="20" y="24" width="76" height="22" rx="11" fill={color} />
  <rect x="27" y="40" width="8" height="16" rx="3" fill={color} />
  <rect x="80" y="40" width="8" height="16" rx="3" fill={color} />
  <path
    d="M82 32C86 19 93 12 104 12c8 0 14 5 19 9 3 2 2 7-2 7l-13 2c-8 2-14 6-18 10z"
    fill={color}
  />
  <path d="M99 14c-6 3-8 13-4 21 5 0 8-9 9-17z" fill={ear} />
  {#if sleeping}
    <path d="M107.4 18.6q2.6 2.2 5.2 0" fill="none" stroke="#2a1a10" stroke-width="1.5" stroke-linecap="round" />
  {:else}
    <ellipse cx="110.4" cy="18.4" rx="2.9" ry="2.6" fill="#fffaf3" />
    <circle cx="111.1" cy="18.6" r="1.7" fill="#2a1a10" />
    <circle cx="111.7" cy="17.8" r="0.55" fill="#fffaf3" />
  {/if}
  <circle cx="123" cy="23.5" r="2.3" fill={ear} />
</svg>

<style>
  svg {
    display: block;
    overflow: visible;
  }
</style>
