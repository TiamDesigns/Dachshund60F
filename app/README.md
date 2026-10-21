# app/

The configurator GUI. Built on `host-lib/` and developed against its mock
device first (phase 8).

GUI shell is still open: Tauri (recommended in SPEC.md for signing and
auto-update) or a hosted WebHID web app. Either way the UI is TypeScript and
talks to the device only through host-lib's typed API.
