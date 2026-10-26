<script lang="ts">
  import type { BatteryStatus } from "@dachshund60f/host-lib";

  import { batteryColor } from "../battery.ts";

  /** Battery level drawn as a bone that fills up. */
  let { battery, width = 52, showText = true }: { battery: BatteryStatus | null; width?: number; showText?: boolean } =
    $props();

  const clipId = $props.id();
  const percent = $derived(battery ? Math.max(0, Math.min(100, battery.percent)) : 0);
  const fillWidth = $derived(4 + (percent / 100) * 52);
  const tone = $derived(battery ? batteryColor(percent) : "var(--text-faint)");
  const label = $derived(
    !battery ? "Battery level unknown" : `Battery ${percent}%${battery.charging ? ", charging" : ""}`,
  );

  // Outline of a bone: shaft with two rounded knobs at each end.
  const BONE =
    "M10 4.5a4.5 4.5 0 0 0-4.4 5.5A4.5 4.5 0 0 0 10 15.5c1.6 0 3-.8 3.8-2h32.4c.8 1.2 2.2 2 3.8 2a4.5 4.5 0 0 0 4.4-5.5A4.5 4.5 0 0 0 50 4.5c-1.6 0-3 .8-3.8 2H13.8c-.8-1.2-2.2-2-3.8-2z";
</script>

<span class="battery" role="img" aria-label={label} title={label}>
  <svg width={width} height={width / 3} viewBox="0 0 60 20">
    <defs>
      <clipPath id={clipId}>
        <rect x="0" y="0" width={fillWidth} height="20" />
      </clipPath>
    </defs>
    <path d={BONE} fill="var(--surface-3)" />
    <path d={BONE} fill={tone} clip-path="url(#{clipId})" />
    <path d={BONE} fill="none" stroke="var(--border-strong)" stroke-width="1" />
    {#if battery?.charging}
      <path d="M31 5.5l-4 5h3l-1 4 4-5h-3z" fill="var(--surface)" stroke="var(--text)" stroke-width=".6" />
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
  }

  path {
    transition: fill 300ms;
  }

  .pct {
    font-weight: 600;
    font-size: 12px;
    color: var(--text-muted);
    min-width: 3ch;
  }
</style>
