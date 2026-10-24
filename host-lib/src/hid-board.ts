/**
 * Board over the config protocol: what the GUI uses for a real keyboard (or
 * the host-built firmware in the tests), whatever the transport.
 *
 * Opening a board runs the handshake. A board that speaks another protocol
 * version still opens, so the GUI can show it, but every call on it fails
 * with "version-mismatch" and nothing is ever written to it.
 */
import * as codec from "./advanced-codec.js";
import { ProtocolClient, type ClientOptions } from "./client.js";
import { checkIndicatorColours, colourToNumber, numberToColour } from "./indicators.js";
import { decodeKeycode, encodeKeycode, KeycodeError } from "./keycode-codec.js";
import { checkSettings } from "./settings.js";
import {
  Capability,
  Command,
  decodeGetFeatureResponse,
  decodeGetMacroChunkResponse,
  encodeGetFeatureRequest,
  encodeGetMacroChunkRequest,
  encodeSetFeatureRequest,
  encodeSetMacroChunkRequest,
  FeatureTable,
  type FeatureTableName,
  decodeGetConnectionResponse,
  decodeGetIndicatorsResponse,
  decodeGetKeymapChunkResponse,
  decodeGetLayerColourResponse,
  decodeGetSettingsResponse,
  decodeGetSerialResponse,
  decodeHandshakeResponse,
  encodeGetKeymapChunkRequest,
  encodeGetLayerColourRequest,
  encodeSetIndicatorsRequest,
  encodeSetLayerColourRequest,
  encodeSetSettingsRequest,
  encodeIdentifyRequest,
  encodeRebootRequest,
  encodeSetKeyRequest,
  encodeSelectProfileRequest,
  encodeClearProfileRequest,
  encodeSetOutputRequest,
  PROTOCOL_VERSION,
  type HandshakeResponse,
} from "./protocol.gen.js";
import type { Transport } from "./transport.js";
import {
  BLUETOOTH_STATES,
  BoardError,
  OUTPUTS,
  type ConnectionStatus,
  type AdvancedKeys,
  type AdvancedOptions,
  type AdvancedTable,
  type MacroStep,
  type BatteryStatus,
  type Board,
  type BoardEvent,
  type BoardInfo,
  type BoardSettings,
  DEBOUNCE_ALGORITHMS,
  type EncoderAction,
  type IndicatorColours,
  type KeycodeId,
  type KeyPosition,
  type Keymap,
} from "./types.js";

function profileByte(profile: number): number {
  if (!Number.isInteger(profile) || profile < 0 || profile > 0xff) {
    throw new BoardError("out-of-range", `there's no Bluetooth host ${profile + 1}`);
  }
  return profile;
}

/** Keycodes per get_keymap_chunk request (the most a response holds). */
const CHUNK = 28;

/** Encoder table entries per layer (the firmware's KB_ENCODER_MAX). */
const ENCODER_STRIDE = 2;

/** Macro bytes per get/set_macro_chunk request. */
const MACRO_CHUNK = 56;

/** The protocol table behind each AdvancedKeys table, and its codec. */
const TABLES = {
  combos: { table: "combo", encode: codec.encodeCombo, decode: codec.decodeCombo },
  tapDances: { table: "tapDance", encode: codec.encodeTapDance, decode: codec.decodeTapDance },
  keyOverrides: { table: "keyOverride", encode: codec.encodeKeyOverride, decode: codec.decodeKeyOverride },
  leader: { table: "leader", encode: codec.encodeLeader, decode: codec.decodeLeader },
  tapHold: { table: "tapHold", encode: codec.encodeTapHold, decode: codec.decodeTapHold },
} as const satisfies Record<AdvancedTable, { table: FeatureTableName; encode: unknown; decode: unknown }>;

/** Run an encoder from the codec, turning bad values into "invalid-value". */
function encoded<T>(encode: (value: T) => Uint8Array, value: T): Uint8Array {
  try {
    return encode(value);
  } catch (err) {
    if (err instanceof KeycodeError) throw new BoardError("invalid-value", err.message);
    throw err;
  }
}

/** How long identify flashes the indicators, in seconds. */
const IDENTIFY_SECONDS = 3;

export interface HidBoardOptions extends ClientOptions {
  /**
   * The serial number the transport reports, if it can read one. Used as the
   * id when the board can't be asked (another protocol version).
   */
  serialNumber?: string;
}

