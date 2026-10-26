/**
 * Which Doghouse is newest, from the repo's GitHub releases. The desktop
 * app's updater does the installing; this is how Doghouse still finds a new
 * version when the updater can't (a build without the update key, or an
 * update server that answers with something the updater rejects).
 */

export const APP_REPO = "TiamDesigns/Dachshund60F";
/** Where people download Doghouse by hand. */
export const RELEASES_URL = `https://github.com/${APP_REPO}/releases/latest`;
const RELEASES_API = `https://api.github.com/repos/${APP_REPO}/releases?per_page=30`;
const TAG_PREFIX = "doghouse-v";

/** A published Doghouse release. */
export interface AppRelease {
  version: string;
  url: string;
}

/** The parts of a GitHub release this needs. */
export interface GitHubRelease {
  tag_name: string;
  html_url: string;
  draft: boolean;
  prerelease: boolean;
}

/**
 * Compare two versions like 0.1.2 or v1.0.0-beta.1: negative when a is
 * older, positive when newer, 0 when the same. A pre-release is older than
 * its release.
 */
export function compareVersions(a: string, b: string): number {
  const parse = (v: string) => {
    const [core = "", pre] = v.trim().replace(/^v/, "").split("-", 2);
    return { parts: core.split(".").map((n) => Number.parseInt(n, 10) || 0), pre };
  };
  const x = parse(a);
  const y = parse(b);
  for (let i = 0; i < Math.max(x.parts.length, y.parts.length); i++) {
    const d = (x.parts[i] ?? 0) - (y.parts[i] ?? 0);
    if (d) return Math.sign(d);
  }
  if (x.pre === y.pre) return 0;
  if (x.pre === undefined) return 1;
  if (y.pre === undefined) return -1;
  return x.pre < y.pre ? -1 : 1;
}

/** The newest published Doghouse among the repo's releases (which also has other kinds). */
export function newestRelease(releases: readonly GitHubRelease[]): AppRelease | null {
  let best: AppRelease | null = null;
  for (const r of releases) {
    if (r.draft || r.prerelease || !r.tag_name.startsWith(TAG_PREFIX)) continue;
    const version = r.tag_name.slice(TAG_PREFIX.length);
    if (!best || compareVersions(version, best.version) > 0) best = { version, url: r.html_url };
  }
  return best;
}

/** Ask GitHub for the newest Doghouse. Throws an Error worded for people when it can't. */
export async function fetchNewestRelease(fetchFn: typeof fetch = fetch): Promise<AppRelease | null> {
  let response: Response;
  try {
    response = await fetchFn(RELEASES_API, { headers: { Accept: "application/vnd.github+json" } });
  } catch {
    throw new Error("Couldn't reach GitHub. Check the internet connection and try again.");
  }
  if (response.status === 403 || response.status === 429) {
    throw new Error("GitHub is limiting how often this computer can check. Try again in an hour.");
  }
  if (!response.ok) throw new Error(`GitHub answered ${response.status} ${response.statusText}`.trim() + ".");
  return newestRelease((await response.json()) as GitHubRelease[]);
}
