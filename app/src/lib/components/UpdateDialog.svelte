<script lang="ts">
  import type { FirmwareUpdate, UpdateStage } from "../firmware-update.svelte.ts";
  import Dachshund from "./Dachshund.svelte";

  let { update, onclose }: { update: FirmwareUpdate; onclose: () => void } = $props();

  const STEPS: { stages: UpdateStage[]; label: string }[] = [
    { stages: ["backup"], label: "Back up its settings" },
    { stages: ["bootloader"], label: "Restart into the bootloader" },
    { stages: ["drive", "writing"], label: "Copy the firmware onto its drive" },
    { stages: ["waiting"], label: "Wait for it to come back" },
    { stages: ["restoring"], label: "Check the version and put settings back" },
  ];
  const ORDER: UpdateStage[] = ["ready", "backup", "bootloader", "drive", "writing", "waiting", "restoring", "done"];

  function stepState(step: (typeof STEPS)[number]): "done" | "active" | "todo" {
    if (update.stage === "done") return "done";
    const at = ORDER.indexOf(update.stage);
    if (step.stages.includes(update.stage)) return "active";
    return ORDER.indexOf(step.stages[0]!) < at ? "done" : "todo";
  }

  // Once the board has left, closing the dialog leaves it in its bootloader.
  const busy = $derived(["backup", "bootloader", "writing", "restoring"].includes(update.stage));
  const midway = $derived(["drive", "writing", "waiting"].includes(update.stage));

  let fileUrl = $state<string | null>(null);
  let backupUrl = $state<string | null>(null);
  $effect(() => {
    if (!update.backup) return;
    const url = update.backupUrl();
    backupUrl = url;
    return () => {
      if (url) URL.revokeObjectURL(url);
    };
  });
  $effect(() => () => {
    if (fileUrl) URL.revokeObjectURL(fileUrl);
  });

  function download() {
    fileUrl ??= update.fileUrl();
  }

  function close() {
    if (midway && !confirm(`${update.boardName} is waiting in its bootloader. Close anyway? Double-tap its reset button to start the old firmware again.`)) {
      return;
    }
    onclose();
  }
</script>

