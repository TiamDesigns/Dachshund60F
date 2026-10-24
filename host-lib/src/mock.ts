/**
 * Mock boards for developing the GUI without hardware.
 *
 * MockBoard behaves like the firmware will: edits go to a live copy (RAM),
 * save() commits them, revert() restores the saved copy, and every index,
 * keycode and setting is validated with the same error codes. Battery level
 * drifts over time (charging on USB, draining on the dongle) when simulation
 * is on.
 */

import {
  checkAdvancedEntry,
  checkAdvancedOptions,
  checkMacros,
  defaultAdvancedKeys,
  ADVANCED_SLOTS,
  type AdvancedContext,
} from "./advanced.js";
import { decodeMacros, encodeMacros } from "./advanced-codec.js";
import { defaultKeymap } from "./default-keymaps.js";
import { isMatrixPositionUsed, type KeyboardDefinition } from "./keyboard-definition.js";
import { checkIndicatorColours, defaultIndicatorColours } from "./indicators.js";
import { BLE_PROFILE_COUNT, isValidKeycode } from "./keycodes.js";
import { checkSettings } from "./settings.js";
import {
  BoardError,
  type AdvancedKeys,
  type AdvancedOptions,
  type AdvancedTable,
  type BatteryStatus,
  type Board,
  type BoardEvent,
  type BoardInfo,
  type BoardManager,
  type BoardSettings,
  type BluetoothState,
  type ConnectionKind,
  type ConnectionStatus,
  type InputTiming,
  type EncoderAction,
  type IndicatorColours,
  type KeyPosition,
  type Keymap,
  type KeycodeId,
  type MacroStep,
} from "./types.js";

export const MOCK_FIRMWARE_VERSION = "0.1.0-dev";
export const MOCK_PROTOCOL_VERSION = 1;
const LAYER_COUNT = 4;

export const DEFAULT_SETTINGS: Readonly<BoardSettings> = {
  name: "",
  debounceAlgorithm: "eager-press",
  debounceMs: 5,
  tappingTermMs: 200,
  nkro: true,
  sleepTimeoutMin: 15,
  indicatorBrightness: 40,
};

interface StoredState {
  keymap: Keymap;
  settings: BoardSettings;
  advanced: AdvancedKeys;
  macros: MacroStep[][];
  indicators: IndicatorColours;
}

export interface MockBoardOptions {
  id: string;
  connection: ConnectionKind;
  definition: KeyboardDefinition;
  battery?: BatteryStatus;
  name?: string;
  /** Simulated round-trip time per call. Default 0. */
  latencyMs?: number;
  /** What reboot() does; the manager supplies it. Without it the board can't reboot. */
  onReboot?: (board: MockBoard, bootloader: boolean) => void;
}

export class MockBoard implements Board {
  readonly info: BoardInfo;
  readonly definition: KeyboardDefinition;

  private saved: StoredState;
  private live: StoredState;
  private battery: BatteryStatus;
  private connected = true;
  /** Bluetooth as the firmware keeps it: profiles and preference survive reboots. */
  private link: { preferred: "usb" | "ble"; bluetooth: BluetoothState; profile: number; paired: boolean[] };
  private readonly latencyMs: number;
  private readonly onReboot: ((board: MockBoard, bootloader: boolean) => void) | undefined;
  private readonly listeners = new Set<(event: BoardEvent) => void>();

  constructor(options: MockBoardOptions) {
    const keymap = defaultKeymap(options.definition.name);
    if (!keymap) {
      throw new Error(`no default keymap for ${options.definition.name}`);
    }

    this.definition = options.definition;
    this.info = {
      id: options.id,
      model: options.definition.name,
      connection: options.connection,
      firmwareVersion: MOCK_FIRMWARE_VERSION,
      protocolVersion: MOCK_PROTOCOL_VERSION,
      layerCount: LAYER_COUNT,
    };
    this.saved = {
      keymap,
      settings: { ...DEFAULT_SETTINGS, name: options.name ?? "" },
      advanced: defaultAdvancedKeys(),
      macros: [],
      indicators: defaultIndicatorColours(LAYER_COUNT),
    };
    this.live = structuredClone(this.saved);
    this.battery = options.battery ?? { percent: 100, charging: false };
    this.latencyMs = options.latencyMs ?? 0;
    this.onReboot = options.onReboot;
    // Paired with this computer on host 1, and with another on host 2.
    const paired = Array.from({ length: BLE_PROFILE_COUNT }, (_, i) => i < 2);
    this.link = {
      preferred: "usb",
      bluetooth: options.connection === "ble" || options.connection === "usb" ? "connected" : "advertising",
      profile: 0,
      paired,
    };
  }

