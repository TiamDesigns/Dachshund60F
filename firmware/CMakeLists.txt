# Keyboard firmware (nRF52840), Zephyr application.
# Build for the Dachshund60F with -DBOARD_ROOT=<repo root> (see CLAUDE.md).
# Add a screen with -DEXTRA_DTC_OVERLAY_FILE=displays/<panel>.overlay.
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(kb_firmware LANGUAGES C)

# Hardware-independent keymap logic, shared with the host unit tests.
include(${CMAKE_CURRENT_SOURCE_DIR}/core/sources.cmake)

target_sources(app PRIVATE
  src/main.c
  src/bootloader.c
  src/config.c
  src/default_keymap.c
  src/encoder_input.c
  src/fw_info.c
  src/indicators.c
  src/kb_ble.c
  src/kb_usb.c
  src/keyboard.c
  src/matrix.c
  src/output.c
  src/power.c
  src/storage.c
  ${KB_CORE_SOURCES}
)
target_sources_ifdef(CONFIG_KB_SCREEN app PRIVATE src/status_screen.c)
target_sources_ifdef(CONFIG_KB_DONGLE app PRIVATE src/kb_esb.c)
target_include_directories(app PRIVATE ${KB_CORE_INCLUDE_DIRS})
