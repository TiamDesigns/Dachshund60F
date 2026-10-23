/*
 * What the board saves to flash: the keymap and the global settings,
 * encoded as one blob for kb/store.h.
 *
 * Blob format (schema KB_SETTINGS_SCHEMA): a list of sections, each
 *   tag u8, length u16 (little-endian), body
 * Sections:
 *   KB_SECTION_KEYMAP:   layers u8, rows u8, cols u8, keycodes u16[]
 *   KB_SECTION_SETTINGS: the fields of struct kb_settings, in order
 *   KB_SECTION_FEATURES: blocks of table u8, entry size u8, count u16,
 *                        entries (the advanced-key tables, in the layout
 *                        of protocol.yaml's feature_tables)
 *   KB_SECTION_MACROS:   the macro buffer, without its trailing zeros
 * Unknown sections are skipped and missing ones keep their defaults, so
 * adding a section or appending a field to one needs no new schema. A
 * change that can't be read that way bumps KB_SETTINGS_SCHEMA and adds a
 * migration from the old schema in kb_settings_decode().
 */
#ifndef KB_SETTINGS_H
#define KB_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kb/features.h"
#include "kb/keymap.h"

#define KB_SETTINGS_SCHEMA 1u

#define KB_SECTION_KEYMAP 1u
#define KB_SECTION_SETTINGS 2u
#define KB_SECTION_FEATURES 3u
#define KB_SECTION_MACROS 4u

#define KB_SETTINGS_NAME_MAX 16u

/* Limits the firmware accepts; out-of-range stored values fall back to defaults. */
#define KB_SETTINGS_TAPPING_TERM_MIN 50u
#define KB_SETTINGS_TAPPING_TERM_MAX 2000u
#define KB_SETTINGS_SLEEP_TIMEOUT_MAX 240u
#define KB_SETTINGS_BRIGHTNESS_MAX 100u

struct kb_settings {
    uint8_t debounce_algo;  /* enum kb_debounce_algo */
    uint8_t debounce_ms;    /* 0 - KB_DEBOUNCE_MAX_MS */
    uint16_t tapping_term_ms;
    bool nkro;
    uint8_t sleep_timeout_min;    /* 0 = never */
    uint8_t indicator_brightness; /* percent */
    char name[KB_SETTINGS_NAME_MAX + 1]; /* the owner's name for the board */
};

/* True if every field is in range. */
bool kb_settings_valid(const struct kb_settings *s);

/* Bytes kb_settings_encode needs for this keymap. */
size_t kb_settings_encoded_size(const struct kb_keymap *km);

/* Encode keymap and settings into buf. Returns the length, or 0 if cap is too small. */
size_t kb_settings_encode(const struct kb_keymap *km, const struct kb_settings *s, uint8_t *buf,
                          size_t cap);

/* What kb_settings_decode loaded (bits); the rest kept its defaults. */
#define KB_LOADED_KEYMAP 0x1
#define KB_LOADED_SETTINGS 0x2

/*
 * Decode a blob saved with the given schema over km and s, which hold the
 * defaults on entry. The keymap loads only if its size matches km's (a
 * different board or layer count keeps the default keymap); invalid
 * keycodes become KC_NO and invalid settings fields keep their defaults.
 * Returns the KB_LOADED_* bits, or -1 for a schema this firmware can't
 * read (saved by newer firmware) or a malformed blob, leaving km and s as
 * they were.
 */
int kb_settings_decode(uint16_t schema, const uint8_t *buf, size_t len, struct kb_keymap *km,
                       struct kb_settings *s);

/*
 * The advanced-key tables and macros, as two more sections to append to
 * the blob from kb_settings_encode. Returns the length, or 0 if cap is too
 * small. layers is the keymap's (encoder entries are saved for those).
 */
size_t kb_features_encoded_size(const struct kb_features *f, uint8_t layers);
size_t kb_features_encode(const struct kb_features *f, uint8_t layers, uint8_t *buf, size_t cap);

/*
 * Load the feature sections of a blob over f, which holds the defaults on
 * entry. Entries are checked as a whole with kb_features_valid for this
 * keymap's size; if they fail, f keeps its defaults. Returns 1 if anything
 * loaded, 0 if not (nothing saved, or invalid), or -1 like
 * kb_settings_decode.
 */
int kb_features_decode(uint16_t schema, const uint8_t *buf, size_t len, struct kb_features *f,
                       uint8_t layers, uint8_t rows, uint8_t cols);

#endif /* KB_SETTINGS_H */