  private async call<T>(fn: () => T): Promise<T> {
    if (this.latencyMs > 0) {
      await new Promise((resolve) => setTimeout(resolve, this.latencyMs));
    }
    if (!this.connected) {
      throw new BoardError("disconnected", `${this.info.id} is not connected`);
    }
    return fn();
  }

  private emit(event: BoardEvent): void {
    for (const listener of this.listeners) listener(event);
  }

  private checkLayer(layer: number): void {
    if (!Number.isInteger(layer) || layer < 0 || layer >= this.info.layerCount) {
      throw new BoardError("out-of-range", `layer ${layer} out of range`);
    }
  }

  private checkKeycode(keycode: KeycodeId): void {
    if (!isValidKeycode(keycode, this.info.layerCount)) {
      throw new BoardError("invalid-value", `unknown keycode ${keycode}`);
    }
  }

  getBattery(): Promise<BatteryStatus | null> {
    return this.call(() => ({ ...this.battery }));
  }

  private timing: InputTiming = {
    scanPeriodUs: 1000,
    scanAvgUs: 92,
    scanMaxUs: 122,
    reportAvgUs: 31,
    reportMaxUs: 61,
    keyWakes: 0,
    scans: 0,
  };

  /** Simulated counters: a scan a millisecond since the last reset, a key wake every few hundred. */
  getTiming(reset = false): Promise<InputTiming> {
    return this.call(() => {
      const now = Date.now();
      const scans = Math.floor(now - this.timingSince);
      const out = { ...this.timing, scans, keyWakes: Math.floor(scans / 400) };
      if (reset) this.timingSince = now;
      return out;
    });
  }
  private timingSince = Date.now();

  getKeymap(): Promise<Keymap> {
    return this.call(() => structuredClone(this.live.keymap));
  }

  setKey({ layer, row, col }: KeyPosition, keycode: KeycodeId): Promise<void> {
    return this.call(() => {
      this.checkLayer(layer);
      const { rows, cols } = this.definition.matrix;
      if (!Number.isInteger(row) || !Number.isInteger(col) || row < 0 || row >= rows || col < 0 || col >= cols) {
        throw new BoardError("out-of-range", `matrix position (${row}, ${col}) out of range`);
      }
      if (!isMatrixPositionUsed(this.definition, row, col)) {
        throw new BoardError("out-of-range", `no switch at (${row}, ${col})`);
      }
      this.checkKeycode(keycode);
      this.live.keymap.keys[layer]![row]![col] = keycode;
      this.emit({ type: "changed" });
    });
  }

  setEncoder(layer: number, encoder: number, action: EncoderAction): Promise<void> {
    return this.call(() => {
      this.checkLayer(layer);
      if (!this.definition.encoders.some((e) => e.id === encoder)) {
        throw new BoardError("out-of-range", `encoder ${encoder} out of range`);
      }
      this.checkKeycode(action.cw);
      this.checkKeycode(action.ccw);
      this.live.keymap.encoders[layer]![encoder] = { cw: action.cw, ccw: action.ccw };
      this.emit({ type: "changed" });
    });
  }

  getSettings(): Promise<BoardSettings> {
    return this.call(() => ({ ...this.live.settings }));
  }

  setSettings(patch: Partial<BoardSettings>): Promise<BoardSettings> {
    return this.call(() => {
      const next = { ...this.live.settings, ...patch };
      checkSettings(next);
      this.live.settings = next;
      this.emit({ type: "changed" });
      return { ...next };
    });
  }

  getConnection(): Promise<ConnectionStatus> {
    return this.call(() => {
      const { preferred, bluetooth, profile, paired } = this.link;
      const usb = this.info.connection === "usb";
      const ready = (o: "usb" | "ble") => (o === "usb" ? usb : bluetooth === "connected");
      const other = preferred === "usb" ? "ble" : "usb";
      const active =
        this.info.connection === "dongle" ? "dongle" : ready(preferred) ? preferred : ready(other) ? other : null;
      return { active, preferred, usb, bluetooth, profile, paired: [...paired] };
    });
  }

  setOutput(output: "usb" | "ble"): Promise<void> {
    return this.call(() => {
      if (output !== "usb" && output !== "ble") throw new BoardError("invalid-value", `can't prefer output ${String(output)}`);
      this.link.preferred = output;
    });
  }

  private checkProfile(profile: number): void {
    if (!Number.isInteger(profile) || profile < 0 || profile >= BLE_PROFILE_COUNT) {
      throw new BoardError("out-of-range", `there's no Bluetooth host ${profile + 1}`);
    }
  }

  selectProfile(profile: number): Promise<void> {
    return this.call(() => {
      this.checkProfile(profile);
      this.link.profile = profile;
      // A paired host reconnects by itself; otherwise the board waits for one.
      this.link.bluetooth = this.link.paired[profile] ? "connected" : "advertising";
    });
  }

