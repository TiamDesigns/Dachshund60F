<script lang="ts">
  import { fly } from "svelte/transition";

  import type { Toast } from "../app-state.svelte.ts";

  let { toasts }: { toasts: Toast[] } = $props();
</script>

<div class="toasts" role="status" aria-live="polite">
  {#each toasts as toast (toast.id)}
    <div class="toast {toast.kind}" transition:fly={{ y: 12, duration: 180 }}>
      <svg width="16" height="16" viewBox="0 0 24 24" aria-hidden="true">
        <!-- paw print -->
        <ellipse cx="12" cy="16" rx="5" ry="4" fill="currentColor" />
        <circle cx="6" cy="10" r="2.2" fill="currentColor" />
        <circle cx="10" cy="6.5" r="2.2" fill="currentColor" />
        <circle cx="14" cy="6.5" r="2.2" fill="currentColor" />
        <circle cx="18" cy="10" r="2.2" fill="currentColor" />
      </svg>
      {toast.text}
    </div>
  {/each}
</div>

<style>
  .toasts {
    position: fixed;
    right: 20px;
    bottom: 20px;
    display: flex;
    flex-direction: column;
    gap: 8px;
    z-index: 10;
    pointer-events: none;
  }

  .toast {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 10px 14px;
    border-radius: var(--radius);
    background: var(--surface);
    border: 1px solid var(--border);
    box-shadow: var(--shadow-lg);
    font-weight: 600;
    max-width: 360px;
  }

  .success svg {
    color: var(--sage);
  }

  .info svg {
    color: var(--ochre);
  }

  .error {
    border-color: var(--danger);
  }

  .error svg {
    color: var(--danger);
  }
</style>