<div class="backdrop" role="presentation">
  <div class="dialog" role="dialog" aria-modal="true" aria-labelledby="update-title">
    <header>
      <h2 id="update-title">Update {update.boardName}</h2>
      <button class="btn ghost small" onclick={close} disabled={busy} aria-label="Close">✕</button>
    </header>

    <div class="versions">
      <div>
        <span class="tag">Now</span>
        <strong class="mono">{update.fromVersion}</strong>
      </div>
      <span class="arrow" aria-hidden="true">→</span>
      <div>
        <span class="tag">{update.direction > 0 ? "Newer" : update.direction < 0 ? "Older" : "Same"}</span>
        <strong class="mono">{update.toVersion}</strong>
      </div>
      <small class="file">{update.fileName} · {update.image.blocks} blocks · {Math.round((update.image.end - update.image.start) / 1024)} KB</small>
    </div>

    {#if update.stage === "ready"}
      {#each update.warnings as w (w)}
        <p class="warn">{w}</p>
      {/each}
      <p>
        Doghouse backs up the keymap and settings first, then restarts the board into its bootloader.
        {#if update.findsDrive && !update.demo}
          It copies the firmware onto the board's drive (<strong>{update.volume}</strong>) and checks the result when the board comes back.
        {:else}
          You'll pick its drive {#if !update.demo}(<strong>{update.volume}</strong>){/if} to copy the firmware onto, and Doghouse checks the result when it comes back.
        {/if}
      </p>
      <div class="actions">
        <button class="btn ghost" onclick={onclose}>Cancel</button>
        <button class="btn primary" onclick={() => update.start()}>Back up and update</button>
      </div>
    {:else}
      <ol class="steps">
        {#each STEPS as step (step.label)}
          {@const s = stepState(step)}
          <li class={s}>
            <span class="dot" aria-hidden="true">{s === "done" ? "✓" : s === "active" ? "•" : ""}</span>
            {step.label}
          </li>
        {/each}
      </ol>

      {#if update.stage === "drive"}
        {#if update.demo}
          <p>This is a demo board, so there's no real drive. Doghouse pretends to copy the file.</p>
          <div class="actions">
            <button class="btn primary" onclick={() => update.writeToDrive()}>Copy to the pretend drive</button>
          </div>
        {:else if update.findsDrive}
          <p>Doghouse looks for the <strong>{update.volume}</strong> drive and copies the file onto it.</p>
          <div class="actions">
            <button class="btn primary" onclick={() => update.writeToDrive()}>Try again</button>
          </div>
        {:else if update.canPickDrive}
          <p>A drive called <strong>{update.volume}</strong> has appeared. Choose it, and let the browser edit files there.</p>
          <div class="actions">
            <button class="btn primary" onclick={() => update.writeToDrive()}>Choose the {update.volume} drive…</button>
          </div>
        {:else}
          <p>This browser can't write to drives. Download the file and copy it onto the <strong>{update.volume}</strong> drive yourself.</p>
          <div class="actions">
            {#if fileUrl}
              <a class="btn" href={fileUrl} download={update.fileName}>Download {update.fileName}</a>
            {:else}
              <button class="btn" onclick={download}>Get the file</button>
            {/if}
            <button class="btn primary" onclick={() => update.copiedByHand()}>I've copied it</button>
          </div>
        {/if}
        {#if update.message}<p class="warn">{update.message}</p>{/if}
      {:else if update.stage === "waiting" || update.stage === "writing"}
        <div class="wait">
          <Dachshund width={90} />
          <p>Copying and restarting. The board drops off and comes back on its own; this takes a few seconds.</p>
        </div>
        {#if update.slow}
          <p class="warn">
            Still waiting. If its drive is still showing, the file didn't copy: try again. Otherwise unplug the board and plug it back in.
            {#if !update.demo && !update.findsDrive}If Doghouse doesn't see it, click <strong>Connect a board…</strong>.{/if}
          </p>
          {#if !update.demo && update.canPickDrive}
            <div class="actions"><button class="btn" onclick={() => (update.stage = "drive")}>{update.findsDrive ? "Copy it again" : "Pick the drive again"}</button></div>
          {/if}
        {/if}
      {:else if update.stage === "done"}
        <p class="ok">
          {update.boardName} runs {update.toVersion}.
          {#if update.restored === null}
            It speaks a different config protocol, so its settings couldn't be checked; update Doghouse to edit it.
          {:else if update.restored === 0}
            All its settings came through.
          {:else}
            Doghouse put back {update.restored} setting{update.restored === 1 ? "" : "s"} the update reset, and saved them.
          {/if}
        </p>
        <div class="actions"><button class="btn primary" onclick={onclose}>Done</button></div>
      {:else if update.stage === "failed"}
        <p class="warn">{update.message}</p>
        <div class="actions"><button class="btn" onclick={onclose}>Close</button></div>
      {/if}

      {#if backupUrl && update.stage !== "done"}
        <p class="backup">
          <a href={backupUrl} download="{update.boardName}-backup.json">Download the backup</a> to keep a copy; About → Restore a backup puts it back.
        </p>
      {/if}
    {/if}
  </div>
</div>

<style>
  .backdrop {
    position: fixed;
    inset: 0;
    z-index: 50;
    display: grid;
    place-items: center;
    padding: 16px;
    background: color-mix(in srgb, var(--text) 35%, transparent);
  }

  .dialog {
    width: min(520px, 100%);
    max-height: calc(100vh - 32px);
    overflow: auto;
    padding: 20px 22px;
    border-radius: var(--radius-lg);
    background: var(--surface);
    border: 1px solid var(--border);
    box-shadow: var(--shadow-lg, 0 12px 40px rgb(0 0 0 / 0.25));
    display: flex;
    flex-direction: column;
    gap: 14px;
  }

  header {
    display: flex;
    justify-content: space-between;
    align-items: center;
  }

  h2 {
    font-size: 17px;
    margin: 0;
  }

  p {
    margin: 0;
    color: var(--text-muted);
  }

  .versions {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 6px 16px;
    padding: 12px 14px;
    border-radius: var(--radius-sm);
    background: var(--surface-3);
  }

  .versions > div {
    display: flex;
    flex-direction: column;
  }

  .tag {
    font-size: 12px;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-muted);
  }

  .mono {
    font-family: var(--font-mono);
    font-size: 16px;
  }

  .arrow {
    font-size: 20px;
    color: var(--text-muted);
  }

  .file {
    flex-basis: 100%;
    color: var(--text-faint);
    font-size: 12px;
  }

  .warn {
    color: var(--danger);
  }

  .ok {
    color: var(--text);
  }

  .steps {
    list-style: none;
    margin: 0;
    padding: 0;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }

  .steps li {
    display: flex;
    align-items: center;
    gap: 10px;
    color: var(--text-faint);
  }

  .steps li.active {
    color: var(--text);
    font-weight: 600;
  }

  .steps li.done {
    color: var(--text-muted);
  }

  .dot {
    width: 20px;
    height: 20px;
    flex: none;
    display: grid;
    place-items: center;
    border-radius: 50%;
    font-size: 12px;
    border: 1.5px solid var(--border-strong);
  }

  .done .dot {
    background: var(--sage);
    border-color: var(--sage);
    color: var(--surface);
  }

  .active .dot {
    border-color: var(--accent);
    color: var(--accent);
  }

  .wait {
    display: flex;
    align-items: center;
    gap: 14px;
  }

  .actions {
    display: flex;
    justify-content: flex-end;
    gap: 8px;
    flex-wrap: wrap;
  }

  .backup {
    font-size: 13px;
  }
</style>