  clearProfile(profile: number): Promise<void> {
    return this.call(() => {
      this.checkProfile(profile);
      this.link.paired[profile] = false;
      if (profile === this.link.profile) this.link.bluetooth = "advertising";
    });
  }

  getIndicators(): Promise<IndicatorColours> {
    return this.call(() => structuredClone(this.live.indicators));
  }

  setIndicators(patch: Partial<IndicatorColours>): Promise<IndicatorColours> {
    return this.call(() => {
      const next = { ...this.live.indicators, ...patch };
      checkIndicatorColours(next, this.info.layerCount);
      this.live.indicators = structuredClone(next);
      this.emit({ type: "changed" });
      return structuredClone(next);
    });
  }

  hasUnsavedChanges(): Promise<boolean> {
    return this.call(() => JSON.stringify(this.live) !== JSON.stringify(this.saved));
  }

  save(): Promise<void> {
    return this.call(() => {
      this.saved = structuredClone(this.live);
      this.emit({ type: "saved" });
    });
  }

  revert(): Promise<void> {
    return this.call(() => {
      this.live = structuredClone(this.saved);
      this.emit({ type: "reverted" });
    });
  }

  factoryReset(): Promise<void> {
    return this.call(() => {
      const keymap = defaultKeymap(this.definition.name)!;
      this.saved = {
        keymap,
        settings: { ...DEFAULT_SETTINGS },
        advanced: defaultAdvancedKeys(),
        macros: [],
        indicators: defaultIndicatorColours(LAYER_COUNT),
      };
      this.live = structuredClone(this.saved);
      this.emit({ type: "reverted" });
    });
  }

  /** Restart: unsaved edits are lost, as on a real board. The manager unplugs it and plugs it back. */
  reboot(bootloader = false): Promise<void> {
    return this.call(() => {
      this.onReboot?.(this, bootloader);
    });
  }

  /** Back from a reboot (with new firmware if a version is given). */
  reconnect(firmwareVersion?: string): void {
    if (firmwareVersion) (this.info as { firmwareVersion: string }).firmwareVersion = firmwareVersion;
    this.live = structuredClone(this.saved);
    this.connected = true;
  }

  identify(): Promise<void> {
    return this.call(() => undefined);
  }

  private get advancedContext(): AdvancedContext {
    return {
      layerCount: this.info.layerCount,
      rows: this.definition.matrix.rows,
      cols: this.definition.matrix.cols,
    };
  }

  getAdvancedKeys(): Promise<AdvancedKeys> {
    return this.call(() => structuredClone(this.live.advanced));
  }

  setAdvancedEntry<T extends AdvancedTable>(table: T, index: number, value: AdvancedKeys[T][number]): Promise<void> {
    return this.call(() => {
      if (!(table in ADVANCED_SLOTS)) {
        throw new BoardError("out-of-range", `unknown table ${String(table)}`);
      }
      if (!Number.isInteger(index) || index < 0 || index >= ADVANCED_SLOTS[table]) {
        throw new BoardError("out-of-range", `${table} slot ${index} out of range`);
      }
      checkAdvancedEntry(table, value, this.advancedContext);
      (this.live.advanced[table] as AdvancedKeys[T][number][])[index] = structuredClone(value);
      this.emit({ type: "changed" });
    });
  }

  setAdvancedOptions(patch: Partial<AdvancedOptions>): Promise<AdvancedOptions> {
    return this.call(() => {
      const current = this.live.advanced.options;
      const next: AdvancedOptions = { ...current, ...patch, mouse: { ...current.mouse, ...patch.mouse } };
      checkAdvancedOptions(next);
      this.live.advanced.options = next;
      this.emit({ type: "changed" });
      return structuredClone(next);
    });
  }

  getMacros(): Promise<MacroStep[][]> {
    return this.call(() => structuredClone(this.live.macros));
  }

  setMacros(macros: MacroStep[][]): Promise<void> {
    return this.call(() => {
      checkMacros(macros, this.advancedContext);
      // Stored as the board reads them back (text runs joined, no trailing empty macros).
      this.live.macros = decodeMacros(encodeMacros(macros));
      this.emit({ type: "changed" });
    });
  }

