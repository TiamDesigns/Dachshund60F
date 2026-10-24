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

## Advanced keys and macros

Combos, tap dance, key overrides, Leader sequences, per-key tap-hold rules,
the encoder mapping and their options live in fixed tables (`feature_tables`
in `protocol.yaml`, `kb/features.h` in the firmware). `get_feature` and
`set_feature` read and write one entry at a time, in the byte layout the
YAML gives. A write is checked against all the tables together (positions on
the board, keycodes, no tap dance inside a combo, and so on), and on an error
nothing changes. Fields are only ever appended to an entry; a shorter entry
from an older app keeps the board's values for the rest.

Macros are one 1 KB buffer. The host writes it in chunks with
`set_macro_chunk` (nothing changes on the keyboard meanwhile) and then sends
`apply_macros`, which checks every step and swaps it in, or drops the edit.

## Settings and indicator colours

`get_settings` and `set_settings` carry the global settings as one struct:
debounce algorithm and time, tapping term, NKRO, sleep timeout, indicator
brightness and the board's name (16 bytes of UTF-8). A write replaces all of
them, is checked whole (the limits are in `kb/settings.h`), and takes effect
at once; `save` keeps it.

Indicator colours are 0x00RRGGBB, before the brightness setting scales
them; black is off. `get_indicators` and `set_indicators` hold the Caps,
Num and Scroll Lock colours, the battery gauge's normal and low colours, and
the low level. The layer LED's colour is per layer, read and written with
`get_layer_colour` and `set_layer_colour` for the layers the keymap has.

## Rules (see CLAUDE.md)

- Bump `version` on any breaking change: removing or renumbering a command,
  or changing a field. Adding a command or a status is not breaking.
- The app refuses writes on a version mismatch.
- Ask before changing the frame format.
