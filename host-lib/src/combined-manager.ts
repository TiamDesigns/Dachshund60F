/**
 * Several board managers shown as one list, e.g. real boards over WebHID
 * next to simulated ones in demo mode.
 */
import type { Board, BoardManager } from "./types.js";

export class CombinedBoardManager implements BoardManager {
  private readonly listeners = new Set<(boards: Board[]) => void>();

  constructor(readonly managers: readonly BoardManager[]) {
    for (const manager of managers) {
      manager.subscribe(() => {
        const boards = this.list();
        for (const listener of this.listeners) listener(boards);
      });
    }
  }

  list(): Board[] {
    return this.managers.flatMap((manager) => manager.list());
  }

  subscribe(listener: (boards: Board[]) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }
}
