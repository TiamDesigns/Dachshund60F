/**
 * A transport carries config protocol reports to and from one board: a HID
 * device opened through WebHID or node-hid, or anything else that moves
 * 64-byte reports (the host-built firmware in the tests).
 */
import type { ConnectionKind } from "./types.js";

export interface Transport {
  readonly connection: ConnectionKind;
  /** Send one output report (a request frame, REPORT_SIZE bytes). */
  write(report: Uint8Array): Promise<void>;
  /** Called with every input report. Returns a function that stops it. */
  onReport(listener: (report: Uint8Array) => void): () => void;
  /** Called once when the device goes away or the transport is closed. */
  onClose(listener: () => void): () => void;
  close(): Promise<void>;
}

/** Listener bookkeeping shared by the transports. */
export class TransportEvents {
  private readonly reportListeners = new Set<(report: Uint8Array) => void>();
  private readonly closeListeners = new Set<() => void>();
  private closed = false;

  get isClosed(): boolean {
    return this.closed;
  }

  onReport(listener: (report: Uint8Array) => void): () => void {
    this.reportListeners.add(listener);
    return () => this.reportListeners.delete(listener);
  }

  onClose(listener: () => void): () => void {
    if (this.closed) {
      queueMicrotask(listener);
      return () => {};
    }
    this.closeListeners.add(listener);
    return () => this.closeListeners.delete(listener);
  }

  report(report: Uint8Array): void {
    for (const listener of this.reportListeners) listener(report);
  }

  close(): void {
    if (this.closed) return;
    this.closed = true;
    for (const listener of this.closeListeners) listener();
    this.closeListeners.clear();
    this.reportListeners.clear();
  }
}
