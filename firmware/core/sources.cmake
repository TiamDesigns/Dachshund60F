# Source list for the hardware-independent core, used by both the Zephyr
# build (firmware/CMakeLists.txt) and the host test build (core/CMakeLists.txt).
set(KB_CORE_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include)
set(KB_CORE_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/src/core.c
  ${CMAKE_CURRENT_LIST_DIR}/src/debounce.c
  ${CMAKE_CURRENT_LIST_DIR}/src/engine.c
  ${CMAKE_CURRENT_LIST_DIR}/src/hid.c
  ${CMAKE_CURRENT_LIST_DIR}/src/hold.c
  ${CMAKE_CURRENT_LIST_DIR}/src/indicators.c
  ${CMAKE_CURRENT_LIST_DIR}/src/keycodes.c
  ${CMAKE_CURRENT_LIST_DIR}/src/keymap.c
  ${CMAKE_CURRENT_LIST_DIR}/src/protocol.c
  ${CMAKE_CURRENT_LIST_DIR}/src/protocol_gen.c
  ${CMAKE_CURRENT_LIST_DIR}/src/screen.c
  ${CMAKE_CURRENT_LIST_DIR}/src/settings.c
  ${CMAKE_CURRENT_LIST_DIR}/src/store.c
  ${CMAKE_CURRENT_LIST_DIR}/src/version.c
)
