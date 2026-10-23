#include "kb/settings.h"

#include <string.h>

#include "kb/debounce.h"
#include "kb/keycodes.h"

#define SECTION_HEADER 3u
#define KEYMAP_HEADER 3u
/* debounce_algo, debounce_ms, tapping_term (2), nkro, sleep, brightness, name */
#define SETTINGS_V1_SIZE (7u + KB_SETTINGS_NAME_MAX)

static bool name_valid(const char *name)
{
    size_t n = 0;
    while (n <= KB_SETTINGS_NAME_MAX && name[n] != '\0') {
        n++;
    }
    return n <= KB_SETTINGS_NAME_MAX;
}

bool kb_settings_valid(const struct kb_settings *s)
{
    return s->debounce_algo < KB_DEBOUNCE_ALGO_COUNT && s->debounce_ms <= KB_DEBOUNCE_MAX_MS &&
           s->tapping_term_ms >= KB_SETTINGS_TAPPING_TERM_MIN &&
           s->tapping_term_ms <= KB_SETTINGS_TAPPING_TERM_MAX &&
           s->sleep_timeout_min <= KB_SETTINGS_SLEEP_TIMEOUT_MAX &&
           s->indicator_brightness <= KB_SETTINGS_BRIGHTNESS_MAX && name_valid(s->name);
}

static size_t keymap_bytes(const struct kb_keymap *km)
{
    return KEYMAP_HEADER + 2u * kb_keymap_size(km->layers, km->rows, km->cols);
}

size_t kb_settings_encoded_size(const struct kb_keymap *km)
{
    return SECTION_HEADER + keymap_bytes(km) + SECTION_HEADER + SETTINGS_V1_SIZE;
}

static uint8_t *section(uint8_t *p, uint8_t tag, size_t len)
{
    p[0] = tag;
    p[1] = (uint8_t)len;
    p[2] = (uint8_t)(len >> 8);
    return p + SECTION_HEADER;
}

size_t kb_settings_encode(const struct kb_keymap *km, const struct kb_settings *s, uint8_t *buf,
                          size_t cap)
{
    size_t total = kb_settings_encoded_size(km);
    if (cap < total || keymap_bytes(km) > 0xFFFFu) {
        return 0;
    }

    uint8_t *p = section(buf, KB_SECTION_KEYMAP, keymap_bytes(km));
    *p++ = km->layers;
    *p++ = km->rows;
    *p++ = km->cols;
    size_t n = kb_keymap_size(km->layers, km->rows, km->cols);
    for (size_t i = 0; i < n; i++) {
        *p++ = (uint8_t)km->keys[i];
        *p++ = (uint8_t)(km->keys[i] >> 8);
    }

    p = section(p, KB_SECTION_SETTINGS, SETTINGS_V1_SIZE);
    *p++ = s->debounce_algo;
    *p++ = s->debounce_ms;
    *p++ = (uint8_t)s->tapping_term_ms;
    *p++ = (uint8_t)(s->tapping_term_ms >> 8);
    *p++ = s->nkro ? 1u : 0u;
    *p++ = s->sleep_timeout_min;
    *p++ = s->indicator_brightness;
    memset(p, 0, KB_SETTINGS_NAME_MAX);
    for (size_t i = 0; i < KB_SETTINGS_NAME_MAX && s->name[i] != '\0'; i++) {
        p[i] = (uint8_t)s->name[i];
    }
    p += KB_SETTINGS_NAME_MAX;

    return (size_t)(p - buf);
}

/* Settings section: each field present and valid replaces its default. */
static void decode_settings(const uint8_t *b, size_t len, struct kb_settings *s)
{
    struct kb_settings next = *s;

    if (len >= 1 && b[0] < KB_DEBOUNCE_ALGO_COUNT) {
        next.debounce_algo = b[0];
    }
    if (len >= 2 && b[1] <= KB_DEBOUNCE_MAX_MS) {
        next.debounce_ms = b[1];
    }
    if (len >= 4) {
        uint16_t t = (uint16_t)(b[2] | (b[3] << 8));
        if (t >= KB_SETTINGS_TAPPING_TERM_MIN && t <= KB_SETTINGS_TAPPING_TERM_MAX) {
            next.tapping_term_ms = t;
        }
    }
    if (len >= 5 && b[4] <= 1u) {
        next.nkro = b[4] != 0;
    }
    if (len >= 6 && b[5] <= KB_SETTINGS_SLEEP_TIMEOUT_MAX) {
        next.sleep_timeout_min = b[5];
    }
    if (len >= 7 && b[6] <= KB_SETTINGS_BRIGHTNESS_MAX) {
        next.indicator_brightness = b[6];
    }
    if (len >= SETTINGS_V1_SIZE) {
        memset(next.name, 0, sizeof(next.name));
        for (size_t i = 0; i < KB_SETTINGS_NAME_MAX && b[7 + i] != 0; i++) {
            next.name[i] = (char)b[7 + i];
        }
    }
    *s = next;
}

/* Keymap section: only for the same geometry. Returns true if loaded. */
static bool decode_keymap(const uint8_t *b, size_t len, struct kb_keymap *km)
{
    if (len < KEYMAP_HEADER || b[0] != km->layers || b[1] != km->rows || b[2] != km->cols ||
        len != keymap_bytes(km)) {
        return false;
    }
    size_t n = kb_keymap_size(km->layers, km->rows, km->cols);
    for (size_t i = 0; i < n; i++) {
        kb_keycode_t kc = (kb_keycode_t)(b[KEYMAP_HEADER + 2 * i] | (b[KEYMAP_HEADER + 2 * i + 1] << 8));
        km->keys[i] = kb_keycode_is_valid(kc, km->layers) ? kc : KC_NO;
    }
    return true;
}

/* Schema 1. Checks the section framing before changing anything. */
static int decode_v1(const uint8_t *buf, size_t len, struct kb_keymap *km, struct kb_settings *s)
{
    for (size_t off = 0; off < len;) {
        if (len - off < SECTION_HEADER) {
            return -1;
        }
        size_t n = (size_t)(buf[off + 1] | (buf[off + 2] << 8));
        if (n > len - off - SECTION_HEADER) {
            return -1;
        }
        off += SECTION_HEADER + n;
    }

    int loaded = 0;
    for (size_t off = 0; off < len;) {
        uint8_t tag = buf[off];
        size_t n = (size_t)(buf[off + 1] | (buf[off + 2] << 8));
        const uint8_t *body = &buf[off + SECTION_HEADER];
        if (tag == KB_SECTION_KEYMAP && decode_keymap(body, n, km)) {
            loaded |= KB_LOADED_KEYMAP;
        } else if (tag == KB_SECTION_SETTINGS) {
            decode_settings(body, n, s);
            loaded |= KB_LOADED_SETTINGS;
        }
        off += SECTION_HEADER + n;
    }
    return loaded;
}

int kb_settings_decode(uint16_t schema, const uint8_t *buf, size_t len, struct kb_keymap *km,
                       struct kb_settings *s)
{
    switch (schema) {
    case 1:
        return decode_v1(buf, len, km, s);
    /*
     * A future schema 2 adds "case 1:" migration here: decode the old
     * layout into km and s, and the next save writes schema 2.
     */
    default:
        return -1;
    }
}
