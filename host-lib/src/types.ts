/**
 * Typed board API shared by every transport (mock, hidapi, WebHID).
 *
 * The GUI talks only to these types; no raw protocol bytes leak out. HidBoard
 * (hid-board.ts) implements Board over the config protocol for real boards;
 * MockBoard (mock.ts) simulates one.
 */
import type { MatrixPosition } from "./keyboard-definition.js";

/** How the host reaches the board. */
export type ConnectionKind = "usb" | "dongle" | "ble";

export interface BatteryStatus {
  /** 0-100. */
  percent: number;
  charging: boolean;
}

export interface BoardInfo {
  /** USB serial number: unique per board, stable across connections. */
  id: string;
  /** Keyboard definition name, e.g. "Dachshund60F" (boards/<model>/). */
  model: string;
  connection: ConnectionKind;
  firmwareVersion: string;
  protocolVersion: number;
  layerCount: number;
  /**
   * What the board's firmware can do, by protocol capability name (e.g.
   * "keymap", "persistence"). Absent on the mock, which can do everything.
   */
  capabilities?: readonly string[];
}

/** Debounce algorithms, in the firmware's order (kb/debounce.h). */
export const DEBOUNCE_ALGORITHMS = [
  "defer-global",
  "defer-per-key",
  "eager-global",
  "eager-per-key",
  "eager-press",
] as const;
export type DebounceAlgorithm = (typeof DEBOUNCE_ALGORITHMS)[number];

export interface BoardSettings {
  /** The owner's name for this board, shown in the app. Up to 16 bytes of UTF-8. */
  name: string;
  debounceAlgorithm: DebounceAlgorithm;
  debounceMs: number;
  tappingTermMs: number;
  nkro: boolean;
  /** Minutes of inactivity before sleeping; 0 = never. */
  sleepTimeoutMin: number;
  /** Indicator LED brightness, 0-100. */
  indicatorBrightness: number;
}

/** Inclusive limits the firmware enforces (kb/settings.h). */
export const SETTINGS_LIMITS = {
  /** In bytes of UTF-8, so 16 letters of plain ASCII. */
  nameMaxLength: 16,
  debounceMs: { min: 0, max: 100 },
  tappingTermMs: { min: 50, max: 2000 },
  sleepTimeoutMin: { min: 0, max: 240 },
  indicatorBrightness: { min: 0, max: 100 },
} as const;

/** A colour as "#rrggbb" (lowercase); "#000000" leaves the LED off. */
export type Colour = string;

/** What each indicator LED shows, before the brightness setting scales it. */
export interface IndicatorColours {
  /** The layer LED's colour on each layer (one per layer the board has). */
  layers: Colour[];
  caps: Colour;
  num: Colour;
  scroll: Colour;
  /** The battery gauge. */
  battery: Colour;
  /** The whole gauge at or below batteryLowPercent. */
  batteryLow: Colour;
  batteryLowPercent: number;
}

/**
 * Keycode identifier, e.g. "KC_A", "MO(1)", "BT_SEL(0)". The numeric 16-bit
 * encoding is defined by the firmware keycode model (phase 3) and protocol
 * (phase 5); the GUI never sees it.
 */
export type KeycodeId = string;

export interface KeyPosition {
  layer: number;
  row: number;
  col: number;
}

export interface EncoderAction {
  cw: KeycodeId;
  ccw: KeycodeId;
}

export interface Keymap {
  /** keys[layer][row][col] */
  keys: KeycodeId[][][];
  /** encoders[layer][encoderId] */
  encoders: EncoderAction[][];
}

/** Pressing all keys (2-4) within timeoutMs acts as result. result KC_NO = an unused slot. */
export interface Combo {
  keys: MatrixPosition[];
  result: KeycodeId;
  /** 0 = the firmware default (50 ms). */
  timeoutMs: number;
  /** Layers it works on; empty = all. */
  layers: number[];
}

/** TD(n): tap once, hold, tap twice or three times. KC_NO actions fall back to tap. */
export interface TapDance {
  tap: KeycodeId;
  hold: KeycodeId;
  doubleTap: KeycodeId;
  tripleTap: KeycodeId;
  /** 0 = the tapping term. */
  termMs: number;
}

/**
 * With exactly these modifiers held (either side counts), triggerKey sends
 * replacement without them; e.g. Shift + Backspace = Delete. replacement
 * KC_NO = an unused slot.
 */
export interface KeyOverride {
  /** HID modifier bits: 0x01 Ctrl, 0x02 Shift, 0x04 Alt, 0x08 GUI (right-hand ones count the same). */
  triggerMods: number;
  /** A basic key, e.g. "KC_BSPC". */
  triggerKey: KeycodeId;
  replacement: KeycodeId;
  layers: number[];
}

/** QK_LEAD, then these basic keys (1-4), taps result. result KC_NO = unused. */
export interface LeaderSequence {
  keys: KeycodeId[];
  result: KeycodeId;
}

