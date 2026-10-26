/**
 * Battery charge as a colour: white when full, shading to yellow as it
 * drains, and red once it drops below 15%.
 */

export const LOW_BATTERY = 15;

const FULL = [255, 255, 255];
const LOW = [245, 196, 40];
const RED = "#e0402e";

export function batteryColor(percent: number): string {
  const p = Math.max(0, Math.min(100, percent));
  if (p < LOW_BATTERY) return RED;
  const t = (100 - p) / (100 - LOW_BATTERY);
  const hex = FULL.map((c, i) => Math.round(c + (LOW[i]! - c) * t).toString(16).padStart(2, "0")).join("");
  return `#${hex}`;
}
