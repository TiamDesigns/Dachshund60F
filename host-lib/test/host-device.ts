// Runs firmware/core's kb_host_device (the protocol handler and store built
// for the host) and wraps its stdin/stdout in a Transport. KB_HOST_DEVICE
// names the binary; without it, the core's default build directory is tried.
import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { existsSync } from "node:fs";
import { createInterface } from "node:readline";
import { fileURLToPath } from "node:url";
import path from "node:path";

import { TransportEvents, type Transport } from "../src/index.js";

const repoDir = fileURLToPath(new URL("../../../", import.meta.url));

/** The binary to run, or undefined (with the reason) when there is none. */
export function hostDevicePath(): { path?: string; skip?: string } {
  const env = process.env["KB_HOST_DEVICE"];
  if (env) {
    if (!existsSync(env)) throw new Error(`KB_HOST_DEVICE=${env} doesn't exist`);
    return { path: env };
  }
  const exe = process.platform === "win32" ? "kb_host_device.exe" : "kb_host_device";
  for (const dir of ["firmware/core/build", "firmware/core/build/Debug"]) {
    const candidate = path.join(repoDir, dir, exe);
    if (existsSync(candidate)) return { path: candidate };
  }
  return { skip: "kb_host_device not built (build firmware/core, or set KB_HOST_DEVICE)" };
}

export class HostDeviceTransport implements Transport {
  readonly connection = "usb" as const;
  private readonly events = new TransportEvents();
  private readonly child: ChildProcessWithoutNullStreams;

  constructor(binary: string) {
    this.child = spawn(binary, [], { stdio: "pipe" });
    const lines = createInterface({ input: this.child.stdout });
    lines.on("line", (line) => {
      if (line.length === 0) return; // no response to a headerless request
      this.events.report(Uint8Array.from(line.match(/../g)!.map((h) => parseInt(h, 16))));
    });
    this.child.on("exit", () => this.events.close());
    this.child.stdin.on("error", () => this.events.close());
  }

  async write(report: Uint8Array): Promise<void> {
    if (this.events.isClosed) throw new Error("transport closed");
    this.child.stdin.write(Buffer.from(report).toString("hex") + "\n");
  }

  onReport(listener: (report: Uint8Array) => void): () => void {
    return this.events.onReport(listener);
  }

  onClose(listener: () => void): () => void {
    return this.events.onClose(listener);
  }

  async close(): Promise<void> {
    if (this.events.isClosed) return;
    this.events.close();
    this.child.stdin.end();
    await new Promise<void>((resolve) => {
      if (this.child.exitCode !== null) resolve();
      else this.child.once("exit", () => resolve());
    });
  }
}