/** Capability names set in a handshake's capability bits. */
export function capabilityNames(bits: number): string[] {
  return Object.entries(Capability)
    .filter(([, bit]) => (bits & bit) !== 0)
    .map(([name]) => name);
}

export class HidBoard implements Board {
  readonly info: BoardInfo;
  /** False when the board speaks another protocol version; it is then read-only and unusable. */
  readonly compatible: boolean;
  /** The firmware build (git describe), or "" when unknown. */
  readonly build: string;
  readonly rows: number;
  readonly cols: number;

  private readonly client: ProtocolClient;
  private readonly capabilities: number;
  private readonly encoderCount: number;
  private readonly macroBufferSize: number;
  private unsaved = false;
  private readonly listeners = new Set<(event: BoardEvent) => void>();

  /** Run the handshake on a transport and return the board behind it. */
  static async open(transport: Transport, options: HidBoardOptions = {}): Promise<HidBoard> {
    const client = new ProtocolClient(transport, options);
    try {
      const payload = await client.request(Command.handshake);
      const version = payload[0];
      if (version !== PROTOCOL_VERSION) {
        return new HidBoard(client, null, version ?? 0, options.serialNumber ?? "");
      }
      const handshake = decodeHandshakeResponse(payload);
      const { serial } = decodeGetSerialResponse(await client.request(Command.getSerial));
      return new HidBoard(client, handshake, version, serial || options.serialNumber || "");
    } catch (err) {
      await client.close().catch(() => undefined);
      throw err;
    }
  }

  private constructor(
    client: ProtocolClient,
    handshake: HandshakeResponse | null,
    protocolVersion: number,
    serial: string,
  ) {
    this.client = client;
    this.compatible = handshake !== null;
    this.build = handshake?.build ?? "";
    this.rows = handshake?.rows ?? 0;
    this.cols = handshake?.cols ?? 0;
    this.capabilities = handshake?.capabilities ?? 0;
    this.encoderCount = handshake?.encoders ?? 0;
    this.macroBufferSize = handshake?.macroBufferSize ?? 0;
    this.info = {
      id: serial,
      model: handshake?.model ?? "",
      connection: client.transport.connection,
      firmwareVersion: handshake
        ? `${handshake.firmwareMajor}.${handshake.firmwareMinor}.${handshake.firmwarePatch}`
        : "",
      protocolVersion,
      layerCount: handshake?.layers ?? 0,
      capabilities: capabilityNames(this.capabilities),
    };
    client.transport.onClose(() => this.emit({ type: "disconnected" }));
  }

  get connected(): boolean {
    return !this.client.isClosed;
  }

  /** Close the transport. The board then reports "disconnected". */
  close(): Promise<void> {
    return this.client.close();
  }

  private emit(event: BoardEvent): void {
    for (const listener of this.listeners) listener(event);
  }

  private check(capability?: number, what?: string): void {
    if (!this.compatible) {
      throw new BoardError(
        "version-mismatch",
        `the board speaks protocol ${this.info.protocolVersion}, this app ${PROTOCOL_VERSION}; update one of them`,
      );
    }
    if (capability !== undefined && (this.capabilities & capability) === 0) {
      throw new BoardError("not-supported", `this firmware doesn't support ${what}`);
    }
  }

  private async call(command: number, payload?: Uint8Array, capability?: number, what?: string): Promise<Uint8Array> {
    this.check(capability, what);
    return this.client.request(command, payload);
  }

  async getBattery(): Promise<BatteryStatus | null> {
    this.check();
    // Battery reading arrives with power management (phase 13).
    return null;
  }

  async getKeymap(): Promise<Keymap> {
    this.check(Capability.keymap, "keymap access");
    const { rows, cols } = this;
    const layers = this.info.layerCount;
    const total = layers * rows * cols;
    const numbers: number[] = [];
    while (numbers.length < total) {
      const count = Math.min(CHUNK, total - numbers.length);
      const payload = await this.client.request(
        Command.getKeymapChunk,
        encodeGetKeymapChunkRequest({ offset: numbers.length, count }),
      );
      const chunk = decodeGetKeymapChunkResponse(payload);
      if (chunk.offset !== numbers.length || chunk.keycodes.length !== count) {
        throw new BoardError("protocol-error", `keymap chunk at ${numbers.length} came back wrong`);
      }
      numbers.push(...chunk.keycodes);
    }

    const keys: KeycodeId[][][] = [];
    for (let l = 0; l < layers; l++) {
      const layer: KeycodeId[][] = [];
      for (let r = 0; r < rows; r++) {
        const start = (l * rows + r) * cols;
        layer.push(numbers.slice(start, start + cols).map(decodeKeycode));
      }
      keys.push(layer);
    }
    const encoders: EncoderAction[][] = keys.map(() => []);
    if (this.capabilities & Capability.encoders) {
      for (let l = 0; l < layers; l++) {
        for (let e = 0; e < this.encoderCount; e++) {
          encoders[l]!.push(codec.decodeEncoder(await this.getEntry("encoder", l * ENCODER_STRIDE + e)));
        }
      }
    }
    return { keys, encoders };
  }

