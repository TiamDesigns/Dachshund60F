/**
 * Request/response over a Transport: one request in flight at a time, each
 * with its own sequence number, so a late answer to a timed-out request is
 * never taken for the answer to the next one. Timed-out requests are sent
 * again (with a new sequence number) unless the command isn't safe to repeat.
 */
import { Command, PROTOCOL_VERSION, Status } from "./protocol.gen.js";
import { decodeResponse, encodeRequest, FrameError, statusText, type ResponseFrame } from "./protocol.js";
import type { Transport } from "./transport.js";
import { BoardError, type BoardErrorCode } from "./types.js";

export interface ClientOptions {
  /** How long to wait for each response. Default 1000 ms. */
  timeoutMs?: number;
  /** Extra attempts after a timeout. Default 2. */
  retries?: number;
}

/** Commands that do something each time they arrive, so a timeout isn't retried. */
const NOT_REPEATABLE = new Set<number>([Command.reboot]);

const STATUS_CODES: Record<number, BoardErrorCode> = {
  [Status.unknownCommand]: "not-supported",
  [Status.badLength]: "protocol-error",
  [Status.outOfRange]: "out-of-range",
  [Status.badValue]: "invalid-value",
  [Status.busy]: "busy",
  [Status.storageFull]: "storage-full",
  [Status.storageError]: "storage-error",
  [Status.versionMismatch]: "version-mismatch",
  [Status.notSupported]: "not-supported",
  [Status.unreachable]: "unreachable",
};

/** The BoardError for a response status other than ok. */
export function statusError(status: number): BoardError {
  return new BoardError(STATUS_CODES[status] ?? "protocol-error", statusText(status));
}

export class ProtocolClient {
  private readonly timeoutMs: number;
  private readonly retries: number;
  private sequence = 0;
  private queue: Promise<unknown> = Promise.resolve();
  private closed = false;
  private waiting: ((frame: ResponseFrame | null) => void) | null = null;
  private readonly stopReports: () => void;
  private readonly stopClose: () => void;

  /** Closed because a dongle answered that its keyboard is out of reach. */
  unreachable = false;

  /** Protocol version sent in each request; set from the handshake. */
  version: number = PROTOCOL_VERSION;

  constructor(
    readonly transport: Transport,
    options: ClientOptions = {},
  ) {
    this.timeoutMs = options.timeoutMs ?? 1000;
    this.retries = options.retries ?? 2;
    this.stopReports = transport.onReport((report) => this.onReport(report));
    this.stopClose = transport.onClose(() => this.onClose());
  }

  get isClosed(): boolean {
    return this.closed;
  }

  /**
   * Send a command and return the response payload. Throws BoardError for an
   * error status, a timeout after every retry, or a lost transport.
   */
  request(command: number, payload: Uint8Array = new Uint8Array(0)): Promise<Uint8Array> {
    const run = this.queue.then(() => this.attempts(command, payload));
    this.queue = run.catch(() => undefined);
    return run;
  }

  async close(): Promise<void> {
    this.onClose();
    await this.transport.close();
  }

  private async attempts(command: number, payload: Uint8Array): Promise<Uint8Array> {
    const tries = NOT_REPEATABLE.has(command) ? 1 : 1 + this.retries;
    for (let i = 0; i < tries; i++) {
      const frame = await this.once(command, payload);
      if (frame === null) continue; // timed out
      if (frame.status === Status.unreachable) {
        // A dongle without its keyboard: the board is gone until it comes back.
        this.unreachable = true;
        void this.close().catch(() => undefined);
      }
      if (frame.status !== Status.ok) throw statusError(frame.status);
      return frame.payload;
    }
    throw new BoardError("timeout", `no response to command 0x${command.toString(16)} after ${tries} tries`);
  }

  /** One request; resolves with its response, or null on a timeout. */
  private async once(command: number, payload: Uint8Array): Promise<ResponseFrame | null> {
    if (this.closed) throw new BoardError("disconnected", "the board is not connected");
    const sequence = (this.sequence = (this.sequence + 1) & 0xff);
    let timer: ReturnType<typeof setTimeout> | undefined;
    const answer = new Promise<ResponseFrame | null>((resolve) => {
      this.waiting = (frame) => {
        if (frame === null || (frame.command === command && frame.sequence === sequence)) {
          this.waiting = null;
          clearTimeout(timer);
          resolve(frame);
        }
      };
      timer = setTimeout(() => {
        this.waiting = null;
        resolve(null);
      }, this.timeoutMs);
    });
    try {
      await this.transport.write(encodeRequest(command, sequence, payload, this.version));
    } catch (err) {
      this.waiting = null;
      clearTimeout(timer);
      if (this.closed) throw new BoardError("disconnected", "the board is not connected");
      throw err;
    }
    const frame = await answer;
    if (this.closed) throw new BoardError("disconnected", "the board is not connected");
    return frame;
  }

  private onReport(report: Uint8Array): void {
    let frame: ResponseFrame;
    try {
      frame = decodeResponse(report);
    } catch (err) {
      if (err instanceof FrameError) return; // not a frame: ignore it
      throw err;
    }
    this.waiting?.(frame);
  }

  private onClose(): void {
    if (this.closed) return;
    this.closed = true;
    this.stopReports();
    this.stopClose();
    this.waiting?.(null);
  }
}