/** Tap-hold behaviour for one layer-tap or mod-tap key. null in a list = an unused slot. */
export interface TapHoldRule {
  row: number;
  col: number;
  permissiveHold: boolean;
  holdOnOtherKeyPress: boolean;
  retroTap: boolean;
  tappingTermMs: number;
  /** 0 = off. */
  quickTapMs: number;
}

export interface AdvancedOptions {
  /** Time allowed per key of a Leader sequence. */
  leaderTimeoutMs: number;
  /** Both Shifts together toggle Caps Word. */
  capsWordOnBothShifts: boolean;
  /** Caps Word turns off after this long without typing; 0 = never. */
  capsWordIdleMs: number;
  /** Auto Shift on at power-up (AS_TOGG flips it). */
  autoShift: boolean;
  /** Hold a key this long to get it shifted. */
  autoShiftMs: number;
  mouse: {
    intervalMs: number;
    /** Starting speed, pixels per report. */
    moveDelta: number;
    maxSpeed: number;
    timeToMaxMs: number;
    wheelIntervalMs: number;
  };
}

/** Everything the advanced-key editors edit, as the board has it live. */
export interface AdvancedKeys {
  options: AdvancedOptions;
  combos: Combo[];
  tapDances: TapDance[];
  keyOverrides: KeyOverride[];
  leader: LeaderSequence[];
  tapHold: (TapHoldRule | null)[];
}

/** The tables of AdvancedKeys that hold one entry per slot. */
export type AdvancedTable = "combos" | "tapDances" | "keyOverrides" | "leader" | "tapHold";

/** One step of a macro (M(n)). */
export type MacroStep =
  | { type: "tap" | "down" | "up"; keycode: KeycodeId }
  | { type: "delay"; ms: number }
  /** Printable ASCII, typed on a US layout. */
  | { type: "text"; text: string };

export type BoardEvent =
  | { type: "battery"; battery: BatteryStatus }
  | { type: "changed" } // keymap or settings changed (live, not yet saved)
  | { type: "saved" }
  | { type: "reverted" }
  | { type: "disconnected" };

/** Error codes from SPEC.md's protocol error list, plus transport failures. */
export type BoardErrorCode =
  | "out-of-range"
  | "invalid-value"
  | "busy"
  | "storage-full"
  | "storage-error"
  | "version-mismatch"
  | "not-supported"
  | "timeout"
  | "protocol-error"
  | "disconnected";

export class BoardError extends Error {
  readonly code: BoardErrorCode;

  constructor(code: BoardErrorCode, message: string) {
    super(message);
    this.name = "BoardError";
    this.code = code;
  }
}

export interface Board {
  readonly info: BoardInfo;

  getBattery(): Promise<BatteryStatus | null>;

  /** Live keymap (RAM on the board), including unsaved edits. */
  getKeymap(): Promise<Keymap>;
  setKey(position: KeyPosition, keycode: KeycodeId): Promise<void>;
  setEncoder(layer: number, encoder: number, action: EncoderAction): Promise<void>;

  getSettings(): Promise<BoardSettings>;
  setSettings(patch: Partial<BoardSettings>): Promise<BoardSettings>;

  /** Indicator colours (live). Absent on boards without settable colours. */
  getIndicators?(): Promise<IndicatorColours>;
  /** Change some of them; layers, if given, has one colour per layer. */
  setIndicators?(patch: Partial<IndicatorColours>): Promise<IndicatorColours>;

  /** True when live keymap or settings differ from what is saved in flash. */
  hasUnsavedChanges(): Promise<boolean>;
  save(): Promise<void>;
  revert(): Promise<void>;
  factoryReset(): Promise<void>;

  /** Flash the board's indicators so the user can tell which one it is. */
  identify(): Promise<void>;

  /**
   * Restart the board; with bootloader set, into its UF2 bootloader for a
   * firmware update. It drops off the bus. Absent on boards that can't.
   */
  reboot?(bootloader: boolean): Promise<void>;

  /**
   * Advanced keys (combos, tap dance, overrides, Leader, tap-hold rules and
   * their options). Absent on boards that don't have them.
   */
  getAdvancedKeys?(): Promise<AdvancedKeys>;
  /** Replace slot index of a table (live, like setKey). */
  setAdvancedEntry?<T extends AdvancedTable>(table: T, index: number, value: AdvancedKeys[T][number]): Promise<void>;
  setAdvancedOptions?(patch: Partial<AdvancedOptions>): Promise<AdvancedOptions>;

  /** Macros M(0)... in order. Absent on boards without macros. */
  getMacros?(): Promise<MacroStep[][]>;
  /** Replace every macro at once. "storage-full" if they don't fit the board's buffer. */
  setMacros?(macros: MacroStep[][]): Promise<void>;

  subscribe(listener: (event: BoardEvent) => void): () => void;
}

export interface BoardManager {
  list(): Board[];
  /** Called with the full list whenever a board connects or disconnects. */
  subscribe(listener: (boards: Board[]) => void): () => void;
}
