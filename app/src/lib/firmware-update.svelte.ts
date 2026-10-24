/**
 * The firmware update flow, from a .uf2 file to a board running it with its
 * config intact:
 *
 *   ready      the file is checked and its version compared with the board's
 *   backup     every setting is read off the board (and kept in this browser)
 *   bootloader the board restarts into its UF2 bootloader and leaves
 *   drive      the user picks the board's drive (the desktop app finds it); the file is written to it
 *   waiting    the board restarts and comes back over WebHID
 *   restoring  its version is checked, and anything the update lost is put back
 *   done / failed
 *
 * It lives on AppState, not on a board session, because the session goes
 * away while the board is in its bootloader.
 */
import {
  backupBoard,
  compareVersions,
  parseUf2,
  protocol,
  restoreBoard,
  uf2BoardId,
  type Board,
  type BoardBackup,
  type KeyboardDefinition,
  type MockBoardManager,
  type Uf2Image,
} from "@dachshund60f/host-lib";

import type { Desktop } from "./desktop.ts";

export type UpdateStage = "ready" | "backup" | "bootloader" | "drive" | "writing" | "waiting" | "restoring" | "done" | "failed";

/** How long the desktop app looks for the board's drive to mount. */
const DRIVE_WAIT_MS = 20_000;
const DRIVE_POLL_MS = 500;

/** How long to wait for the board before suggesting a replug. */
const SLOW_RETURN_MS = 30_000;

const BACKUP_KEY = (id: string) => `doghouse.backup.${id}`;

/** The File System Access API's directory picker (Chrome and Edge). */
type DirectoryPicker = (options?: { mode?: "read" | "readwrite"; id?: string }) => Promise<FileSystemDirectoryHandle>;

function directoryPicker(): DirectoryPicker | undefined {
  return (globalThis as { showDirectoryPicker?: DirectoryPicker }).showDirectoryPicker;
}

function errorText(err: unknown): string {
  return err instanceof Error ? err.message : String(err);
}

export interface UpdateHost {
  notify(text: string, kind?: "info" | "success" | "error"): void;
  /** Present in demo mode, for simulated boards. */
  mock: MockBoardManager | undefined;
  /** The desktop app, which writes to the drive itself. */
  desktop?: Desktop | undefined;
}

export class FirmwareUpdate {
  stage = $state<UpdateStage>("ready");
  /** Why it failed, or a hint while waiting. */
  message = $state<string | null>(null);
  /** How many settings were put back after the update. */
  restored = $state<number | null>(null);
  slow = $state(false);
  backup = $state<BoardBackup | null>(null);

  readonly boardId: string;
  readonly boardName: string;
  readonly fromVersion: string;
  readonly fileName: string;
  readonly image: Uf2Image;
  /** Newer, same or older than the board's firmware: > 0, 0, < 0. */
  readonly direction: number;
  /** Warnings to show before starting (protocol change, downgrade). */
  readonly warnings: string[] = [];
  /** A simulated board, "flashed" by the mock manager instead of through a drive. */
  readonly demo: boolean;

  private readonly board: Board;
  private readonly definition: KeyboardDefinition;
  private readonly bytes: Uint8Array;
  private readonly host: UpdateHost;
  private slowTimer: ReturnType<typeof setTimeout> | undefined;

