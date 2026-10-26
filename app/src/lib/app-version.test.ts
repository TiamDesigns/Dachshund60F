import assert from "node:assert/strict";
import { test } from "node:test";

import { compareVersions, fetchNewestRelease, newestRelease, type GitHubRelease } from "./app-version.ts";

test("versions compare part by part, numerically", () => {
  assert.equal(compareVersions("0.1.2", "0.1.1"), 1);
  assert.equal(compareVersions("0.1.9", "0.1.10"), -1);
  assert.equal(compareVersions("v1.0.0", "1.0.0"), 0);
  assert.equal(compareVersions("1.0", "1.0.0"), 0);
  assert.equal(compareVersions("1.0.0-beta.1", "1.0.0"), -1);
  assert.equal(compareVersions("1.0.0", "1.0.0-beta.1"), 1);
});

const release = (tag: string, extra: Partial<GitHubRelease> = {}): GitHubRelease => ({
  tag_name: tag,
  html_url: `https://example.test/${tag}`,
  draft: false,
  prerelease: false,
  ...extra,
});

test("the newest Doghouse skips drafts, pre-releases and other tags", () => {
  const releases = [
    release("doghouse-v0.1.1"),
    release("doghouse-v0.1.9", { draft: true }),
    release("doghouse-v0.1.8", { prerelease: true }),
    release("firmware-v2.0.0"),
    release("doghouse-v0.1.10"),
    release("doghouse-v0.1.2"),
  ];
  assert.deepEqual(newestRelease(releases), { version: "0.1.10", url: "https://example.test/doghouse-v0.1.10" });
  assert.equal(newestRelease([release("firmware-v1.0.0")]), null);
});

test("fetching says what went wrong in words", async () => {
  const answer = (status: number, body: unknown = []) =>
    (async () => new Response(JSON.stringify(body), { status })) as unknown as typeof fetch;

  assert.deepEqual(await fetchNewestRelease(answer(200, [release("doghouse-v0.2.0")])), {
    version: "0.2.0",
    url: "https://example.test/doghouse-v0.2.0",
  });
  await assert.rejects(fetchNewestRelease(answer(403)), /limiting/);
  await assert.rejects(fetchNewestRelease(answer(500)), /GitHub answered 500/);
  const offline = (async () => {
    throw new TypeError("Failed to fetch");
  }) as unknown as typeof fetch;
  await assert.rejects(fetchNewestRelease(offline), /Couldn't reach GitHub/);
});
