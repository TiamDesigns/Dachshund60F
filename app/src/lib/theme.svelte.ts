/** Light, dark or the system's choice, remembered in localStorage. */

export type Theme = "system" | "light" | "dark";
export const THEMES: Theme[] = ["system", "light", "dark"];
export const THEME_LABELS: Record<Theme, string> = { system: "Match system", light: "Daylight", dark: "Den (dark)" };
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

class ThemeChoice {
  current = $state<Theme>(load());

  set(theme: Theme): void {
    this.current = theme;
    const root = document.documentElement;
    if (theme === "system") root.removeAttribute("data-theme");
    else root.setAttribute("data-theme", theme);
    try {
      localStorage.setItem(KEY, theme);
    } catch {
      /* storage unavailable */
    }
  }

  next(): void {
    this.set(THEMES[(THEMES.indexOf(this.current) + 1) % THEMES.length]!);
  }
}

export const theme = new ThemeChoice();
theme.set(theme.current);