  /** Check the file against the board; throws with a reason if it can't go on it. */
  constructor(
    board: Board,
    boardName: string,
    definition: KeyboardDefinition,
    file: { name: string; bytes: Uint8Array },
    host: UpdateHost,
  ) {
    this.board = board;
    this.boardId = board.info.id;
    this.boardName = boardName;
    this.fromVersion = board.info.firmwareVersion;
    this.definition = definition;
    this.fileName = file.name;
    this.bytes = file.bytes;
    this.host = host;
    this.demo = !!host.mock?.list().some((b) => b === board);

    this.image = parseUf2(file.bytes);
    const info = this.image.info;
    if (!info) {
      throw new Error("This file has no Dachshund firmware info in it, so it isn't keyboard firmware from this project.");
    }
    if (info.model !== board.info.model) {
      throw new Error(`This is firmware for a ${info.model || "different board"}, not a ${board.info.model}.`);
    }
    this.direction = compareVersions(info.version, this.fromVersion);
    if (this.direction < 0) {
      this.warnings.push(`It's older than the firmware on the board (${this.fromVersion}). Older firmware may not read the newer settings, so they could reset.`);
    } else if (this.direction === 0) {
      this.warnings.push("The board already runs this version; updating reinstalls it.");
    }
    if (info.protocolVersion !== protocol.PROTOCOL_VERSION) {
      this.warnings.push(
        `It speaks config protocol v${info.protocolVersion} and this Doghouse speaks v${protocol.PROTOCOL_VERSION}, ` +
          "so Doghouse won't be able to edit the board or put its settings back afterwards. Update Doghouse too.",
      );
    }
    if (!definition.bootloader) {
      this.warnings.push("Doghouse doesn't know this board's bootloader drive, so it can't check you pick the right one.");
    }
  }

  get toVersion(): string {
    return this.image.info!.version;
  }


  get canPickDrive(): boolean {
    return !!this.host.desktop || !!directoryPicker();
  }

  /** In the desktop app Doghouse finds the drive itself; no picker. */
  get findsDrive(): boolean {
    return !!this.host.desktop;
  }

  get volume(): string {
    return this.definition.bootloader?.volume ?? "the board's";
  }

  /** Back up, then send the board to its bootloader. */
  async start(): Promise<void> {
    if (this.stage !== "ready") return;
    try {
      this.stage = "backup";
      this.backup = await backupBoard(this.board);
      try {
        localStorage.setItem(BACKUP_KEY(this.boardId), JSON.stringify(this.backup));
      } catch {
        /* storage full or blocked: the backup is still in memory and downloadable */
      }
      if (!this.board.reboot) throw new Error("This board can't be restarted from Doghouse. Hold Esc for 3 seconds instead.");
      this.stage = "bootloader";
      await this.board.reboot(true);
      this.stage = "drive";
      // The desktop app finds the drive itself, so there's nothing to click.
      if (this.host.desktop && !this.demo) void this.writeToDrive();
    } catch (err) {
      this.fail(`Couldn't get the board ready: ${errorText(err)}`);
    }
  }

  /** Write the file onto the UF2 drive (from a click: the picker needs one). */
  async writeToDrive(): Promise<void> {
    if (this.stage !== "drive") return;
    if (this.demo) {
      this.stage = "writing";
      this.waitForBoard();
      await this.host.mock!.flash(this.boardId, this.toVersion).catch((err) => this.fail(errorText(err)));
      return;
    }
    if (this.host.desktop) {
      await this.writeFromDesktop(this.host.desktop);
      return;
    }
    const pick = directoryPicker();
    if (!pick) return;
    let dir: FileSystemDirectoryHandle;
    try {
      dir = await pick({ mode: "readwrite", id: "uf2-drive" });
    } catch (err) {
      if (err instanceof Error && err.name === "AbortError") return; // closed the picker
      this.message = errorText(err);
      return;
    }

    const expected = this.definition.bootloader?.uf2BoardId;
    try {
      const info = await (await (await dir.getFileHandle("INFO_UF2.TXT")).getFile()).text();
      const id = uf2BoardId(info);
      if (expected && id !== expected) {
        this.message = `That's the drive of another board (${id ?? "unknown"}). Pick ${this.volume}.`;
        return;
      }
    } catch {
      this.message = `That folder isn't a UF2 drive. Pick the drive called ${this.volume}.`;
      return;
    }

    this.message = null;
    this.stage = "writing";
    try {
      const handle = await dir.getFileHandle("firmware.uf2", { create: true });
      const writable = await handle.createWritable();
      await writable.write(this.bytes as Uint8Array<ArrayBuffer>);
      await writable.close();
    } catch (err) {
      // The bootloader restarts the moment the last block lands, which can
      // make the browser's close fail. The board coming back is the real test.
      console.warn("writing firmware.uf2:", err);
    }
    this.waitForBoard();
  }