  subscribe(listener: (event: BoardEvent) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  /** Test and dev hook: change the battery and notify listeners. */
  setBattery(battery: BatteryStatus): void {
    this.battery = {
      percent: Math.max(0, Math.min(100, Math.round(battery.percent))),
      charging: battery.charging,
    };
    this.emit({ type: "battery", battery: { ...this.battery } });
  }

  /** One simulation step: USB charges toward full, wireless drains slowly. */
  tick(): void {
    const { percent, charging } = this.battery;
    if (this.info.connection === "usb") {
      const next = Math.min(100, percent + 1);
      this.setBattery({ percent: next, charging: next < 100 });
    } else if (percent > 5) {
      this.setBattery({ percent: percent - 1, charging: false });
    } else if (charging) {
      this.setBattery({ percent, charging: false });
    }
  }

  /** Called by the manager when the board is unplugged. */
  disconnect(): void {
    this.connected = false;
    this.emit({ type: "disconnected" });
    this.listeners.clear();
  }
}

export interface MockBoardManagerOptions {
  /** Keyboard definitions by model name, e.g. { Dachshund60F: def }. */
  definitions: Record<string, KeyboardDefinition>;
  /** Battery simulation interval; 0 turns simulation off. Default 0. */
  tickMs?: number;
  latencyMs?: number;
}

/** How long a simulated board takes to come back after a reboot or an update. */
const MOCK_REBOOT_MS = 1200;

export class MockBoardManager implements BoardManager {
  private readonly boards = new Map<string, MockBoard>();
  /** Boards sitting in their (simulated) UF2 bootloader, by id. */
  private readonly inBootloader = new Map<string, MockBoard>();
  private readonly listeners = new Set<(boards: Board[]) => void>();
  private readonly definitions: Record<string, KeyboardDefinition>;
  private readonly latencyMs: number;
  private timer: ReturnType<typeof setInterval> | undefined;
  private nextSerial = 1;

  constructor(options: MockBoardManagerOptions) {
    this.definitions = options.definitions;
    this.latencyMs = options.latencyMs ?? 0;
    if (options.tickMs && options.tickMs > 0) {
      this.timer = setInterval(() => {
        for (const board of this.boards.values()) board.tick();
      }, options.tickMs);
    }
  }

  list(): MockBoard[] {
    return [...this.boards.values()];
  }

  subscribe(listener: (boards: Board[]) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private notify(): void {
    const boards = this.list();
    for (const listener of this.listeners) listener(boards);
  }

  /** Simulate plugging in a board (or its dongle). */
  plug(
    model: string,
    connection: ConnectionKind,
    options: { battery?: BatteryStatus; name?: string; id?: string } = {},
  ): MockBoard {
    const definition = this.definitions[model];
    if (!definition) {
      throw new Error(`no keyboard definition for ${model}`);
    }
    const id = options.id ?? `D60F-${(0x1a2b + this.nextSerial++ * 0x3c1).toString(16).toUpperCase().padStart(4, "0")}`;
    if (this.boards.has(id)) {
      throw new Error(`board ${id} is already connected`);
    }

    const board = new MockBoard({
      id,
      connection,
      definition,
      latencyMs: this.latencyMs,
      onReboot: (b, bootloader) => this.rebooted(b, bootloader),
      ...(options.battery ? { battery: options.battery } : {}),
      ...(options.name !== undefined ? { name: options.name } : {}),
    });
    this.boards.set(id, board);
    this.notify();
    return board;
  }

  private rebooted(board: MockBoard, bootloader: boolean): void {
    const id = board.info.id;
    // After the reply, as the firmware does.
    setTimeout(() => {
      if (this.boards.get(id) !== board) return;
      this.boards.delete(id);
      board.disconnect();
      this.notify();
      if (bootloader) {
        this.inBootloader.set(id, board);
      } else {
        setTimeout(() => this.comeBack(board), MOCK_REBOOT_MS);
      }
    }, 0);
  }

  private comeBack(board: MockBoard, firmwareVersion?: string): void {
    if (this.boards.has(board.info.id)) return;
    board.reconnect(firmwareVersion);
    this.boards.set(board.info.id, board);
    this.notify();
  }

  /** Ids of boards waiting in their bootloader for firmware. */
  bootloaderIds(): string[] {
    return [...this.inBootloader.keys()];
  }

  /**
   * Simulate copying firmware onto a board's UF2 drive: it restarts with
   * that version and its saved settings. Resolves once it's back.
   */
  async flash(id: string, firmwareVersion: string): Promise<void> {
    const board = this.inBootloader.get(id);
    if (!board) throw new Error(`${id} isn't in its bootloader`);
    this.inBootloader.delete(id);
    await new Promise((resolve) => setTimeout(resolve, MOCK_REBOOT_MS));
    this.comeBack(board, firmwareVersion);
  }

  /** Simulate unplugging a board. */
  unplug(id: string): void {
    const board = this.boards.get(id);
    if (!board) return;
    this.boards.delete(id);
    board.disconnect();
    this.notify();
  }

  dispose(): void {
    if (this.timer) clearInterval(this.timer);
    this.timer = undefined;
    for (const id of [...this.boards.keys()]) this.unplug(id);
    this.listeners.clear();
  }
}
