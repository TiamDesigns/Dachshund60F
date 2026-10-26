import { test } from "node:test";
import assert from "node:assert/strict";

import { SerialQueue } from "./serial.ts";

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

test("jobs run one at a time, in order", async () => {
  const q = new SerialQueue();
  const log: string[] = [];
  const job = (name: string, ms: number) => async () => {
    log.push(`${name} start`);
    await sleep(ms);
    log.push(`${name} end`);
    return name;
  };
  const results = await Promise.all([q.run(job("a", 20)), q.run(job("b", 0)), q.run(job("c", 5))]);
  assert.deepEqual(results, ["a", "b", "c"]);
  assert.deepEqual(log, ["a start", "a end", "b start", "b end", "c start", "c end"]);
});

test("a failed job rejects its own promise and doesn't block the next", async () => {
  const q = new SerialQueue();
  const failed = q.run(async () => {
    await sleep(5);
    throw new Error("unplugged");
  });
  const next = q.run(async () => "ok");
  await assert.rejects(failed, /unplugged/);
  assert.equal(await next, "ok");
});

test("a job sees what the job before it wrote", async () => {
  const q = new SerialQueue();
  let value = "KC_ESC";
  const set = (to: string) =>
    q.run(async () => {
      const before = value;
      await sleep(Math.random() * 10);
      value = to;
      return before;
    });
  const befores = await Promise.all([set("KC_A"), set("KC_B")]);
  assert.deepEqual(befores, ["KC_ESC", "KC_A"]);
  assert.equal(value, "KC_B");
});