  /** The desktop app: wait for the board's drive to mount, then write onto it. */
  private async writeFromDesktop(desktop: Desktop): Promise<void> {
    const expected = this.definition.bootloader?.uf2BoardId;
    this.message = null;
    this.stage = "writing";
    const deadline = Date.now() + DRIVE_WAIT_MS;
    let others: string[] = [];
    for (;;) {
      let drives: { path: string; info: string }[] = [];
      try {
        drives = await desktop.uf2Drives();
      } catch (err) {
        console.warn("listing UF2 drives:", err);
      }
      const ours = drives.filter((d) => !expected || uf2BoardId(d.info) === expected);
      others = drives.filter((d) => !ours.includes(d)).map((d) => uf2BoardId(d.info) ?? "unknown");
      if (ours.length === 1) {
        try {
          await desktop.uf2Write(ours[0]!.path, this.bytes);
        } catch (err) {
          // As in the browser: the bootloader may restart before the write returns.
          console.warn("writing firmware.uf2:", err);
        }
        this.waitForBoard();
        return;
      }
      if (ours.length > 1) {
        this.stage = "drive";
        this.message = `There are ${ours.length} ${this.volume} drives. Unplug the other boards, then try again.`;
        return;
      }
      if (Date.now() >= deadline || this.stage !== "writing") break;
      await new Promise((resolve) => setTimeout(resolve, DRIVE_POLL_MS));
    }
    if (this.stage !== "writing") return;
    this.stage = "drive";
    this.message = others.length
      ? `Only another board's drive showed up (${others.join(", ")}). Check that ${this.volume} is mounted, then try again.`
      : `The ${this.volume} drive didn't show up. Check that it's mounted (double-tap the board's reset if not), then try again.`;
  }

  /** For browsers without the drive picker: the user copies the file over. */
  copiedByHand(): void {
    if (this.stage === "drive") this.waitForBoard();
  }

  /** A URL to download the .uf2 (for copying by hand). Revoke it when done. */
  fileUrl(): string {
    return URL.createObjectURL(new Blob([this.bytes as Uint8Array<ArrayBuffer>], { type: "application/octet-stream" }));
  }

  backupUrl(): string | null {
    if (!this.backup) return null;
    return URL.createObjectURL(new Blob([JSON.stringify(this.backup, null, 2)], { type: "application/json" }));
  }

  private waitForBoard(): void {
    this.stage = "waiting";
    clearTimeout(this.slowTimer);
    this.slowTimer = setTimeout(() => {
      if (this.stage === "waiting") this.slow = true;
    }, SLOW_RETURN_MS);
  }

  /** AppState calls this when a board with our id has connected and loaded. */
  async boardBack(board: Board): Promise<boolean> {
    if (board.info.id !== this.boardId || (this.stage !== "waiting" && this.stage !== "writing")) return false;
    clearTimeout(this.slowTimer);
    this.slow = false;
    if (board.info.firmwareVersion !== this.toVersion) {
      this.fail(`${this.boardName} came back with firmware ${board.info.firmwareVersion || "it can't report"}, not ${this.toVersion}. The update didn't take; try again.`);
      return true;
    }
    if (board.info.protocolVersion !== protocol.PROTOCOL_VERSION || !this.backup) {
      this.stage = "done";
      this.restored = null;
      return true;
    }
    this.stage = "restoring";
    try {
      this.restored = await restoreBoard(board, this.backup);
      this.stage = "done";
      try {
        localStorage.removeItem(BACKUP_KEY(this.boardId));
      } catch {
        /* nothing to clean up */
      }
    } catch (err) {
      this.fail(`The update worked, but putting the settings back failed: ${errorText(err)}. Download the backup and restore it from the About tab.`);
    }
    return true;
  }

  private fail(message: string): void {
    clearTimeout(this.slowTimer);
    this.stage = "failed";
    this.message = message;
  }

  dispose(): void {
    clearTimeout(this.slowTimer);
  }
}

/** A backup this browser kept from an update that didn't finish. */
export function savedBackup(boardId: string): BoardBackup | null {
  try {
    const text = localStorage.getItem(BACKUP_KEY(boardId));
    return text ? (JSON.parse(text) as BoardBackup) : null;
  } catch {
    return null;
  }
}
