/**
 * Host library ("driver") for the keyboard config protocol.
 *
 * The typed board API (types.ts) is what the GUI uses. HidBoard implements it
 * over the config protocol on any Transport; WebHidBoardManager finds boards
 * in the browser and follows hotplug (NodeHidBoardManager does the same on
 * the desktop, from the separate "./node" entry so browser builds never see
 * node-hid); MockBoard simulates boards. The protocol bindings (protocol.ts, generated from
 * protocol/protocol.yaml) are exported as `protocol`, so their names don't
 * mix with the typed API.
 */

export const HOST_LIB_VERSION = "0.1.0";

export * from "./types.js";
export * from "./keyboard-definition.js";
export * from "./keycodes.js";
export { defaultKeymap } from "./default-keymaps.js";
export * from "./mock.js";
export { encodeKeycode, decodeKeycode, KeycodeError } from "./keycode-codec.js";
export * from "./transport.js";
export { ProtocolClient, statusError, type ClientOptions } from "./client.js";
export { HidBoard, capabilityNames, type HidBoardOptions } from "./hid-board.js";
export * from "./hid-manager.js";
export { CombinedBoardManager } from "./combined-manager.js";
export * from "./webhid.js";
export * as protocol from "./protocol.js";
