/**
 * What the WebHID and node-hid board managers share: which HID devices are
 * config interfaces of our boards, and the list of open boards with its
 * listeners.
 */
import { HidBoard, type HidBoardOptions } from "./hid-board.js";
import { HID_USAGE, HID_USAGE_PAGE } from "./protocol.gen.js";
import type { Transport } from "./transport.js";
import { BoardError, type Board, type BoardManager } from "./types.js";

/** USB IDs of the keyboard (pid.codes test PID; see CLAUDE.md). */
export const KEYBOARD_USB_IDS = [{ vendorId: 0x1209, productId: 0x0001 }] as const;
/** USB IDs of its 2.4 GHz dongle, which relays the config protocol to the keyboard. */
export const DONGLE_USB_IDS = [{ vendorId: 0x1209, productId: 0x0002 }] as const;
/** Everything a board can be reached through over USB. */
export const BOARD_USB_IDS = [...KEYBOARD_USB_IDS, ...DONGLE_USB_IDS] as const;

/** How often to try a dongle again while its keyboard is out of reach (default). */
export const DONGLE_RETRY_MS = 2000;

/** The config interface's number, for hosts that don't report usage pages (Linux hidraw). */
export const CONFIG_INTERFACE_NUMBER = 2;

export function isKeyboardUsbId(vendorId: number, productId: number): boolean {
  return KEYBOARD_USB_IDS.some((id) => id.vendorId === vendorId && id.productId === productId);
}

export function isDongleUsbId(vendorId: number, productId: number): boolean {
  return DONGLE_USB_IDS.some((id) => id.vendorId === vendorId && id.productId === productId);
}

/** A keyboard or its dongle. */
export function isBoardUsbId(vendorId: number, productId: number): boolean {
  return isKeyboardUsbId(vendorId, productId) || isDongleUsbId(vendorId, productId);
}

export function isConfigUsage(usagePage: number | undefined, usage: number | undefined): boolean {
  return usagePage === HID_USAGE_PAGE && usage === HID_USAGE;
}

export interface HidManagerOptions extends HidBoardOptions {
  /** Called when a device that looked like a board couldn't be opened. */
  onError?: (error: unknown) => void;
  /** How often to try a dongle again while its keyboard is asleep or away. Default DONGLE_RETRY_MS. */
  dongleRetryMs?: number;
}

export abstract class HidManagerBase implements BoardManager {
  private readonly boards = new Map<string, HidBoard>();
  private readonly pending = new Set<string>();
  private readonly listeners = new Set<(boards: Board[]) => void>();
  private readonly retries = new Map<string, ReturnType<typeof setTimeout>>();
  private closing = false;

  constructor(protected readonly options: HidManagerOptions = {}) {}

  /**
   * The open boards, one per board: a board reachable over both USB and
   * Bluetooth (charging from the computer it's paired with) is listed once,
   * over USB, and its Bluetooth link takes over when the cable comes out.
   */
  list(): Board[] {
    const byId = new Map<string, HidBoard>();
    for (const board of this.boards.values()) {
      const seen = byId.get(board.info.id);
      if (!seen || (seen.info.connection === "ble" && board.info.connection !== "ble")) byId.set(board.info.id, board);
    }
    return [...byId.values()];
  }

  subscribe(listener: (boards: Board[]) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  /** Close every board. */
  async close(): Promise<void> {
    this.closing = true;
    for (const timer of this.retries.values()) clearTimeout(timer);
    this.retries.clear();
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
      // Without a serial number (WebHID can't read one), the key at least keeps ids unique.
      const options: HidBoardOptions = { ...this.options, serialNumber: serialNumber || key };
      const board = await HidBoard.open(transport, options);
      if (!board.connected) return null;
      this.boards.set(key, board);
      board.subscribe((event) => {
        if (event.type === "disconnected" && this.boards.get(key) === board) {
          this.boards.delete(key);
          this.notify();
          if (board.unreachable) this.retryLater(key, open, serialNumber);
        }
      });
      this.notify();
      return board;
    } catch (err) {
      // A dongle whose keyboard is asleep or away isn't an error: try again in a while.
      if (err instanceof BoardError && err.code === "unreachable") this.retryLater(key, open, serialNumber);
      else this.options.onError?.(err);
      return null;
    } finally {
      this.pending.delete(key);
    }
  }

  /**
   * Whether the device behind key is still there to try again. Managers
   * that rescan by themselves (node-hid) say no and let the scan do it.
   */
  protected canRetry(_key: string): boolean {
    return true;
  }

  private retryLater(key: string, open: () => Promise<Transport>, serialNumber?: string): void {
    if (this.closing || this.retries.has(key) || !this.canRetry(key)) return;
    const timer = setTimeout(() => {
      this.retries.delete(key);
      if (!this.closing && this.canRetry(key)) void this.add(key, open, serialNumber);
    }, this.options.dongleRetryMs ?? DONGLE_RETRY_MS);
    (timer as { unref?: () => void }).unref?.();
    this.retries.set(key, timer);
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