  private async getEntry(table: FeatureTableName, index: number): Promise<Uint8Array> {
    const { id, size } = FeatureTable[table];
    const r = decodeGetFeatureResponse(
      await this.client.request(Command.getFeature, encodeGetFeatureRequest({ table: id, index })),
    );
    if (r.table !== id || r.index !== index || r.data.length < size) {
      throw new BoardError("protocol-error", `${table} entry ${index} came back wrong`);
    }
    return r.data.subarray(0, size);
  }

  private async setEntry(table: FeatureTableName, index: number, data: Uint8Array): Promise<void> {
    await this.client.request(
      Command.setFeature,
      encodeSetFeatureRequest({ table: FeatureTable[table].id, index, data }),
    );
    this.unsaved = true;
    this.emit({ type: "changed" });
  }

  async setKey({ layer, row, col }: KeyPosition, keycode: KeycodeId): Promise<void> {
    this.check(Capability.keymap, "keymap access");
    let number: number;
    try {
      number = encodeKeycode(keycode);
    } catch (err) {
      if (err instanceof KeycodeError) throw new BoardError("invalid-value", err.message);
      throw err;
    }
    for (const [name, value, limit] of [
      ["layer", layer, this.info.layerCount],
      ["row", row, this.rows],
      ["column", col, this.cols],
    ] as const) {
      if (!Number.isInteger(value) || value < 0 || value >= limit) {
        throw new BoardError("out-of-range", `${name} ${value} out of range`);
      }
    }
    await this.client.request(Command.setKey, encodeSetKeyRequest({ layer, row, col, keycode: number }));
    this.unsaved = true;
    this.emit({ type: "changed" });
  }

  async setEncoder(layer: number, encoder: number, action: EncoderAction): Promise<void> {
    this.check(Capability.encoders, "encoder mapping");
    for (const [name, value, limit] of [
      ["layer", layer, this.info.layerCount],
      ["encoder", encoder, this.encoderCount],
    ] as const) {
      if (!Number.isInteger(value) || value < 0 || value >= limit) {
        throw new BoardError("out-of-range", `${name} ${value} out of range`);
      }
    }
    await this.setEntry("encoder", layer * ENCODER_STRIDE + encoder, encoded(codec.encodeEncoder, action));
  }

  async getAdvancedKeys(): Promise<AdvancedKeys> {
    this.check(Capability.advancedKeys, "advanced keys");
    const read = async <T>(table: FeatureTableName, decode: (b: Uint8Array) => T): Promise<T[]> => {
      const out: T[] = [];
      for (let i = 0; i < FeatureTable[table].entries; i++) out.push(decode(await this.getEntry(table, i)));
      return out;
    };
    return {
      options: codec.decodeOptions(await this.getEntry("options", 0)),
      combos: await read("combo", codec.decodeCombo),
      tapDances: await read("tapDance", codec.decodeTapDance),
      keyOverrides: await read("keyOverride", codec.decodeKeyOverride),
      leader: await read("leader", codec.decodeLeader),
      tapHold: await read("tapHold", codec.decodeTapHold),
    };
  }

  async setAdvancedEntry<T extends AdvancedTable>(
    table: T,
    index: number,
    value: AdvancedKeys[T][number],
  ): Promise<void> {
    this.check(Capability.advancedKeys, "advanced keys");
    const spec = TABLES[table];
    if (!spec) throw new BoardError("invalid-value", `no table ${String(table)}`);
    const { entries } = FeatureTable[spec.table];
    if (!Number.isInteger(index) || index < 0 || index >= entries) {
      throw new BoardError("out-of-range", `${table} slot ${index} out of range`);
    }
    const encode = spec.encode as (v: AdvancedKeys[T][number]) => Uint8Array;
    await this.setEntry(spec.table, index, encoded(encode, value));
  }

