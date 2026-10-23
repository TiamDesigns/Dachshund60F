/**
 * Host library ("driver") for the keyboard config protocol.
 *
 * The typed board API (types.ts) is what the GUI uses. Today only the mock
 * transport implements it; the hidapi and WebHID transports arrive in phase 7,
 * on top of the protocol bindings (protocol.ts, generated from
 * protocol/protocol.yaml). The bindings are exported as `protocol`, so their
 * names don't mix with the typed API.
 */

export const HOST_LIB_VERSION = "0.1.0";

export * from "./types.js";
export * from "./keyboard-definition.js";
export * from "./keycodes.js";
export { defaultKeymap } from "./default-keymaps.js";
export * from "./mock.js";
export * as protocol from "./protocol.js";
