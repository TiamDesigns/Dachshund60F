/**
 * Application state: connected boards and the editing session for each.
 *
 * Boards come from a host-lib BoardManager. Until the hidapi/WebHID
 * transports exist (phase 7), that is the MockBoardManager with simulated
 * Dachshund60Fs; nothing else in the app changes when the real one arrives.
 */

import {
  BoardError,
  MockBoardManager,
  type BatteryStatus,
  type Board,
  type BoardManager,
  type BoardSettings,
  type ConnectionKind,
  type EncoderAction,
  type KeyboardDefinition,
  type KeyPosition,
  type Keymap,
  type KeycodeId,
} from "@dachshund60f/host-lib";
import dachshund60f from "@boards/Dachshund60F/keyboard.json";

import { EditHistory, type Edit } from "./history.ts";

export const definitions: Record<string, KeyboardDefinition> = {
  Dachshund60F: dachshund60f as KeyboardDefinition,
};

/** Dog breed shown for each board model. */
export const BREEDS: Record<string, string> = {
  Dachshund60F: "Dachshund",
};

export type Toast = { id: number; text: string; kind: "info" | "success" | "error" };

function errorText(err: unknown): string {
  if (err instanceof BoardError) return err.message;
  if (err instanceof Error) return err.message;
  return String(err);
}

export class BoardSession {
  readonly board: Board;
  readonly definition: KeyboardDefinition;

  battery = $state<BatteryStatus | null>(null);
  settings = $state<BoardSettings | null>(null);
  keymap = $state<Keymap | null>(null);
  unsaved = $state(false);
  busy = $state(false);
  canUndo = $state(false);
  canRedo = $state(false);

  private readonly history = new EditHistory();
  private readonly unsubscribe: () => void;
  private readonly app: AppState;

  constructor(board: Board, app: AppState) {
    this.board = board;
    this.app = app;
    const definition = definitions[board.info.model];
    if (!definition) throw new Error(`no keyboard definition for ${board.info.model}`);
    this.definition = definition;

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

  get connection(): ConnectionKind {
    return this.board.info.connection;
  }

  /** The owner's name for the board, falling back to the model. */
  get displayName(): string {
    return this.settings?.name.trim() || this.board.info.model;
  }

  async load(): Promise<void> {
    const [battery, settings, keymap, unsaved] = await Promise.all([
      this.board.getBattery(),
      this.board.getSettings(),
      this.board.getKeymap(),
      this.board.hasUnsavedChanges(),
    ]);
    this.battery = battery;
    this.settings = settings;
    this.keymap = keymap;
    this.unsaved = unsaved;
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

  dispose(): void {
    this.unsubscribe();
  }
}

export class AppState {
  sessions = $state<BoardSession[]>([]);
  selectedId = $state<string | null>(null);
  toasts = $state<Toast[]>([]);

  readonly manager: BoardManager;
  /** Present only while boards are simulated. */
  readonly mock: MockBoardManager | undefined;
  private nextToast = 1;

  constructor(manager: BoardManager, mock?: MockBoardManager) {
    this.manager = manager;
    this.mock = mock;
    manager.subscribe((boards) => this.sync(boards));
    this.sync(manager.list());
  }

  get selected(): BoardSession | undefined {
    return this.sessions.find((s) => s.id === this.selectedId);
  }

  private sync(boards: Board[]): void {
    const ids = new Set(boards.map((b) => b.info.id));
    for (const s of this.sessions) {
      if (!ids.has(s.id)) {
        s.dispose();
        this.notify(`${s.displayName} left the kennel.`, "info");
      }
    }
    const kept = this.sessions.filter((s) => ids.has(s.id));
    const known = new Set(kept.map((s) => s.id));
    const added = boards.filter((b) => !known.has(b.info.id)).map((b) => new BoardSession(b, this));
    this.sessions = [...kept, ...added];

    for (const session of added) {
      session.load().catch((err) => this.notify(errorText(err), "error"));
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
  const mock = new MockBoardManager({ definitions, tickMs: 4000, latencyMs: 40 });
  mock.plug("Dachshund60F", "usb", { name: "Biscuit", battery: { percent: 82, charging: true } });
  mock.plug("Dachshund60F", "dongle", { name: "Noodle", battery: { percent: 46, charging: false } });
  return new AppState(mock, mock);
}

export const app = createApp();
