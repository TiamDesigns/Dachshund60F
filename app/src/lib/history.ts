/**
 * Undo/redo history for keymap edits. The editor applies an edit's `after`
 * to the board on redo and its `before` on undo.
 */

import type { EncoderAction, KeycodeId } from "@dachshund60f/host-lib";

export type Edit =
  | { kind: "key"; layer: number; row: number; col: number; before: KeycodeId; after: KeycodeId }
  | { kind: "encoder"; layer: number; encoder: number; before: EncoderAction; after: EncoderAction };

function isNoOp(edit: Edit): boolean {
  if (edit.kind === "key") return edit.before === edit.after;
  return edit.before.cw === edit.after.cw && edit.before.ccw === edit.after.ccw;
}

export class EditHistory {
  private undoStack: Edit[] = [];
  private redoStack: Edit[] = [];
  private readonly limit: number;

  constructor(limit = 200) {
    this.limit = limit;
  }

  /** Record a new edit. No-op edits are ignored; a new edit clears redo. */
  record(edit: Edit): void {
    if (isNoOp(edit)) return;
    this.undoStack.push(edit);
    if (this.undoStack.length > this.limit) this.undoStack.shift();
    this.redoStack = [];
  }

  /** The edit to reverse, or undefined if there is nothing to undo. */
  undo(): Edit | undefined {
    const edit = this.undoStack.pop();
    if (edit) this.redoStack.push(edit);
    return edit;
  }

  /** The edit to reapply, or undefined if there is nothing to redo. */
  redo(): Edit | undefined {
    const edit = this.redoStack.pop();
    if (edit) this.undoStack.push(edit);
    return edit;
  }

  get canUndo(): boolean {
    return this.undoStack.length > 0;
  }

  get canRedo(): boolean {
    return this.redoStack.length > 0;
  }

  clear(): void {
    this.undoStack = [];
    this.redoStack = [];
  }
}
