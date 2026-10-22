<script lang="ts">
  type Theme = "system" | "light" | "dark";
  const ORDER: Theme[] = ["system", "light", "dark"];
  const LABELS: Record<Theme, string> = { system: "Match system", light: "Daylight", dark: "Den (dark)" };
  const KEY = "doghouse.theme";

  function load(): Theme {
    try {
      const saved = localStorage.getItem(KEY);
      if (saved === "light" || saved === "dark" || saved === "system") return saved;
    } catch {
      /* storage unavailable */
    }
    return "system";
  }

  let theme = $state<Theme>(load());

  $effect(() => {
    const root = document.documentElement;
    if (theme === "system") root.removeAttribute("data-theme");
    else root.setAttribute("data-theme", theme);
    try {
      localStorage.setItem(KEY, theme);
    } catch {
      /* storage unavailable */
    }
  });

  function next() {
    theme = ORDER[(ORDER.indexOf(theme) + 1) % ORDER.length]!;
  }
</script>

<button class="btn ghost small" onclick={next} title="Change theme">
  <svg width="15" height="15" viewBox="0 0 16 16" aria-hidden="true">
    {#if theme === "dark"}
      <path d="M13 10.5A6 6 0 0 1 5.5 3a6 6 0 1 0 7.5 7.5z" fill="currentColor" />
    {:else if theme === "light"}
      <circle cx="8" cy="8" r="3.2" fill="currentColor" />
      <path d="M8 1v2M8 13v2M1 8h2M13 8h2M3 3l1.4 1.4M11.6 11.6 13 13M3 13l1.4-1.4M11.6 4.4 13 3" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" />
    {:else}
      <circle cx="8" cy="8" r="6" fill="none" stroke="currentColor" stroke-width="1.4" />
      <path d="M8 2a6 6 0 0 1 0 12z" fill="currentColor" />
    {/if}
  </svg>
  {LABELS[theme]}
</button>
