/*
 * Hardware- and transport-independent keyboard core.
 *
 * Everything under firmware/core/ must build and run on a host: no Zephyr,
 * nrfx, USB or BLE headers. Keycodes (kb/keycodes.h), the keymap
 * (kb/keymap.h), debounce (kb/debounce.h) and key processing (kb/engine.h)
 * live here, with tap-hold, combos and macros to follow, and are driven by
 * simulated key events and timestamps in the host tests.
 */
#ifndef KB_CORE_H
#define KB_CORE_H

#include <stdint.h>

/* Bumped whenever the core's public interface changes. */
#define KB_CORE_API_VERSION 2u

uint32_t kb_core_api_version(void);

#endif /* KB_CORE_H */
