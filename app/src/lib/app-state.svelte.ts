/**
 * Application state: connected boards and the editing session for each.
 *
 * Boards come from host-lib board managers: real boards over WebHID (Chrome
 * and Edge), plus simulated Dachshund60Fs in demo mode, which is on when the
 * browser has no WebHID or the URL has ?demo.
 */

import {
  BoardError,
  CombinedBoardManager,
  restoreBoard,
  MockBoardManager,
  TauriHidBoardManager,
  webHid,
  WebHidBoardManager,
  type AdvancedKeys,
  type AdvancedOptions,
  type AdvancedTable,
  type BatteryStatus,
  type BoardBackup,
  type Board,
  type BoardManager,
  type BoardSettings,
  type ConnectionKind,
  type EncoderAction,
  type IndicatorColours,
  type KeyboardDefinition,
  type KeyPosition,
  type Keymap,
  type KeycodeId,
  type MacroStep,
  protocol,
} from "@dachshund60f/host-lib";
import dachshund60f from "@boards/Dachshund60F/keyboard.json";
import pup from "@boards/Pup/keyboard.json";

import { desktopApp, inDesktopApp, type AppUpdate, type Desktop } from "./desktop.ts";
import { FirmwareUpdate } from "./firmware-update.svelte.ts";
import { EditHistory, type Edit } from "./history.ts";

const { PROTOCOL_VERSION } = protocol;

export const definitions: Record<string, KeyboardDefinition> = {
  Dachshund60F: dachshund60f as KeyboardDefinition,
  Pup: pup as KeyboardDefinition,
};

/** Dog breed shown for each board model. */
export const BREEDS: Record<string, string> = {
  Dachshund60F: "Dachshund",
  Pup: "Puppy",
};

/** How each link is named in messages. */
export const CONNECTION_NAMES = { usb: "USB", dongle: "the 2.4 GHz dongle", ble: "Bluetooth" } as const;

export type Toast = { id: number; text: string; kind: "info" | "success" | "error" };

function errorText(err: unknown): string {
  if (err instanceof BoardError) return err.message;
  if (err instanceof Error) return err.message;
  return String(err);
}

const BATTERY_POLL_MS = 30_000;

/** Protocol capability names a board's features need (see protocol.yaml). */
export type Feature =
  | "keymap"
  | "persistence"
  | "settings"
  | "encoders"
  | "identify"
  | "battery"
  | "advancedKeys"
  | "macros"
  | "indicators"
  | "bluetooth"
  | "timing"
  | "dongle";

export class BoardSession {
  readonly board: Board;
  /** Null only for a board on another protocol version, which can't say what it is. */
  readonly definition: KeyboardDefinition | null;

  battery = $state<BatteryStatus | null>(null);
  settings = $state<BoardSettings | null>(null);
  keymap = $state<Keymap | null>(null);
  /** Null when the board has no advanced keys (or macros). */
  advanced = $state<AdvancedKeys | null>(null);
  macros = $state<MacroStep[][] | null>(null);
  /** Null when the board's indicator colours can't be set. */
  indicators = $state<IndicatorColours | null>(null);
  unsaved = $state(false);
  busy = $state(false);
  canUndo = $state(false);
  canRedo = $state(false);
  /** Why the board can't be edited (another protocol version, a failed load), or null. */
  problem = $state<string | null>(null);

  private readonly history = new EditHistory();
  private readonly unsubscribe: () => void;
  private batteryTimer: ReturnType<typeof setInterval> | null = null;
  private readonly app: AppState;

  constructor(board: Board, app: AppState) {
    this.board = board;
    this.app = app;
    this.definition = definitions[board.info.model] ?? null;

    this.unsubscribe = board.subscribe((event) => {
      switch (event.type) {
        case "battery":
          this.battery = event.battery;
          break;
        case "changed":
        case "saved":
          void this.refreshUnsaved();
          break;
        case "reverted":
          void this.load();
          break;
      }
    });
  }

