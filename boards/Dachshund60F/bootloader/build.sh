#!/usr/bin/env bash
# Build the Adafruit nRF52 UF2 bootloader for the Dachshund60F.
# Needs git, cmake, ninja, arm-none-eabi-gcc and Python 3 with intelhex.
# Usage: boards/Dachshund60F/bootloader/build.sh [workdir]   (default: build/bootloader)
set -euo pipefail

# Adafruit_nRF52_Bootloader commit this board was checked against (master,
# 2026-05-21). The 0.9.2 release predates the BUTTON_DFU board API used here.
REV=c67f0bcf0fa8e841426335b1bbde91cda6ca1f50
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${1:-build/bootloader}"
SRC="$WORK/Adafruit_nRF52_Bootloader"

# Not a shallow clone: the bootloader's CMake takes its version from
# `git describe --tags`, which needs the history back to the last tag.
if [ ! -d "$SRC/.git" ]; then
  git clone --filter=blob:none https://github.com/adafruit/Adafruit_nRF52_Bootloader.git "$SRC"
fi
git -C "$SRC" checkout -q "$REV"
git -C "$SRC" submodule update --init --depth 1 lib/tinyusb lib/nrfx lib/uf2

mkdir -p "$SRC/src/boards/dachshund60f"
cp "$HERE"/board.h "$HERE"/board.mk "$HERE"/board.cmake "$HERE"/pinconfig.c "$SRC/src/boards/dachshund60f/"

# Configure from inside the bootloader checkout: its CMake runs `git describe`
# in the current directory to get the version number.
cd "$SRC"
cmake -S . -B _build/dachshund60f -G Ninja -DBOARD=dachshund60f
cmake --build _build/dachshund60f

# One file for the first SWD flash: MBR + S140 SoftDevice + bootloader + UICR.
OUT="$PWD/_build/dachshund60f"
python3 tools/hexmerge.py -o "$OUT/dachshund60f_bootloader_s140_6.1.1.hex" \
  "$OUT/bootloader.hex" lib/softdevice/s140_nrf52_6.1.1/s140_nrf52_6.1.1_softdevice.hex
echo
echo "SWD (blank chip):  $OUT/dachshund60f_bootloader_s140_6.1.1.hex"
echo "UF2 (update only): $OUT/bootloader_mbr.uf2"
