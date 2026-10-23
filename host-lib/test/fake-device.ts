// A scripted Transport for the client and manager tests: each request goes
// to a handler, which answers with response reports (or none, to simulate a
// lost report).
import { protocol, TransportEvents, type Transport } from "../src/index.js";

export interface Request {
  command: number;
  sequence: number;
  version: number;
  payload: Uint8Array;
}

export type Handler = (request: Request) => Uint8Array[] | Uint8Array | null;

/** A 64-byte response report. */
export function response(command: number, sequence: number, status: number, payload: Uint8Array = new Uint8Array(0)): Uint8Array {
  const r = new Uint8Array(protocol.REPORT_SIZE);
  r.set([command, sequence, payload.length, status]);
  r.set(payload, protocol.HEADER_SIZE);
  return r;
}

/** Answers handshake and get_serial like a board with the given protocol version. */
export function boardHandler(serial: string, version: number = protocol.PROTOCOL_VERSION): Handler {
  return (req) => {
    if (req.command === protocol.Command.handshake) {
      const payload = protocol.encodeHandshakeResponse({
        protocolVersion: version,
        firmwareMajor: 1,
        firmwareMinor: 2,
        firmwarePatch: 3,
        build: "fake",
        capabilities: protocol.Capability.keymap | protocol.Capability.identify,
        rows: 6,
        cols: 15,
        layers: 4,
        encoders: 0,
        macroBufferSize: 0,
        model: "Dachshund60F",
      });
      return response(req.command, req.sequence, 0, payload);
    }
    if (req.version !== protocol.PROTOCOL_VERSION || version !== protocol.PROTOCOL_VERSION) {
      return response(req.command, req.sequence, protocol.Status.versionMismatch);
    }
    if (req.command === protocol.Command.getSerial) {
      return response(req.command, req.sequence, 0, protocol.encodeGetSerialResponse({ serial }));
    }
    return response(req.command, req.sequence, protocol.Status.unknownCommand);
  };
}

export class FakeTransport implements Transport {
  readonly connection = "usb" as const;
  readonly events = new TransportEvents();
  readonly requests: Request[] = [];

  constructor(public handler: Handler) {}

  async write(report: Uint8Array): Promise<void> {
    if (this.events.isClosed) throw new Error("transport closed");
    const req: Request = {
      command: report[0]!,
      sequence: report[1]!,
      version: report[3]!,
      payload: report.slice(protocol.HEADER_SIZE, protocol.HEADER_SIZE + report[2]!),
    };
    this.requests.push(req);
    const out = this.handler(req);
    const reports = out === null ? [] : Array.isArray(out) ? out : [out];
    // Answer asynchronously, like a device.
    setTimeout(() => reports.forEach((r) => this.events.report(r)), 0);
  }

  onReport(listener: (report: Uint8Array) => void): () => void {
    return this.events.onReport(listener);
  }

  onClose(listener: () => void): () => void {
    return this.events.onClose(listener);
  }

  async close(): Promise<void> {
    this.events.close();
  }
}
