# protocol/

The single definition of the config protocol spoken over the vendor HID
interface, plus the generator that turns it into firmware (C) and host
(TypeScript) bindings.

- `protocol.yaml`: the only place commands, payloads, status codes and
  capability bits are defined, along with the frame format and the HID
  interface (vendor usage page 0xFF00, 64-byte reports).
- `gen/gen.py`: the generator. It needs Python 3 and PyYAML, which every NCS
  install already has. Run it from the repo root after changing
  `protocol.yaml`, and commit what it writes:

      python protocol/gen/gen.py

  It writes `firmware/core/include/kb/protocol_gen.h`,
  `firmware/core/src/protocol_gen.c` and `host-lib/src/protocol.gen.ts`,
  and `host-lib/src/keycodes.gen.ts` from the keycode numbers in
  `firmware/core/include/kb/keycodes.h`.
  Never edit those by hand. CI runs `python protocol/gen/gen.py --check` and
  fails if they don't match `protocol.yaml`.

The firmware's request handler is `firmware/core/src/protocol.c`. It checks
every index and length and is fuzzed in CI (`firmware/core/fuzz/`). The host
frame helpers are `host-lib/src/protocol.ts`.

## Frame

Every report is one frame, padded with zeros to 64 bytes:

| Byte | Request (host to device) | Response (device to host) |
| --- | --- | --- |
| 0 | command | command (echoed) |
| 1 | sequence | sequence (echoed) |
| 2 | payload length (0-60) | payload length |
| 3 | protocol version | status |
| 4-63 | payload | payload |

The device answers every command except `handshake` with
`version_mismatch` when byte 3 isn't its protocol version, so an app built
for another version can't change anything. Error responses carry no
payload. There's no CRC, because USB, BLE and the 2.4 GHz link already check
every packet.

## Rules (see CLAUDE.md)

- Bump `version` on any breaking change: removing or renumbering a command,
  or changing a field. Adding a command or a status is not breaking.
- The app refuses writes on a version mismatch.
- Ask before changing the frame format.
