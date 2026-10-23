# Vendored footprints

`keyswitch.pretty/` holds copies of the footprints this board uses
from [perigoso/keyswitch-kicad-library](https://github.com/perigoso/keyswitch-kicad-library)
v2.4 (installed through KiCad's Plugin and Content Manager), licensed
[CC-BY-SA-4.0](https://creativecommons.org/licenses/by-sa/4.0/).

- `SW_Hotswap_Kailh_MX_{1.00,1.25,1.50,1.75,2.00,2.25,2.75,6.25}u`
- `Stabilizer_Cherry_MX_{2.00,6.25}u`

They are copied in so the project opens without the PCM package and so the
Konnect MCP server, which cannot expand `${KICAD10_3RD_PARTY}`, can resolve
them. The project `fp-lib-table` registers the folder as `Keyswitch` through
`${KIPRJMOD}`.

The only change from upstream is the file format: the hot-swap footprints
were saved in the KiCad 5 format, which Konnect cannot parse, so the folder
was run through `kicad-cli fp upgrade --force` (KiCad 10.0.3). Geometry is
unchanged. To pick up an upstream fix, copy the file again and rerun that
command.

# Project footprints

`Dachshund60F.pretty/` holds footprints changed for this board, registered
as `Dachshund60F` in the project `fp-lib-table`.

- `LED_SK6812MINI-E_3.2x2.8mm_P1.5mm_ReverseMount_Cutout3.4`: KiCad's
  `LED_SMD:LED_SK6812MINI-E_3.2x2.8mm_P1.5mm_ReverseMount` with its
  corner-relief light cutout replaced by a plain 3.4 × 3.4 mm cutout with
  0.5 mm corners. The relief corners came within 0.247 mm of the pads; the
  plain cutout keeps them 0.35 mm away, and the LED body still clears the
  rounded corners.
