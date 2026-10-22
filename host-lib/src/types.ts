/**
 * Typed board API shared by every transport (mock, hidapi, WebHID).
 *
 * The GUI talks only to these types; no raw protocol bytes leak out. Real
 * transports will implement Board on top of the bindings generated from
 * protocol/protocol.yaml (phases 5 and 7).
 */

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
}

export interface BoardSettings {
  /** The owner's name for this board, shown in the app. Up to 16 characters. */
  name: string;
  debounceMs: number;
  tappingTermMs: number;
  nkro: boolean;
  /** Minutes of inactivity before sleeping; 0 = never. */
  sleepTimeoutMin: number;
  /** Indicator LED brightness, 0-100. */
  indicatorBrightness: number;
}

/** Inclusive limits the firmware enforces; the GUI uses them for its controls. */
export const SETTINGS_LIMITS = {
  nameMaxLength: 16,
  debounceMs: { min: 1, max: 20 },
  tappingTermMs: { min: 100, max: 500 },
  sleepTimeoutMin: { min: 0, max: 120 },
  indicatorBrightness: { min: 0, max: 100 },
} as const;

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

export type BoardEvent =
  | { type: "battery"; battery: BatteryStatus }
  | { type: "changed" } // keymap or settings changed (live, not yet saved)
  | { type: "saved" }
  | { type: "reverted" }
  | { type: "disconnected" };

/** Error codes from SPEC.md's protocol error list. */
export type BoardErrorCode =
  | "out-of-range"
  | "invalid-value"
  | "busy"
  | "storage-full"
  | "version-mismatch"
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

  /** True when live keymap or settings differ from what is saved in flash. */
  hasUnsavedChanges(): Promise<boolean>;
  save(): Promise<void>;
  revert(): Promise<void>;
  factoryReset(): Promise<void>;

  /** Flash the board's indicators so the user can tell which one it is. */
  identify(): Promise<void>;

  subscribe(listener: (event: BoardEvent) => void): () => void;
}

export interface BoardManager {
  list(): Board[];
  /** Called with the full list whenever a board connects or disconnects. */
  subscribe(listener: (boards: Board[]) => void): () => void;
}
