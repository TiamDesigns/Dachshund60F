import { test } from "node:test";
import assert from "node:assert/strict";

import { BoardError, protocol, ProtocolClient } from "../src/index.js";
import { FakeTransport, response } from "./fake-device.js";

const { Command, Status } = protocol;

async function rejectsWith(promise: Promise<unknown>, code: string): Promise<void> {
  await assert.rejects(promise, (err: unknown) => err instanceof BoardError && err.code === code);
}

test("a response is matched by command and sequence; others are ignored", async () => {
  const t = new FakeTransport((req) => [
    response(req.command, (req.sequence + 1) & 0xff, 0, Uint8Array.of(9)), // stale sequence
    response(Command.save, req.sequence, 0, Uint8Array.of(8)), // other command
    Uint8Array.of(1, 2), // not a frame
    response(req.command, req.sequence, 0, Uint8Array.of(7)),
  ]);
  const client = new ProtocolClient(t);
  assert.deepEqual(await client.request(Command.getKey, Uint8Array.of(0, 0, 0)), Uint8Array.of(7));
  assert.equal(t.requests[0]!.version, protocol.PROTOCOL_VERSION);
});

test("requests go one at a time, each with a new sequence number", async () => {
  let inFlight = 0;
  let most = 0;
  const t = new FakeTransport((req) => {
    inFlight++;
    most = Math.max(most, inFlight);
    setTimeout(() => inFlight--, 0);
    return response(req.command, req.sequence, 0, Uint8Array.of(req.sequence));
  });
  const client = new ProtocolClient(t);
  const answers = await Promise.all([1, 2, 3, 4].map(() => client.request(Command.getSerial)));
  assert.equal(most, 1);
  const sequences = t.requests.map((r) => r.sequence);
  assert.equal(new Set(sequences).size, 4);
  assert.deepEqual(answers.map((a) => a[0]), sequences);
});

test("a lost response is retried with a new sequence number, then times out", async () => {
  let drop = 1;
  const t = new FakeTransport((req) => (drop-- > 0 ? null : response(req.command, req.sequence, 0)));
  const client = new ProtocolClient(t, { timeoutMs: 20, retries: 2 });
  await client.request(Command.save);
  assert.equal(t.requests.length, 2);
  assert.notEqual(t.requests[0]!.sequence, t.requests[1]!.sequence);

  t.handler = () => null;
  t.requests.length = 0;
  await rejectsWith(client.request(Command.save), "timeout");
  assert.equal(t.requests.length, 3);
});

test("a late answer to a timed-out request isn't taken for the next one", async () => {
  const late: Uint8Array[] = [];
  let n = 0;
  const t = new FakeTransport((req) => {
    if (n++ === 0) {
      late.push(response(req.command, req.sequence, 0, Uint8Array.of(1)));
      return null;
    }
    // Deliver the first request's answer just before this one's.
    return [...late, response(req.command, req.sequence, 0, Uint8Array.of(2))];
  });
  const client = new ProtocolClient(t, { timeoutMs: 20 });
  assert.deepEqual(await client.request(Command.getSerial), Uint8Array.of(2));
});

test("reboot is never sent twice", async () => {
  const t = new FakeTransport(() => null);
  const client = new ProtocolClient(t, { timeoutMs: 20, retries: 3 });
  await rejectsWith(client.request(Command.reboot, Uint8Array.of(0)), "timeout");
  assert.equal(t.requests.length, 1);
});

test("error statuses become BoardErrors", async () => {
  const cases: [number, string][] = [
    [Status.unknownCommand, "not-supported"],
    [Status.badLength, "protocol-error"],
    [Status.outOfRange, "out-of-range"],
    [Status.badValue, "invalid-value"],
    [Status.busy, "busy"],
    [Status.storageFull, "storage-full"],
    [Status.storageError, "storage-error"],
    [Status.versionMismatch, "version-mismatch"],
    [Status.notSupported, "not-supported"],
    [200, "protocol-error"],
  ];
  for (const [status, code] of cases) {
    const client = new ProtocolClient(new FakeTransport((req) => response(req.command, req.sequence, status)));
    await rejectsWith(client.request(Command.save), code);
  }
});

test("a lost transport fails the request in flight and every later one", async () => {
  const t = new FakeTransport(() => null);
  const client = new ProtocolClient(t, { timeoutMs: 5000 });
  const pending = client.request(Command.save);
  setTimeout(() => void t.close(), 5);
  await rejectsWith(pending, "disconnected");
  await rejectsWith(client.request(Command.save), "disconnected");
});
