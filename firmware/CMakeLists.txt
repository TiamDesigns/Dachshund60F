# Keyboard firmware (nRF52840), Zephyr application.
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(kb_firmware LANGUAGES C)

# Hardware-independent keymap logic, shared with the host unit tests.
include(${CMAKE_CURRENT_SOURCE_DIR}/core/sources.cmake)

target_sources(app PRIVATE
  src/main.c
  ${KB_CORE_SOURCES}
)
target_include_directories(app PRIVATE ${KB_CORE_INCLUDE_DIRS})
