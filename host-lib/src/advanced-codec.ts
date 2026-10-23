/**
 * Advanced-key table entries and the macro buffer, to and from the bytes
 * of protocol.yaml's feature_tables (the firmware's kb/features.h). Values
 * are checked only as far as the bytes need; the board checks the rest and
 * answers "invalid-value".
 */
import { decodeKeycode, encodeKeycode, KeycodeError } from "./keycode-codec.js";
import { KEYCODE_CONSTANTS as K } from "./keycodes.gen.js";
import type { MatrixPosition } from "./keyboard-definition.js";
import { FeatureTable } from "./protocol.gen.js";
import type {
  AdvancedOptions,
  Combo,
  EncoderAction,
  KeyOverride,
  LeaderSequence,
  MacroStep,
  TapDance,
  TapHoldRule,
} from "./types.js";

const NONE = 0xff;
const COMBO_KEYS = 4;
const LEADER_KEYS = 4;

const MACRO_END = 0x00;
const MACRO_OPS = { tap: 0x01, down: 0x02, up: 0x03, delay: 0x04 } as const;

const TH_PERMISSIVE_HOLD = 0x01;
const TH_HOLD_ON_OTHER_PRESS = 0x02;
const TH_RETRO_TAP = 0x04;

class Writer {
  readonly bytes: Uint8Array;
  private readonly view: DataView;
  private off = 0;

  constructor(size: number) {
    this.bytes = new Uint8Array(size);
    this.view = new DataView(this.bytes.buffer);
  }

  u8(v: number, what: string): this {
    if (!Number.isInteger(v) || v < 0 || v > 0xff) throw new KeycodeError(`${what} must be 0-255`);
    this.view.setUint8(this.off++, v);
    return this;
  }

  u16(v: number, what: string): this {
    if (!Number.isInteger(v) || v < 0 || v > 0xffff) throw new KeycodeError(`${what} must be 0-65535`);
    this.view.setUint16(this.off, v, true);
    this.off += 2;
    return this;
  }

  keycode(id: string): this {
    return this.u16(encodeKeycode(id), "keycode");
  }
}

class Reader {
  private readonly view: DataView;
  private off = 0;

  constructor(bytes: Uint8Array) {
    this.view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  }

  u8(): number {
    return this.view.getUint8(this.off++);
  }

  u16(): number {
    const v = this.view.getUint16(this.off, true);
    this.off += 2;
    return v;
  }

  keycode(): string {
    return decodeKeycode(this.u16());
  }
}

/** A basic key's HID usage (KC_A ... KC_RGUI). */
function usage(id: string): number {
  const n = encodeKeycode(id);
  if (n < 0x04 || n > K.KB_KC_BASIC_MAX!) throw new KeycodeError(`${id} is not a basic key`);
  return n;
}

function layersToMask(layers: readonly number[]): number {
  let mask = 0;
  for (const l of layers) {
    if (!Number.isInteger(l) || l < 0 || l > 15) throw new KeycodeError(`layer ${l} out of range`);
    mask |= 1 << l;
  }
  return mask;
}

function maskToLayers(mask: number): number[] {
  return Array.from({ length: 16 }, (_, l) => l).filter((l) => mask & (1 << l));
}

export function encodeOptions(o: AdvancedOptions): Uint8Array {
  return new Writer(FeatureTable.options.size)
    .u16(o.leaderTimeoutMs, "leaderTimeoutMs")
    .u8(o.capsWordOnBothShifts ? 1 : 0, "capsWordOnBothShifts")
    .u16(o.capsWordIdleMs, "capsWordIdleMs")
    .u8(o.autoShift ? 1 : 0, "autoShift")
    .u16(o.autoShiftMs, "autoShiftMs")
    .u8(o.mouse.intervalMs, "mouse.intervalMs")
    .u8(o.mouse.moveDelta, "mouse.moveDelta")
    .u8(o.mouse.maxSpeed, "mouse.maxSpeed")
    .u16(o.mouse.timeToMaxMs, "mouse.timeToMaxMs")
    .u8(o.mouse.wheelIntervalMs, "mouse.wheelIntervalMs").bytes;
}

