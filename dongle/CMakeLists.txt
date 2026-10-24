# 2.4 GHz dongle firmware (second nRF52840), Zephyr application.
cmake_minimum_required(VERSION 3.20.0)

# The same USB HID interfaces as the keyboard.
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/../firmware/app.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(kb_dongle LANGUAGES C)

# The link packets and HID reports come from the keyboard's core; the USB
# device is the keyboard's too (its hooks are in src/hooks.c).
include(${CMAKE_CURRENT_SOURCE_DIR}/../firmware/core/sources.cmake)

target_sources(app PRIVATE
  src/main.c
  src/hooks.c
  src/led.c
  src/radio.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/kb_usb.c
  ${KB_CORE_SOURCES}
)
# firmware/src for the prototypes of the hooks kb_usb.c calls.
target_include_directories(app PRIVATE src ${KB_CORE_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src)
