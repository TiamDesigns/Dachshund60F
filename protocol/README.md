# protocol/

The single definition of the config protocol spoken over the vendor HID
interface, plus the generators that turn it into firmware (C) and host
(TypeScript) bindings.

- `protocol.yaml` — the only place commands, payloads and error codes are
  defined. Written in phase 5.
- `gen/` — code generators. Generated files are never hand-edited; regenerate.

Rules (see CLAUDE.md): bump the protocol version on any breaking change; the
app refuses writes on a version mismatch; ask before changing the frame format.
