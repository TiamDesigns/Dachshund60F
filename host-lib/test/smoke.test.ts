import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

import { HOST_LIB_VERSION } from "../src/index.js";

test("HOST_LIB_VERSION matches package.json", () => {
  const pkg = JSON.parse(
    readFileSync(new URL("../../package.json", import.meta.url), "utf8"),
  ) as { version: string };
  assert.equal(HOST_LIB_VERSION, pkg.version);
});