  get id(): string {
    return this.board.info.id;
  }

  /** Whether the board's firmware has a feature. Boards that don't list capabilities have all. */
  supports(feature: Feature): boolean {
    const caps = this.board.info.capabilities;
    return caps === undefined || caps.includes(feature);
  }

  get connection(): ConnectionKind {
    return this.board.info.connection;
  }

  /** The owner's name for the board, falling back to the model. */
  get displayName(): string {
    return this.settings?.name.trim() || this.board.info.model || "Unknown board";
  }

  async load(): Promise<void> {
    try {
      // One after another: a real board answers one request at a time anyway.
      this.keymap = await this.board.getKeymap();
      this.unsaved = await this.board.hasUnsavedChanges();
      this.settings = this.supports("settings") ? await this.board.getSettings() : null;
      this.advanced =
        this.supports("advancedKeys") && this.board.getAdvancedKeys ? await this.board.getAdvancedKeys() : null;
      this.macros = this.supports("macros") && this.board.getMacros ? await this.board.getMacros() : null;
      this.indicators =
        this.supports("indicators") && this.board.getIndicators ? await this.board.getIndicators() : null;
      this.battery = await this.board.getBattery();
      this.watchBattery();
      this.problem = null;
    } catch (err) {
      if (err instanceof BoardError && err.code === "version-mismatch") {
        this.problem =
          `${this.board.info.model || "This board"} speaks protocol v${this.board.info.protocolVersion}, ` +
          `and this Doghouse speaks v${PROTOCOL_VERSION}. Update ${this.board.info.protocolVersion > PROTOCOL_VERSION ? "Doghouse" : "the board's firmware"} to edit it.`;
      } else {
        this.problem = `Couldn't read the board: ${errorText(err)}`;
      }
    }
  }

  private async refreshUnsaved(): Promise<void> {
    try {
      this.unsaved = await this.board.hasUnsavedChanges();
    } catch {
      /* board went away; the manager will drop the session */
    }
  }

  private syncHistory(): void {
    this.canUndo = this.history.canUndo;
    this.canRedo = this.history.canRedo;
  }

  private async run<T>(fn: () => Promise<T>): Promise<T | undefined> {
    this.busy = true;
    try {
      return await fn();
    } catch (err) {
      this.app.notify(errorText(err), "error");
      return undefined;
    } finally {
      this.busy = false;
    }
  }

  private async applyKey(layer: number, row: number, col: number, keycode: KeycodeId): Promise<void> {
    const pos: KeyPosition = { layer, row, col };
    await this.board.setKey(pos, keycode);
    this.keymap!.keys[layer]![row]![col] = keycode;
  }

  private async applyEncoder(layer: number, encoder: number, action: EncoderAction): Promise<void> {
    await this.board.setEncoder(layer, encoder, action);
    this.keymap!.encoders[layer]![encoder] = { ...action };
  }

  async setKey(layer: number, row: number, col: number, keycode: KeycodeId): Promise<void> {
    if (!this.keymap) return;
    const before = this.keymap.keys[layer]?.[row]?.[col];
    if (before === undefined || before === keycode) return;
    await this.run(async () => {
      await this.applyKey(layer, row, col, keycode);
      this.history.record({ kind: "key", layer, row, col, before, after: keycode });
      this.syncHistory();
    });
  }

  async setEncoder(layer: number, encoder: number, action: EncoderAction): Promise<void> {
    if (!this.keymap) return;
    const current = this.keymap.encoders[layer]?.[encoder];
    if (!current) return;
    const before = { ...current };
    await this.run(async () => {
      await this.applyEncoder(layer, encoder, action);
      this.history.record({ kind: "encoder", layer, encoder, before, after: { ...action } });
      this.syncHistory();
    });
  }

  private async apply(edit: Edit, direction: "before" | "after"): Promise<void> {
    if (edit.kind === "key") {
      await this.applyKey(edit.layer, edit.row, edit.col, edit[direction]);
    } else {
      await this.applyEncoder(edit.layer, edit.encoder, edit[direction]);
    }
  }

