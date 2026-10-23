/**
 * Board over the config protocol: what the GUI uses for a real keyboard (or
 * the host-built firmware in the tests), whatever the transport.
 *
 * Opening a board runs the handshake. A board that speaks another protocol
 * version still opens, so the GUI can show it, but every call on it fails
 * with "version-mismatch" and nothing is ever written to it.
 */
import { ProtocolClient, type ClientOptions } from "./client.js";
import { decodeKeycode, encodeKeycode, KeycodeError } from "./keycode-codec.js";
import {
  Capability,
  Command,
  decodeGetKeymapChunkResponse,
  decodeGetSerialResponse,
  decodeHandshakeResponse,
  encodeGetKeymapChunkRequest,
  encodeIdentifyRequest,
  encodeRebootRequest,
  encodeSetKeyRequest,
  PROTOCOL_VERSION,
  type HandshakeResponse,
} from "./protocol.gen.js";
import type { Transport } from "./transport.js";
import {
  BoardError,
  type BatteryStatus,
  type Board,
  type BoardEvent,
  type BoardInfo,
  type BoardSettings,
  type EncoderAction,
  type KeycodeId,
  type KeyPosition,
  type Keymap,
} from "./types.js";

/** Keycodes per get_keymap_chunk request (the most a response holds). */
const CHUNK = 28;

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
    // Encoder mapping arrives in phase 9; until then no layer has any.
    const encoders: EncoderAction[][] = keys.map(() => []);
    return { keys, encoders };
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
    void layer;
    void encoder;
    void action;
    this.check(Capability.encoders, "encoder mapping");
    throw new BoardError("not-supported", `encoder mapping isn't in protocol ${PROTOCOL_VERSION} yet`);
  }

  async getSettings(): Promise<BoardSettings> {
    this.check(Capability.settings, "settings");
    throw new BoardError("not-supported", `settings aren't in protocol ${PROTOCOL_VERSION} yet`);
  }

  async setSettings(patch: Partial<BoardSettings>): Promise<BoardSettings> {
    void patch;
    this.check(Capability.settings, "settings");
    throw new BoardError("not-supported", `settings aren't in protocol ${PROTOCOL_VERSION} yet`);
  }

  /**
   * True after a change made through this connection that hasn't been saved
   * or reverted. The protocol has no way to ask the board, so changes made
   * by another app (or before this connection) don't count.
   */
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
