<script lang="ts">
  let { size = 34 }: { size?: number } = $props();
</script>

<div class="logo">
  <svg width={size} height={size} viewBox="0 0 64 64" aria-hidden="true">
    <path
      d="M8 30 32 8l24 22"
      fill="none"
      stroke="var(--accent-hover)"
      stroke-width="6"
      stroke-linecap="round"
      stroke-linejoin="round"
    />
    <path d="M14 28v28h36V28L32 12z" fill="var(--accent)" />
    <path d="M24 56V44a8 8 0 0 1 16 0v12z" fill="var(--text)" />
    <circle cx="32" cy="30" r="3.5" fill="var(--ochre-soft)" />
  </svg>
  <span class="wordmark">Doghouse</span>
</div>

<style>
  .logo {
    display: flex;
    align-items: center;
    gap: 10px;
  }

  .wordmark {
    font-family: var(--font-display);
    font-size: 21px;
    font-weight: 700;
    letter-spacing: -0.02em;
  }
</style>
