/**
 * Config protocol frames. Commands and payloads come from protocol.gen.ts,
 * generated from protocol/protocol.yaml; this file adds the 4-byte frame
 * header around them.
 *
 * Request:  command, sequence, payload length, protocol version, payload
 * Response: command, sequence, payload length, status, payload
 * Both are padded with zeros to REPORT_SIZE.
 */
import { HEADER_SIZE, MAX_PAYLOAD, PROTOCOL_VERSION, REPORT_SIZE, STATUS_TEXT, Status } from "./protocol.gen.js";

export * from "./protocol.gen.js";

export interface ResponseFrame {
  command: number;
  sequence: number;
  status: number;
  payload: Uint8Array;
}

/** A frame that can't be built or read. */
export class FrameError extends Error {}

/** One output report: the request frame, padded to REPORT_SIZE. */
export function encodeRequest(
  command: number,
  sequence: number,
  payload: Uint8Array,
  version: number = PROTOCOL_VERSION,
): Uint8Array {
  if (payload.length > MAX_PAYLOAD) throw new FrameError(`payload of ${payload.length} bytes exceeds ${MAX_PAYLOAD}`);
  const frame = new Uint8Array(REPORT_SIZE);
  frame[0] = command & 0xff;
  frame[1] = sequence & 0xff;
  frame[2] = payload.length;
  frame[3] = version & 0xff;
  frame.set(payload, HEADER_SIZE);
  return frame;
}

/** Read an input report as a response frame. */
export function decodeResponse(report: Uint8Array): ResponseFrame {
  if (report.length < HEADER_SIZE) throw new FrameError(`report of ${report.length} bytes has no header`);
  const length = report[2]!;
  if (length > MAX_PAYLOAD || HEADER_SIZE + length > report.length) {
    throw new FrameError(`payload length ${length} doesn't fit a ${report.length}-byte report`);
  }
  return {
    command: report[0]!,
    sequence: report[1]!,
    status: report[3]!,
    payload: report.slice(HEADER_SIZE, HEADER_SIZE + length),
  };
}

/** A human-readable status, e.g. for an error message. */
export function statusText(status: number): string {
  return STATUS_TEXT[status] ?? `Unknown status ${status}`;
}

export function isOk(frame: ResponseFrame): boolean {
  return frame.status === Status.ok;
}
