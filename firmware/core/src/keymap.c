#include "kb/keymap.h"

static size_t index_of(const struct kb_keymap *km, uint8_t layer, uint8_t row, uint8_t col)
{
    return ((size_t)layer * km->rows + row) * km->cols + col;
}

static bool in_range(const struct kb_keymap *km, uint8_t layer, uint8_t row, uint8_t col)
{
    return layer < km->layers && row < km->rows && col < km->cols;
}

int kb_keymap_init(struct kb_keymap *km, kb_keycode_t *storage, uint8_t layers, uint8_t rows,
                   uint8_t cols, const kb_keycode_t *def)
{
    if (layers == 0 || layers > KB_MAX_LAYERS || rows == 0 || rows > KB_MATRIX_MAX_ROWS ||
        cols == 0 || cols > KB_MATRIX_MAX_COLS || storage == NULL) {
        return -1;
    }

    km->layers = layers;
    km->rows = rows;
    km->cols = cols;
    km->keys = storage;

    int replaced = 0;
    size_t n = kb_keymap_size(layers, rows, cols);
    for (size_t i = 0; i < n; i++) {
        kb_keycode_t kc = def ? def[i] : KC_NO;
        if (!kb_keycode_is_valid(kc, layers)) {
            kc = KC_NO;
            replaced++;
        }
        storage[i] = kc;
    }
    return replaced;
}

kb_keycode_t kb_keymap_get(const struct kb_keymap *km, uint8_t layer, uint8_t row, uint8_t col)
{
    return in_range(km, layer, row, col) ? km->keys[index_of(km, layer, row, col)] : KC_NO;
}

int kb_keymap_set(struct kb_keymap *km, uint8_t layer, uint8_t row, uint8_t col, kb_keycode_t kc)
{
    if (!in_range(km, layer, row, col) || !kb_keycode_is_valid(kc, km->layers)) {
        return -1;
    }
    km->keys[index_of(km, layer, row, col)] = kc;
    return 0;
}

kb_keycode_t kb_keymap_resolve(const struct kb_keymap *km, uint32_t layer_mask, uint8_t row,
                               uint8_t col)
{
    if (row >= km->rows || col >= km->cols) {
        return KC_NO;
    }

    for (int layer = km->layers - 1; layer >= 0; layer--) {
        if (layer_mask & (1u << layer)) {
            kb_keycode_t kc = km->keys[index_of(km, (uint8_t)layer, row, col)];
            if (kc != KC_TRNS) {
                return kc;
            }
        }
    }
    return KC_NO;
}
