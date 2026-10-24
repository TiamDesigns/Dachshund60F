/**
 * A board's whole configuration as one JSON-friendly object: taken before a
 * firmware update, put back afterwards if the new firmware lost any of it,
 * and saveable as a file.
 *
 * Restoring writes only what differs, then saves, so restoring onto a board
 * that kept its settings changes nothing.
 */
import {
  BoardError,
  type AdvancedKeys,
  type AdvancedTable,
  type Board,
  type BoardSettings,
  type IndicatorColours,
  type Keymap,
  type MacroStep,
} from "./types.js";

export const BACKUP_FORMAT = 1;

export interface BoardBackup {
  format: typeof BACKUP_FORMAT;
  /** ISO time it was taken. */
  takenAt: string;
  board: { id: string; model: string; firmwareVersion: string; layerCount: number };
  keymap: Keymap;
  settings: BoardSettings | null;
  advanced: AdvancedKeys | null;
  macros: MacroStep[][] | null;
  indicators: IndicatorColours | null;
}

const has = (board: Board, capability: string) =>
  board.info.capabilities === undefined || board.info.capabilities.includes(capability);

/** Read everything the board's firmware lets us read. */
export async function backupBoard(board: Board): Promise<BoardBackup> {
  const { id, model, firmwareVersion, layerCount } = board.info;
  return {
    format: BACKUP_FORMAT,
    takenAt: new Date().toISOString(),
    board: { id, model, firmwareVersion, layerCount },
    keymap: await board.getKeymap(),
    settings: has(board, "settings") ? await board.getSettings() : null,
    advanced: has(board, "advancedKeys") && board.getAdvancedKeys ? await board.getAdvancedKeys() : null,
    macros: has(board, "macros") && board.getMacros ? await board.getMacros() : null,
    indicators: has(board, "indicators") && board.getIndicators ? await board.getIndicators() : null,
  };
}

const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);

const TABLES: AdvancedTable[] = ["combos", "tapDances", "keyOverrides", "leader", "tapHold"];

/**
 * Put a backup back: writes what differs from the board's live config,
 * skips what the board can't take (fewer layers, a missing feature), and
 * saves if anything changed. Returns the number of writes.
 */
export async function restoreBoard(board: Board, backup: BoardBackup): Promise<number> {
  if (backup.format !== BACKUP_FORMAT) {
    throw new BoardError("invalid-value", `backup format ${String(backup.format)} isn't one this app reads`);
  }
  if (backup.board.model !== board.info.model) {
    throw new BoardError("invalid-value", `that backup is from a ${backup.board.model}, not a ${board.info.model}`);
  }
  const now = await backupBoard(board);
  let writes = 0;

  const layers = Math.min(backup.keymap.keys.length, now.keymap.keys.length);
  for (let layer = 0; layer < layers; layer++) {
    const rows = backup.keymap.keys[layer]!;
    for (let row = 0; row < rows.length; row++) {
      for (let col = 0; col < rows[row]!.length; col++) {
        const kc = rows[row]![col]!;
        if (now.keymap.keys[layer]?.[row]?.[col] !== undefined && now.keymap.keys[layer]![row]![col] !== kc) {
          await board.setKey({ layer, row, col }, kc);
          writes++;
        }
      }
    }
    const encoders = backup.keymap.encoders[layer] ?? [];
    for (let e = 0; e < encoders.length; e++) {
      const was = now.keymap.encoders[layer]?.[e];
      if (was && !same(was, encoders[e])) {
        await board.setEncoder(layer, e, encoders[e]!);
        writes++;
      }
    }
  }

  if (backup.settings && now.settings && !same(backup.settings, now.settings)) {
    await board.setSettings(backup.settings);
    writes++;
  }
  if (backup.advanced && now.advanced && board.setAdvancedEntry && board.setAdvancedOptions) {
    if (!same(backup.advanced.options, now.advanced.options)) {
      await board.setAdvancedOptions(backup.advanced.options);
      writes++;
    }
    for (const table of TABLES) {
      const want = backup.advanced[table];
      for (let i = 0; i < Math.min(want.length, now.advanced[table].length); i++) {
        if (!same(want[i], now.advanced[table][i])) {
          await board.setAdvancedEntry(table, i, want[i]!);
          writes++;
        }
      }
    }
  }
  if (backup.macros && now.macros && board.setMacros && !same(backup.macros, now.macros)) {
    await board.setMacros(backup.macros);
    writes++;
  }
  if (backup.indicators && now.indicators && board.setIndicators && !same(backup.indicators, now.indicators)) {
    const layerColours = [...now.indicators.layers];
    backup.indicators.layers.slice(0, layerColours.length).forEach((c, i) => (layerColours[i] = c));
    await board.setIndicators({ ...backup.indicators, layers: layerColours });
    writes++;
  }

  if (writes > 0 && has(board, "persistence")) await board.save();
  return writes;
}
