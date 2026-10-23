/*
 * Keymap storage and lookup by (layer, row, col).
 *
 * The keymap is a flat array of keycodes, keys[(layer * rows + row) * cols
 * + col], owned by the caller: the firmware keeps a RAM copy it can edit
 * live, starting from the compiled-in default.
 */
#ifndef KB_KEYMAP_H
#define KB_KEYMAP_H

#include <stddef.h>
#include <stdint.h>

#include "kb/keycodes.h"
#include "kb/matrix.h"

/* Layer-tap keycodes reach layers 0-15, so 16 is the most a keymap can use. */
#define KB_MAX_LAYERS 16u

struct kb_keymap {
    uint8_t layers;
    uint8_t rows;
    uint8_t cols;
    kb_keycode_t *keys;
};

static inline size_t kb_keymap_size(uint8_t layers, uint8_t rows, uint8_t cols)
{
    return (size_t)layers * rows * cols;
}

/*
 * Point a keymap at storage for layers x rows x cols keycodes and copy the
 * default into it (or fill it with KC_NO if def is NULL). Keycodes in the
 * default that are not valid become KC_NO. Returns the number of keycodes
 * replaced, or -1 if a size is out of range.
 */
int kb_keymap_init(struct kb_keymap *km, kb_keycode_t *storage, uint8_t layers, uint8_t rows,
                   uint8_t cols, const kb_keycode_t *def);

/* Keycode at (layer, row, col), or KC_NO if any index is out of range. */
kb_keycode_t kb_keymap_get(const struct kb_keymap *km, uint8_t layer, uint8_t row, uint8_t col);

/* Set one key. Returns 0, or -1 for an index out of range or an invalid keycode. */
int kb_keymap_set(struct kb_keymap *km, uint8_t layer, uint8_t row, uint8_t col, kb_keycode_t kc);

/*
 * The keycode a key sends with the layers in layer_mask active: the highest
 * active layer whose key is not KC_TRNS. All transparent gives KC_NO.
 */
kb_keycode_t kb_keymap_resolve(const struct kb_keymap *km, uint32_t layer_mask, uint8_t row,
                               uint8_t col);

#endif /* KB_KEYMAP_H */
