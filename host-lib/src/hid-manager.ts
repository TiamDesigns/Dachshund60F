/**
 * What the WebHID and node-hid board managers share: which HID devices are
 * config interfaces of our boards, and the list of open boards with its
 * listeners.
 */
import { HidBoard, type HidBoardOptions } from "./hid-board.js";
import { HID_USAGE, HID_USAGE_PAGE } from "./protocol.gen.js";
import type { Transport } from "./transport.js";
import type { Board, BoardManager } from "./types.js";

/** USB IDs of the keyboard (pid.codes test PID; see CLAUDE.md). */
export const KEYBOARD_USB_IDS = [{ vendorId: 0x1209, productId: 0x0001 }] as const;

/** The config interface's number, for hosts that don't report usage pages (Linux hidraw). */
export const CONFIG_INTERFACE_NUMBER = 2;

export function isKeyboardUsbId(vendorId: number, productId: number): boolean {
  return KEYBOARD_USB_IDS.some((id) => id.vendorId === vendorId && id.productId === productId);
}

export function isConfigUsage(usagePage: number | undefined, usage: number | undefined): boolean {
  return usagePage === HID_USAGE_PAGE && usage === HID_USAGE;
}

export interface HidManagerOptions extends HidBoardOptions {
  /** Called when a device that looked like a board couldn't be opened. */
  onError?: (error: unknown) => void;
}

export abstract class HidManagerBase implements BoardManager {
  private readonly boards = new Map<string, HidBoard>();
  private readonly pending = new Set<string>();
  private readonly listeners = new Set<(boards: Board[]) => void>();

  constructor(protected readonly options: HidManagerOptions = {}) {}

  list(): Board[] {
    return [...this.boards.values()];
  }

  subscribe(listener: (boards: Board[]) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  /** Close every board. */
  async close(): Promise<void> {
    await Promise.all([...this.boards.values()].map((board) => board.close().catch(() => undefined)));
  }

  protected has(key: string): boolean {
    return this.boards.has(key) || this.pending.has(key);
  }

  /** Open a board on a transport under key; it leaves the list when the transport closes. */
  protected async add(key: string, open: () => Promise<Transport>, serialNumber?: string): Promise<HidBoard | null> {
    if (this.has(key)) return this.boards.get(key) ?? null;
    this.pending.add(key);
    try {
      const transport = await open();
      const options: HidBoardOptions = { ...this.options };
      if (serialNumber !== undefined) options.serialNumber = serialNumber;
      const board = await HidBoard.open(transport, options);
      if (!board.connected) return null;
      this.boards.set(key, board);
      board.subscribe((event) => {
        if (event.type === "disconnected" && this.boards.get(key) === board) {
          this.boards.delete(key);
          this.notify();
        }
      });
      this.notify();
      return board;
    } catch (err) {
      this.options.onError?.(err);
      return null;
    } finally {
      this.pending.delete(key);
    }
  }

  protected keys(): string[] {
    return [...this.boards.keys()];
  }

  protected async remove(key: string): Promise<void> {
    await this.boards.get(key)?.close();
  }

  private notify(): void {
    const boards = this.list();
    for (const listener of this.listeners) listener(boards);
  }
}