  async setAdvancedOptions(patch: Partial<AdvancedOptions>): Promise<AdvancedOptions> {
    this.check(Capability.advancedKeys, "advanced keys");
    const current = codec.decodeOptions(await this.getEntry("options", 0));
    const next = { ...current, ...patch, mouse: { ...current.mouse, ...patch.mouse } };
    await this.setEntry("options", 0, encoded(codec.encodeOptions, next));
    return next;
  }

  async getMacros(): Promise<MacroStep[][]> {
    this.check(Capability.macros, "macros");
    const buf = new Uint8Array(this.macroBufferSize);
    for (let offset = 0; offset < buf.length; offset += MACRO_CHUNK) {
      const count = Math.min(MACRO_CHUNK, buf.length - offset);
      const r = decodeGetMacroChunkResponse(
        await this.client.request(Command.getMacroChunk, encodeGetMacroChunkRequest({ offset, count })),
      );
      if (r.offset !== offset || r.data.length !== count) {
        throw new BoardError("protocol-error", `macro chunk at ${offset} came back wrong`);
      }
      buf.set(r.data, offset);
    }
    return codec.decodeMacros(buf);
  }

  async setMacros(macros: MacroStep[][]): Promise<void> {
    this.check(Capability.macros, "macros");
    const bytes = encoded(codec.encodeMacros, macros);
    if (bytes.length > this.macroBufferSize) {
      throw new BoardError(
        "storage-full",
        `the macros take ${bytes.length} bytes; the board has room for ${this.macroBufferSize}`,
      );
    }
    // The whole buffer, so nothing of the old macros is left after the new ones.
    const buf = new Uint8Array(this.macroBufferSize);
    buf.set(bytes);
    for (let offset = 0; offset < buf.length; offset += MACRO_CHUNK) {
      const data = buf.subarray(offset, Math.min(offset + MACRO_CHUNK, buf.length));
      await this.client.request(Command.setMacroChunk, encodeSetMacroChunkRequest({ offset, data }));
    }
    await this.client.request(Command.applyMacros);
    this.unsaved = true;
    this.emit({ type: "changed" });
  }

  async getSettings(): Promise<BoardSettings> {
    const r = decodeGetSettingsResponse(await this.call(Command.getSettings, undefined, Capability.settings, "settings"));
    const debounceAlgorithm = DEBOUNCE_ALGORITHMS[r.debounceAlgorithm];
    if (!debounceAlgorithm) {
      throw new BoardError("protocol-error", `unknown debounce algorithm ${r.debounceAlgorithm}`);
    }
    return {
      name: r.name,
      debounceAlgorithm,
      debounceMs: r.debounceMs,
      tappingTermMs: r.tappingTermMs,
      nkro: r.nkro !== 0,
      sleepTimeoutMin: r.sleepTimeoutMin,
      indicatorBrightness: r.indicatorBrightness,
    };
  }

  async setSettings(patch: Partial<BoardSettings>): Promise<BoardSettings> {
    const next = { ...(await this.getSettings()), ...patch };
    checkSettings(next);
    await this.client.request(
      Command.setSettings,
      encodeSetSettingsRequest({
        debounceAlgorithm: DEBOUNCE_ALGORITHMS.indexOf(next.debounceAlgorithm),
        debounceMs: next.debounceMs,
        tappingTermMs: next.tappingTermMs,
        nkro: next.nkro ? 1 : 0,
        sleepTimeoutMin: next.sleepTimeoutMin,
        indicatorBrightness: next.indicatorBrightness,
        name: next.name,
      }),
    );
    this.unsaved = true;
    this.emit({ type: "changed" });
    return next;
  }

  async getIndicators(): Promise<IndicatorColours> {
    const r = decodeGetIndicatorsResponse(
      await this.call(Command.getIndicators, undefined, Capability.indicators, "indicator colours"),
    );
    const layers: string[] = [];
    for (let layer = 0; layer < this.info.layerCount; layer++) {
      const c = decodeGetLayerColourResponse(
        await this.client.request(Command.getLayerColour, encodeGetLayerColourRequest({ layer })),
      );
      if (c.layer !== layer) throw new BoardError("protocol-error", `layer ${layer} colour came back wrong`);
      layers.push(numberToColour(c.colour));
    }
    return {
      layers,
      caps: numberToColour(r.caps),
      num: numberToColour(r.num),
      scroll: numberToColour(r.scroll),
      battery: numberToColour(r.battery),
      batteryLow: numberToColour(r.batteryLow),
      batteryLowPercent: r.batteryLowPercent,
    };
  }