export function decodeOptions(b: Uint8Array): AdvancedOptions {
  const r = new Reader(b);
  return {
    leaderTimeoutMs: r.u16(),
    capsWordOnBothShifts: r.u8() !== 0,
    capsWordIdleMs: r.u16(),
    autoShift: r.u8() !== 0,
    autoShiftMs: r.u16(),
    mouse: {
      intervalMs: r.u8(),
      moveDelta: r.u8(),
      maxSpeed: r.u8(),
      timeToMaxMs: r.u16(),
      wheelIntervalMs: r.u8(),
    },
  };
}

export function encodeCombo(c: Combo): Uint8Array {
  if (c.keys.length > COMBO_KEYS) throw new KeycodeError(`a combo has at most ${COMBO_KEYS} keys`);
  const w = new Writer(FeatureTable.combo.size);
  for (let k = 0; k < COMBO_KEYS; k++) {
    const key = c.keys[k];
    w.u8(key ? key.row : NONE, "row").u8(key ? key.col : NONE, "col");
  }
  return w.keycode(c.result).u16(c.timeoutMs, "timeoutMs").u16(layersToMask(c.layers), "layers").bytes;
}

export function decodeCombo(b: Uint8Array): Combo {
  const r = new Reader(b);
  const keys: MatrixPosition[] = [];
  for (let k = 0; k < COMBO_KEYS; k++) {
    const row = r.u8();
    const col = r.u8();
    if (row !== NONE) keys.push({ row, col });
  }
  return { keys, result: r.keycode(), timeoutMs: r.u16(), layers: maskToLayers(r.u16()) };
}

export function encodeTapDance(t: TapDance): Uint8Array {
  return new Writer(FeatureTable.tapDance.size)
    .keycode(t.tap)
    .keycode(t.hold)
    .keycode(t.doubleTap)
    .keycode(t.tripleTap)
    .u16(t.termMs, "termMs").bytes;
}

export function decodeTapDance(b: Uint8Array): TapDance {
  const r = new Reader(b);
  return { tap: r.keycode(), hold: r.keycode(), doubleTap: r.keycode(), tripleTap: r.keycode(), termMs: r.u16() };
}

export function encodeKeyOverride(o: KeyOverride): Uint8Array {
  const unused = o.replacement === "KC_NO";
  return new Writer(FeatureTable.keyOverride.size)
    .u8(unused ? 0 : o.triggerMods, "triggerMods")
    .u8(unused ? 0 : usage(o.triggerKey), "triggerKey")
    .keycode(o.replacement)
    .u16(layersToMask(o.layers), "layers").bytes;
}

export function decodeKeyOverride(b: Uint8Array): KeyOverride {
  const r = new Reader(b);
  const triggerMods = r.u8();
  const key = r.u8();
  return {
    triggerMods,
    triggerKey: decodeKeycode(key),
    replacement: r.keycode(),
    layers: maskToLayers(r.u16()),
  };
}

export function encodeLeader(s: LeaderSequence): Uint8Array {
  if (s.keys.length > LEADER_KEYS) throw new KeycodeError(`a Leader sequence has at most ${LEADER_KEYS} keys`);
  const w = new Writer(FeatureTable.leader.size);
  for (let k = 0; k < LEADER_KEYS; k++) {
    const key = s.keys[k];
    w.u8(key === undefined ? 0 : usage(key), "key");
  }
  return w.keycode(s.result).bytes;
}

export function decodeLeader(b: Uint8Array): LeaderSequence {
  const r = new Reader(b);
  const keys: string[] = [];
  for (let k = 0; k < LEADER_KEYS; k++) {
    const u = r.u8();
    if (u !== 0) keys.push(decodeKeycode(u));
  }
  return { keys, result: r.keycode() };
}