  async undo(): Promise<void> {
    const edit = this.history.undo();
    this.syncHistory();
    if (edit) await this.run(() => this.apply(edit, "before"));
  }

  async redo(): Promise<void> {
    const edit = this.history.redo();
    this.syncHistory();
    if (edit) await this.run(() => this.apply(edit, "after"));
  }

  async updateSettings(patch: Partial<BoardSettings>): Promise<void> {
    const next = await this.run(() => this.board.setSettings(patch));
    if (next) this.settings = next;
  }

  async updateIndicators(patch: Partial<IndicatorColours>): Promise<void> {
    const set = this.board.setIndicators?.bind(this.board);
    if (!set) return;
    const next = await this.run(() => set(patch));
    if (next) this.indicators = next;
  }

  /** Replace one slot of an advanced-key table. Resolves true if the board took it. */
  async setAdvancedEntry<T extends AdvancedTable>(
    table: T,
    index: number,
    value: AdvancedKeys[T][number],
  ): Promise<boolean> {
    const set = this.board.setAdvancedEntry;
    if (!this.advanced || !set) return false;
    const ok = await this.run(async () => {
      await set.call(this.board, table, index, value);
      (this.advanced![table] as AdvancedKeys[T][number][])[index] = $state.snapshot(value) as AdvancedKeys[T][number];
      return true;
    });
    return ok ?? false;
  }

  async updateAdvancedOptions(patch: Partial<AdvancedOptions>): Promise<void> {
    const set = this.board.setAdvancedOptions;
    if (!this.advanced || !set) return;
    const next = await this.run(() => set.call(this.board, patch));
    if (next) this.advanced.options = next;
  }

  /** Replace every macro. Resolves true if the board took them. */
  async setMacros(macros: MacroStep[][]): Promise<boolean> {
    const set = this.board.setMacros;
    if (!this.macros || !set) return false;
    const ok = await this.run(async () => {
      await set.call(this.board, macros);
      // Read back: the board joins text runs and drops trailing empty macros.
      this.macros = await this.board.getMacros!();
      return true;
    });
    return ok ?? false;
  }

  async save(): Promise<void> {
    await this.run(async () => {
      await this.board.save();
      this.history.clear();
      this.syncHistory();
      this.app.notify(`Good dog! ${this.displayName} saved its settings.`, "success");
    });
  }

  async revert(): Promise<void> {
    await this.run(async () => {
      await this.board.revert();
      this.history.clear();
      this.syncHistory();
      this.app.notify("Back to the last saved state.", "info");
    });
  }

  async factoryReset(): Promise<void> {
    await this.run(async () => {
      await this.board.factoryReset();
      this.history.clear();
      this.syncHistory();
      this.app.notify("Factory settings restored.", "info");
    });
  }

  async identify(): Promise<void> {
    await this.run(async () => {
      await this.board.identify();
      this.app.notify(`${this.displayName} is wagging its lights.`, "info");
    });
  }

  /** Restart into the UF2 bootloader; the board leaves and its drive appears. */
  async enterBootloader(): Promise<void> {
    const reboot = this.board.reboot;
    if (!reboot) return;
    await this.run(async () => {
      await reboot.call(this.board, true);
      this.app.notify(`${this.displayName} is off to the bootloader. Copy the .uf2 file onto its drive.`, "info");
    });
  }

  /** Put back a backup file's keymap and settings, and save. */
  async restoreBackup(file: File): Promise<void> {
    await this.run(async () => {
      let backup: BoardBackup;
      try {
        backup = JSON.parse(await file.text()) as BoardBackup;
      } catch {
        throw new Error(`${file.name} isn't a Doghouse backup.`);
      }
      const writes = await restoreBoard(this.board, backup);
      await this.load();
      this.history.clear();
      this.syncHistory();
      this.app.notify(
        writes ? `${this.displayName} has its settings back from ${file.name}.` : `${this.displayName} already matches that backup.`,
        "success",
      );
    });
  }