  async setIndicators(patch: Partial<IndicatorColours>): Promise<IndicatorColours> {
    const current = await this.getIndicators();
    const next = { ...current, ...patch };
    checkIndicatorColours(next, this.info.layerCount);
    const { layers, ...fixed } = next;
    const { layers: _, ...was } = current;
    if (JSON.stringify(fixed) !== JSON.stringify(was)) {
      await this.client.request(
        Command.setIndicators,
        encodeSetIndicatorsRequest({
          caps: colourToNumber(next.caps),
          num: colourToNumber(next.num),
          scroll: colourToNumber(next.scroll),
          battery: colourToNumber(next.battery),
          batteryLow: colourToNumber(next.batteryLow),
          batteryLowPercent: next.batteryLowPercent,
        }),
      );
    }
    for (let layer = 0; layer < layers.length; layer++) {
      if (layers[layer] !== current.layers[layer]) {
        await this.client.request(
          Command.setLayerColour,
          encodeSetLayerColourRequest({ layer, colour: colourToNumber(layers[layer]!) }),
        );
      }
    }
    this.unsaved = true;
    this.emit({ type: "changed" });
    return next;
  }

  /**
   * True after a change made through this connection that hasn't been saved
   * or reverted. The protocol has no way to ask the board, so changes made
   * by another app (or before this connection) don't count.
   */
  async getConnection(): Promise<ConnectionStatus> {
    const r = decodeGetConnectionResponse(
      await this.call(Command.getConnection, undefined, Capability.bluetooth, "Bluetooth"),
    );
    const preferred = OUTPUTS[r.preferredOutput];
    const bluetooth = BLUETOOTH_STATES[r.ble];
    if (!preferred || bluetooth === undefined || r.activeOutput >= OUTPUTS.length || r.profile >= r.profileCount) {
      throw new BoardError("protocol-error", "the board reported a connection state this app doesn't know");
    }
    return {
      active: OUTPUTS[r.activeOutput] ?? null,
      preferred,
      usb: r.usb !== 0,
      bluetooth,
      profile: r.profile,
      paired: Array.from({ length: r.profileCount }, (_, i) => ((r.bonded >> i) & 1) === 1),
    };
  }

  async setOutput(output: "usb" | "ble"): Promise<void> {
    const n = OUTPUTS.indexOf(output);
    if (output !== "usb" && output !== "ble") throw new BoardError("invalid-value", `can't prefer output ${String(output)}`);
    await this.call(Command.setOutput, encodeSetOutputRequest({ output: n }), Capability.bluetooth, "Bluetooth");
  }

  async selectProfile(profile: number): Promise<void> {
    await this.call(
      Command.selectProfile,
      encodeSelectProfileRequest({ profile: profileByte(profile) }),
      Capability.bluetooth,
      "Bluetooth",
    );
  }

  async clearProfile(profile: number): Promise<void> {
    await this.call(
      Command.clearProfile,
      encodeClearProfileRequest({ profile: profileByte(profile) }),
      Capability.bluetooth,
      "Bluetooth",
    );
  }

  async hasUnsavedChanges(): Promise<boolean> {
    this.check();
    return this.unsaved;
  }

  async save(): Promise<void> {
    await this.call(Command.save, undefined, Capability.persistence, "saving");
    this.unsaved = false;
    this.emit({ type: "saved" });
  }

  async revert(): Promise<void> {
    await this.call(Command.revert, undefined, Capability.persistence, "saving");
    this.unsaved = false;
    this.emit({ type: "reverted" });
  }

  async factoryReset(): Promise<void> {
    await this.call(Command.factoryReset, undefined, Capability.persistence, "saving");
    this.unsaved = false;
    this.emit({ type: "reverted" });
  }

  async identify(): Promise<void> {
    await this.call(
      Command.identify,
      encodeIdentifyRequest({ seconds: IDENTIFY_SECONDS }),
      Capability.identify,
      "identify",
    );
  }

  /**
   * Restart the board, or with bootloader set, restart it into the UF2
   * bootloader for a firmware update. It drops off the bus either way.
   */
  async reboot(bootloader = false): Promise<void> {
    await this.call(Command.reboot, encodeRebootRequest({ mode: bootloader ? 1 : 0 }));
  }

  subscribe(listener: (event: BoardEvent) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }
}
