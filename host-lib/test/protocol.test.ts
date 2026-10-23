// Config protocol bindings: frame encoding, the generated payload codecs, and
// agreement with protocol/protocol.yaml and the generated C header.
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import path from "node:path";

import { protocol } from "../src/index.js";

const repoDir = fileURLToPath(new URL("../../../", import.meta.url));
const header = readFileSync(path.join(repoDir, "firmware/core/include/kb/protocol_gen.h"), "utf8");
const yaml = readFileSync(path.join(repoDir, "protocol/protocol.yaml"), "utf8");

function cDefine(name: string): number {
  const m = new RegExp(`^#define ${name} (0x[0-9A-F]+|\\d+)u`, "m").exec(header);
  assert.ok(m, `${name} not in protocol_gen.h`);
  return Number(m[1]);
}

test("TS and C bindings agree on the frame and the HID interface", () => {
  assert.equal(protocol.PROTOCOL_VERSION, cDefine("KB_PROTO_VERSION"));
  assert.equal(protocol.REPORT_SIZE, cDefine("KB_PROTO_REPORT_SIZE"));
  assert.equal(protocol.HEADER_SIZE, cDefine("KB_PROTO_HEADER_SIZE"));
  assert.equal(protocol.MAX_PAYLOAD, cDefine("KB_PROTO_MAX_PAYLOAD"));
  assert.equal(protocol.HID_USAGE_PAGE, cDefine("KB_PROTO_HID_USAGE_PAGE"));
  assert.ok(protocol.HID_USAGE_PAGE >= 0xff00, "the config interface must use a vendor-defined usage page");
});

test("every command and status in protocol.yaml is in the TS bindings", () => {
  const names = [...yaml.matchAll(/^ {2}- name: (\w+)\n {4}id: (0x[0-9a-fA-F]+)/gm)];
  assert.ok(names.length >= 10);
  for (const [, name, id] of names) {
    const key = name!.replace(/_(\w)/g, (_, c: string) => c.toUpperCase()) as protocol.CommandName;
    assert.equal(protocol.Command[key], Number(id), name);
  }
  assert.equal(protocol.Status.ok, 0);
  assert.equal(protocol.Status.versionMismatch, cDefineEnum("KB_PROTO_VERSION_MISMATCH"));
});

function cDefineEnum(name: string): number {
  const m = new RegExp(`\\b${name} = (\\d+),`).exec(header);
  assert.ok(m, `${name} not in protocol_gen.h`);
  return Number(m[1]);
}

test("request frames carry the header and are padded to a full report", () => {
  const payload = protocol.encodeSetKeyRequest({ layer: 1, row: 2, col: 3, keycode: 0x4101 });
  const frame = protocol.encodeRequest(protocol.Command.setKey, 7, payload);
  assert.equal(frame.length, 64);
  assert.deepEqual([...frame.subarray(0, 9)], [0x11, 7, 5, protocol.PROTOCOL_VERSION, 1, 2, 3, 0x01, 0x41]);
  assert.ok(frame.subarray(9).every((b) => b === 0));
  assert.throws(() => protocol.encodeRequest(1, 0, new Uint8Array(61)), protocol.FrameError);
});

test("response frames decode, and bad lengths are refused", () => {
  const report = new Uint8Array(64);
  report.set([0x10, 9, 2, protocol.Status.ok, 0x34, 0x12]);
  const r = protocol.decodeResponse(report);
  assert.equal(r.command, protocol.Command.getKey);
  assert.equal(r.sequence, 9);
  assert.ok(protocol.isOk(r));
  assert.deepEqual(protocol.decodeGetKeyResponse(r.payload), { keycode: 0x1234 });

  report[2] = 61;
  assert.throws(() => protocol.decodeResponse(report), protocol.FrameError);
  assert.throws(() => protocol.decodeResponse(new Uint8Array(3)), protocol.FrameError);
  assert.equal(protocol.statusText(protocol.Status.outOfRange), "An index (layer, row, column, offset) is out of range");
  assert.equal(protocol.statusText(200), "Unknown status 200");
});

test("payload codecs round-trip and check their input", () => {
  const hs = {
    protocolVersion: 1,
    firmwareMajor: 0,
    firmwareMinor: 5,
    firmwarePatch: 0,
    build: "abc1234-dirty",
    capabilities: 0x80000201,
    rows: 6,
    cols: 15,
    layers: 4,
    encoders: 1,
    macroBufferSize: 1024,
    model: "Dachshund60F",
  };
  const bytes = protocol.encodeHandshakeResponse(hs);
  assert.equal(bytes.length, 46);
  assert.deepEqual(protocol.decodeHandshakeResponse(bytes), hs);
  assert.throws(() => protocol.decodeHandshakeResponse(bytes.subarray(1)), protocol.PayloadError);
  assert.throws(() => protocol.encodeHandshakeResponse({ ...hs, model: "x".repeat(17) }), protocol.PayloadError);

  const chunk = { offset: 300, keycodes: Array.from({ length: 28 }, (_, i) => i * 0x101) };
  const c = protocol.encodeSetKeymapChunkRequest(chunk);
  assert.equal(c.length, 58);
  assert.deepEqual(protocol.decodeSetKeymapChunkRequest(c), chunk);
  assert.throws(
    () => protocol.encodeSetKeymapChunkRequest({ offset: 0, keycodes: new Array(29).fill(4) }),
    protocol.PayloadError,
  );
  assert.throws(() => protocol.decodeSetKeymapChunkRequest(new Uint8Array(5)), protocol.PayloadError);
  assert.throws(() => protocol.encodeGetKeyRequest({ layer: 256, row: 0, col: 0 }), protocol.PayloadError);
  assert.deepEqual(protocol.decodeSaveRequest(protocol.encodeSaveRequest({})), {});
});