  /** Real boards don't announce battery changes: ask every so often (the board reads its cell once a minute). */
  private watchBattery(): void {
    if (this.batteryTimer || !this.board.info.capabilities?.includes("battery")) return;
    this.batteryTimer = setInterval(() => {
      this.board.getBattery().then(
        (b) => (this.battery = b),
        () => {
          /* gone; the manager drops the session */
        },
      );
    }, BATTERY_POLL_MS);
  }

  dispose(): void {
    this.unsubscribe();
    if (this.batteryTimer) clearInterval(this.batteryTimer);
    this.batteryTimer = null;
  }
}

export class AppState {
  sessions = $state<BoardSession[]>([]);
  selectedId = $state<string | null>(null);
  toasts = $state<Toast[]>([]);
  /** A firmware update in progress (or just finished), shown as a dialog. */
  update = $state<FirmwareUpdate | null>(null);

  readonly manager: BoardManager;
  /** Real boards over WebHID; absent when the browser has no WebHID, and in the desktop app. */
  readonly hid: WebHidBoardManager | undefined;
  /** The desktop app, when Doghouse runs in it: boards come in by themselves. */
  readonly desktop: Desktop | undefined;
  /** A newer Doghouse for the desktop app, once found. */
  appUpdate = $state<AppUpdate | null>(null);
  /** Installing it: the download's progress (0-1), or null when unknown. */
  appUpdating = $state<number | null | false>(false);
  /** Simulated boards; present only in demo mode. */
  readonly mock: MockBoardManager | undefined;
  private nextToast = 1;
  /** Boards with no keyboard definition here, already reported. */
  private readonly unknown = new Set<Board>();

  constructor(
    manager: BoardManager,
    sources: { hid?: WebHidBoardManager; mock?: MockBoardManager; desktop?: Desktop } = {},
  ) {
    this.manager = manager;
    this.hid = sources.hid;
    this.desktop = sources.desktop;
    this.mock = sources.mock;
    manager.subscribe((boards) => this.sync(boards));
    this.sync(manager.list());
  }

  /** Ask the browser for a board (from a click). */
  async connect(): Promise<void> {
    try {
      await this.hid?.request();
    } catch (err) {
      // Closing the picker without choosing isn't an error worth a toast.
      if (!(err instanceof Error && err.name === "NotFoundError")) this.notify(errorText(err), "error");
    }
  }

  /** Check a .uf2 file for this board and open the update dialog. */
  async prepareUpdate(session: BoardSession, file: File): Promise<void> {
    if (!session.definition) return;
    try {
      const bytes = new Uint8Array(await file.arrayBuffer());
      this.update?.dispose();
      this.update = new FirmwareUpdate(session.board, session.displayName, session.definition, { name: file.name, bytes }, {
        notify: (text, kind) => this.notify(text, kind),
        mock: this.mock,
        desktop: this.desktop,
      });
    } catch (err) {
      this.notify(`${file.name}: ${errorText(err)}`, "error");
    }
  }

  /** Look for a newer Doghouse (desktop app only). Quiet unless one is found. */
  async checkForAppUpdate(): Promise<void> {
    try {
      this.appUpdate = (await this.desktop?.checkForUpdate()) ?? null;
    } catch (err) {
      console.warn("checking for a Doghouse update:", err);
    }
  }

  /** Download and install the newer Doghouse, then restart into it. */
  async installAppUpdate(): Promise<void> {
    if (!this.appUpdate || this.appUpdating !== false) return;
    this.appUpdating = null;
    try {
      await this.appUpdate.install((fraction) => (this.appUpdating = fraction));
    } catch (err) {
      this.appUpdating = false;
      this.notify(`Couldn't update Doghouse: ${errorText(err)}`, "error");
    }
  }

  closeUpdate(): void {
    this.update?.dispose();
    this.update = null;
  }