export function encodeTapHold(rule: TapHoldRule | null): Uint8Array {
  const w = new Writer(FeatureTable.tapHold.size);
  if (!rule) return w.u8(NONE, "row").u8(NONE, "col").bytes;
  const flags =
    (rule.permissiveHold ? TH_PERMISSIVE_HOLD : 0) |
    (rule.holdOnOtherKeyPress ? TH_HOLD_ON_OTHER_PRESS : 0) |
    (rule.retroTap ? TH_RETRO_TAP : 0);
  return w
    .u8(rule.row, "row")
    .u8(rule.col, "col")
    .u8(flags, "flags")
    .u16(rule.tappingTermMs, "tappingTermMs")
    .u16(rule.quickTapMs, "quickTapMs").bytes;
}

export function decodeTapHold(b: Uint8Array): TapHoldRule | null {
  const r = new Reader(b);
  const row = r.u8();
  const col = r.u8();
  const flags = r.u8();
  if (row === NONE) return null;
  return {
    row,
    col,
    permissiveHold: (flags & TH_PERMISSIVE_HOLD) !== 0,
    holdOnOtherKeyPress: (flags & TH_HOLD_ON_OTHER_PRESS) !== 0,
    retroTap: (flags & TH_RETRO_TAP) !== 0,
    tappingTermMs: r.u16(),
    quickTapMs: r.u16(),
  };
}

export function encodeEncoder(a: EncoderAction): Uint8Array {
  return new Writer(FeatureTable.encoder.size).keycode(a.cw).keycode(a.ccw).bytes;
}

export function decodeEncoder(b: Uint8Array): EncoderAction {
  const r = new Reader(b);
  return { cw: r.keycode(), ccw: r.keycode() };
}

/** Every macro, one after another, each ending with an end marker. */
export function encodeMacros(macros: readonly MacroStep[][]): Uint8Array {
  const out: number[] = [];
  for (const steps of macros) {
    for (const step of steps) {
      if (step.type === "text") {
        for (const ch of step.text) {
          const code = ch.codePointAt(0)!;
          if (code < 0x20 || code > 0x7e) throw new KeycodeError(`macro text can't type ${JSON.stringify(ch)}`);
          out.push(code);
        }
      } else {
        const arg = step.type === "delay" ? step.ms : encodeKeycode(step.keycode);
        if (!Number.isInteger(arg) || arg < 0 || arg > 0xffff) throw new KeycodeError(`macro delay must be 0-65535 ms`);
        out.push(MACRO_OPS[step.type], arg & 0xff, arg >> 8);
      }
    }
    out.push(MACRO_END);
  }
  return Uint8Array.from(out);
}

/**
 * The macros in a buffer, up to the last that isn't empty. A malformed
 * step ends the list there (the board never accepts one).
 */
export function decodeMacros(buf: Uint8Array): MacroStep[][] {
  const macros: MacroStep[][] = [];
  let steps: MacroStep[] = [];
  let off = 0;
  while (off < buf.length) {
    const op = buf[off]!;
    if (op === MACRO_END) {
      macros.push(steps);
      steps = [];
      off++;
    } else if (op >= MACRO_OPS.tap && op <= MACRO_OPS.delay) {
      if (off + 3 > buf.length) break;
      const arg = buf[off + 1]! | (buf[off + 2]! << 8);
      if (op === MACRO_OPS.delay) steps.push({ type: "delay", ms: arg });
      else {
        const type = op === MACRO_OPS.tap ? "tap" : op === MACRO_OPS.down ? "down" : "up";
        steps.push({ type, keycode: decodeKeycode(arg) });
      }
      off += 3;
    } else if (op >= 0x20 && op <= 0x7e) {
      const last = steps[steps.length - 1];
      const ch = String.fromCharCode(op);
      if (last?.type === "text") last.text += ch;
      else steps.push({ type: "text", text: ch });
      off++;
    } else {
      break;
    }
  }
  if (steps.length) macros.push(steps);
  while (macros.length && macros[macros.length - 1]!.length === 0) macros.pop();
  return macros;
}
