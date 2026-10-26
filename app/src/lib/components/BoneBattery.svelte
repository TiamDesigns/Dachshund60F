<script lang="ts">
  import type { BatteryStatus } from "@dachshund60f/host-lib";

  import { batteryColor } from "../battery.ts";

  /** Battery level drawn as a dog bone that fills up from the left. */
  let { battery, width = 52, showText = true }: { battery: BatteryStatus | null; width?: number; showText?: boolean } =
    $props();

  const clipId = $props.id();
  const percent = $derived(battery ? Math.max(0, Math.min(100, battery.percent)) : 0);
  // The fill runs from the left knobs' outer edge to the right ones'.
  const fillWidth = $derived(1 + (percent / 100) * 46);
  const tone = $derived(battery ? batteryColor(percent) : "var(--text-faint)");
  const label = $derived(
    !battery ? "Battery level unknown" : `Battery ${percent}%${battery.charging ? ", charging" : ""}`,
  );
</script>

<!-- A bone: two round knobs at each end and a thick shaft between them. -->
{#snippet bone()}
  <circle cx="6" cy="6" r="5" />
  <circle cx="6" cy="16" r="5" />
  <circle cx="42" cy="6" r="5" />
  <circle cx="42" cy="16" r="5" />
  <rect x="6" y="5" width="36" height="12" />
{/snippet}

<span class="battery" role="img" aria-label={label} title={label}>
  <svg width={width} height={(width * 22) / 48} viewBox="0 0 48 22" aria-hidden="true">
    <defs>
      <clipPath id={clipId}>
        <rect x="0" y="0" width={fillWidth} height="22" />
      </clipPath>
    </defs>
    <!-- The outline: the shapes stroked, then filled over, leaves only the bone's outer edge. -->
    <g fill="none" stroke="var(--bone-edge)" stroke-width="2">{@render bone()}</g>
    <g fill="var(--surface-3)">{@render bone()}</g>
    <g class="fill" fill={tone} clip-path="url(#{clipId})">{@render bone()}</g>
    {#if battery?.charging}
      <path d="M25.5 4.5l-5 7h3.5l-1.5 6 5-7.5h-3.5z" fill="var(--text)" stroke="var(--surface)" stroke-width=".8" stroke-linejoin="round" />
    {/if}
  </svg>
  {#if showText}
    <span class="pct">{battery ? `${percent}%` : "–"}</span>
  {/if}
</span>

<style>
  .battery {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    font-variant-numeric: tabular-nums;
  }

  svg {
    display: block;
    overflow: visible;
  }

  .fill {
    transition: fill 300ms;
  }

  .pct {
    font-weight: 600;
    font-size: 12px;
    color: var(--text-muted);
    min-width: 3ch;
  }
</style>