  get selected(): BoardSession | undefined {
    return this.sessions.find((s) => s.id === this.selectedId);
  }

  private sync(all: Board[]): void {
    const boards = all.filter((b) => {
      if (definitions[b.info.model] || (b.info.model === "" && b.info.protocolVersion !== PROTOCOL_VERSION)) {
        return true;
      }
      if (!this.unknown.has(b)) {
        this.unknown.add(b);
        this.notify(`Doghouse doesn't know the ${b.info.model} yet.`, "error");
      }
      return false;
    });
    const ids = new Set(boards.map((b) => b.info.id));
    const present = new Set(boards);
    for (const s of this.sessions) {
      if (present.has(s.board)) continue;
      s.dispose();
      if (ids.has(s.id)) {
        // The same board over another link (the cable came out, or went in).
        const now = boards.find((b) => b.info.id === s.id)!;
        this.notify(`${s.displayName} is here over ${CONNECTION_NAMES[now.info.connection]} now.`, "info");
      } else if (this.update?.boardId !== s.id) {
        // Leaving for its bootloader is part of an update, which says so itself.
        this.notify(`${s.displayName} left the kennel.`, "info");
      }
    }
    const kept = this.sessions.filter((s) => present.has(s.board));
    const known = new Set(kept.map((s) => s.board));
    const added = boards.filter((b) => !known.has(b)).map((b) => new BoardSession(b, this));
    this.sessions = [...kept, ...added];

    for (const session of added) {
      session
        .load()
        .then(async () => {
          // A board back from a firmware update: check it and put its settings back.
          if (this.update && (await this.update.boardBack(session.board))) {
            if (this.update.restored) await session.load();
            this.selectedId = session.id;
          }
        })
        .catch((err) => this.notify(errorText(err), "error"));
    }
    if (!this.selected) this.selectedId = this.sessions[0]?.id ?? null;
  }

  select(id: string): void {
    this.selectedId = id;
  }

  notify(text: string, kind: Toast["kind"] = "info"): void {
    const toast = { id: this.nextToast++, text, kind };
    this.toasts = [...this.toasts, toast];
    setTimeout(() => {
      this.toasts = this.toasts.filter((t) => t.id !== toast.id);
    }, 3500);
  }
}

function createApp(): AppState {
  // The desktop app reaches boards through its own bridge, even where its WebView has WebHID.
  const desktop = inDesktopApp() ? desktopApp() : undefined;
  const browserHid = desktop ? undefined : webHid();
  const demo = (!browserHid && !desktop) || new URLSearchParams(globalThis.location?.search ?? "").has("demo");

  let notify: (text: string) => void = () => {};
  const hid = browserHid
    ? new WebHidBoardManager(browserHid, { onError: (err) => notify(`A board didn't answer: ${errorText(err)}`) })
    : undefined;
  let mock: MockBoardManager | undefined;
  if (demo) {
    mock = new MockBoardManager({ definitions, tickMs: 4000, latencyMs: 40 });
    mock.plug("Dachshund60F", "usb", { name: "Biscuit", battery: { percent: 82, charging: true } });
    mock.plug("Dachshund60F", "dongle", { name: "Noodle", battery: { percent: 46, charging: false } });
  }

  const native = desktop
    ? new TauriHidBoardManager(desktop.hid, { onError: (err) => notify(`A board didn't answer: ${errorText(err)}`) })
    : undefined;

  const managers: BoardManager[] = [hid, native, mock].filter((m): m is NonNullable<typeof m> => m !== undefined);
  const app = new AppState(new CombinedBoardManager(managers), {
    ...(hid && { hid }),
    ...(mock && { mock }),
    ...(desktop && { desktop }),
  });
  notify = (text) => app.notify(text, "error");
  hid?.start().catch((err) => app.notify(errorText(err), "error"));
  native?.start().catch((err) => app.notify(`Doghouse can't reach USB devices: ${errorText(err)}`, "error"));
  void app.checkForAppUpdate();
  return app;
}

export const app = createApp();
