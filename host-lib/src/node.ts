/**
 * Desktop (Node) entry: everything in the main entry plus the node-hid
 * board manager. Kept apart so browser bundles never try to resolve node-hid.
 */
export * from "./index.js";
export * from "./node-hid.js";
