#include "kb/protocol.h"

#include <string.h>

#include "kb/keycodes.h"
#include "kb/output.h"

/* Response payload goes here; returns its length and sets *status. */
typedef size_t (*handler_fn)(const struct kb_protocol *p, const uint8_t *in, size_t len,
                             uint8_t *out, uint8_t *status);

static void lock(const struct kb_protocol *p)
{
    if (p->ops && p->ops->lock) {
        p->ops->lock(p->ctx);
    }
}

static void unlock(const struct kb_protocol *p)
{
    if (p->ops && p->ops->unlock) {
        p->ops->unlock(p->ctx);
    }
}

static void copy_text(char *dst, size_t size, const char *src)
{
    memset(dst, 0, size);
    if (src) {
        size_t n = 0;
        while (n + 1 < size && src[n] != '\0') {
            dst[n] = src[n];
            n++;
        }
    }
}

static size_t keymap_total(const struct kb_keymap *km)
{
    return kb_keymap_size(km->layers, km->rows, km->cols);
}

static size_t h_handshake(const struct kb_protocol *p, const uint8_t *in, size_t len,
                          uint8_t *out, uint8_t *status)
{
    (void)in;
    if (len != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }

    struct kb_proto_handshake_resp r;
    memset(&r, 0, sizeof(r));
    r.protocol_version = KB_PROTO_VERSION;
    r.firmware_major = p->firmware_major;
    r.firmware_minor = p->firmware_minor;
    r.firmware_patch = p->firmware_patch;
    copy_text(r.build, sizeof(r.build), p->build);
    r.capabilities = p->capabilities;
    r.rows = p->keymap->rows;
    r.cols = p->keymap->cols;
    r.layers = p->keymap->layers;
    r.encoders = p->encoders;
    r.macro_buffer_size = p->macro_buffer_size;
    copy_text(r.model, sizeof(r.model), p->model);
    return kb_proto_handshake_resp_encode(&r, out);
}

static size_t h_reboot(const struct kb_protocol *p, const uint8_t *in, size_t len, uint8_t *out,
                       uint8_t *status)
{
    (void)out;
    struct kb_proto_reboot_req r;
    if (kb_proto_reboot_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
    } else if (r.mode > 1) {
        *status = KB_PROTO_BAD_VALUE;
    } else if (!p->ops || !p->ops->reboot) {
        *status = KB_PROTO_NOT_SUPPORTED;
    } else {
        p->ops->reboot(r.mode == 1, p->ctx);
    }
    return 0;
}

static size_t h_identify(const struct kb_protocol *p, const uint8_t *in, size_t len,
                         uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_identify_req r;
    if (kb_proto_identify_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
    } else if (r.seconds < 1 || r.seconds > 30) {
        *status = KB_PROTO_BAD_VALUE;
    } else if (!p->ops || !p->ops->identify) {
        *status = KB_PROTO_NOT_SUPPORTED;
    } else {
        p->ops->identify(r.seconds, p->ctx);
    }
    return 0;
}

static size_t h_get_serial(const struct kb_protocol *p, const uint8_t *in, size_t len,
                           uint8_t *out, uint8_t *status)
{
    (void)in;
    if (len != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    struct kb_proto_get_serial_resp r;
    copy_text(r.serial, sizeof(r.serial), p->serial);
    return kb_proto_get_serial_resp_encode(&r, out);
}

static bool key_in_range(const struct kb_keymap *km, uint8_t layer, uint8_t row, uint8_t col)
{
    return layer < km->layers && row < km->rows && col < km->cols;
}

static size_t h_get_key(const struct kb_protocol *p, const uint8_t *in, size_t len,
                        uint8_t *out, uint8_t *status)
{
    struct kb_proto_get_key_req r;
    if (kb_proto_get_key_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (!key_in_range(p->keymap, r.layer, r.row, r.col)) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }

    struct kb_proto_get_key_resp resp;
    lock(p);
    resp.keycode = kb_keymap_get(p->keymap, r.layer, r.row, r.col);
    unlock(p);
    return kb_proto_get_key_resp_encode(&resp, out);
}

static size_t h_set_key(const struct kb_protocol *p, const uint8_t *in, size_t len,
                        uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_set_key_req r;
    if (kb_proto_set_key_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (!key_in_range(p->keymap, r.layer, r.row, r.col)) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }
    if (!kb_keycode_is_valid(r.keycode, p->keymap->layers)) {
        *status = KB_PROTO_BAD_VALUE;
        return 0;
    }

    lock(p);
    int err = kb_keymap_set(p->keymap, r.layer, r.row, r.col, r.keycode);
    unlock(p);
    if (err) {
        *status = KB_PROTO_BAD_VALUE;
    }
    return 0;
}

static size_t h_get_keymap_chunk(const struct kb_protocol *p, const uint8_t *in, size_t len,
                                 uint8_t *out, uint8_t *status)
{
    struct kb_proto_get_keymap_chunk_req r;
    if (kb_proto_get_keymap_chunk_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (r.count > KB_PROTO_GET_KEYMAP_CHUNK_RESP_MAX_KEYCODES ||
        (size_t)r.offset + r.count > keymap_total(p->keymap)) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }

    struct kb_proto_get_keymap_chunk_resp resp;
    resp.offset = r.offset;
    resp.keycodes_count = r.count;
    lock(p);
    memcpy(resp.keycodes, &p->keymap->keys[r.offset], r.count * sizeof(kb_keycode_t));
    unlock(p);
    return kb_proto_get_keymap_chunk_resp_encode(&resp, out);
}

static size_t h_set_keymap_chunk(const struct kb_protocol *p, const uint8_t *in, size_t len,
                                 uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_set_keymap_chunk_req r;
    if (kb_proto_set_keymap_chunk_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if ((size_t)r.offset + r.keycodes_count > keymap_total(p->keymap)) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }
    /* All or nothing: check every keycode before writing any. */
    for (size_t i = 0; i < r.keycodes_count; i++) {
        if (!kb_keycode_is_valid(r.keycodes[i], p->keymap->layers)) {
            *status = KB_PROTO_BAD_VALUE;
            return 0;
        }
    }

    lock(p);
    memcpy(&p->keymap->keys[r.offset], r.keycodes, r.keycodes_count * sizeof(kb_keycode_t));
    unlock(p);
    return 0;
}

static size_t run_op(const struct kb_protocol *p, size_t len, uint8_t *status,
                     uint8_t (*op)(void *ctx))
{
    if (len != 0) {
        *status = KB_PROTO_BAD_LENGTH;
    } else if (!op) {
        *status = KB_PROTO_NOT_SUPPORTED;
    } else {
        *status = op(p->ctx);
    }
    return 0;
}

static size_t h_save(const struct kb_protocol *p, const uint8_t *in, size_t len, uint8_t *out,
                     uint8_t *status)
{
    (void)in;
    (void)out;
    return run_op(p, len, status, p->ops ? p->ops->save : NULL);
}

/* The live macros changed: start editing from them again. */
static void refresh_macro_edit(const struct kb_protocol *p)
{
    if (p->features && p->macro_edit) {
        lock(p);
        memcpy(p->macro_edit, p->features->macros, KB_MACRO_BUFFER);
        unlock(p);
    }
}

static size_t h_revert(const struct kb_protocol *p, const uint8_t *in, size_t len,
                       uint8_t *out, uint8_t *status)
{
    (void)in;
    (void)out;
    run_op(p, len, status, p->ops ? p->ops->revert : NULL);
    if (*status == KB_PROTO_OK) {
        refresh_macro_edit(p);
    }
    return 0;
}

static size_t h_factory_reset(const struct kb_protocol *p, const uint8_t *in, size_t len,
                              uint8_t *out, uint8_t *status)
{
    (void)in;
    (void)out;
    run_op(p, len, status, p->ops ? p->ops->factory_reset : NULL);
    if (*status == KB_PROTO_OK) {
        refresh_macro_edit(p);
    }
    return 0;
}

static void features_changed(const struct kb_protocol *p)
{
    if (p->ops && p->ops->features_changed) {
        p->ops->features_changed(p->ctx);
    }
}

static size_t h_get_feature(const struct kb_protocol *p, const uint8_t *in, size_t len,
                            uint8_t *out, uint8_t *status)
{
    struct kb_proto_get_feature_req r;
    if (!p->features) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (kb_proto_get_feature_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (r.index >= kb_feature_entries(r.table, p->keymap->layers)) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }

    struct kb_proto_get_feature_resp resp;
    resp.table = r.table;
    resp.index = r.index;
    resp.data_count = kb_feature_entry_size(r.table);
    lock(p);
    kb_feature_get(p->features, r.table, r.index, resp.data);
    unlock(p);
    return kb_proto_get_feature_resp_encode(&resp, out);
}

static size_t h_set_feature(const struct kb_protocol *p, const uint8_t *in, size_t len,
                            uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_set_feature_req r;
    if (!p->features) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (kb_proto_set_feature_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (r.index >= kb_feature_entries(r.table, p->keymap->layers)) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }
    if (r.data_count == 0 || r.data_count > kb_feature_entry_size(r.table)) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }

    /* Write the entry, check the whole set, and put the old entry back on failure. */
    uint8_t old[KB_PROTO_SET_FEATURE_REQ_MAX_DATA];
    lock(p);
    kb_feature_get(p->features, r.table, r.index, old);
    kb_feature_put(p->features, r.table, r.index, r.data, r.data_count);
    if (kb_features_valid(p->features, p->keymap->layers, p->keymap->rows, p->keymap->cols)) {
        features_changed(p);
    } else {
        kb_feature_put(p->features, r.table, r.index, old, kb_feature_entry_size(r.table));
        *status = KB_PROTO_BAD_VALUE;
    }
    unlock(p);
    return 0;
}

static size_t h_get_macro_chunk(const struct kb_protocol *p, const uint8_t *in, size_t len,
                                uint8_t *out, uint8_t *status)
{
    struct kb_proto_get_macro_chunk_req r;
    if (!p->features || !p->macro_edit) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (kb_proto_get_macro_chunk_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (r.count > KB_PROTO_GET_MACRO_CHUNK_RESP_MAX_DATA ||
        (size_t)r.offset + r.count > KB_MACRO_BUFFER) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }

    struct kb_proto_get_macro_chunk_resp resp;
    resp.offset = r.offset;
    resp.data_count = r.count;
    lock(p);
    memcpy(resp.data, &p->macro_edit[r.offset], r.count);
    unlock(p);
    return kb_proto_get_macro_chunk_resp_encode(&resp, out);
}

static size_t h_set_macro_chunk(const struct kb_protocol *p, const uint8_t *in, size_t len,
                                uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_set_macro_chunk_req r;
    if (!p->features || !p->macro_edit) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (kb_proto_set_macro_chunk_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if ((size_t)r.offset + r.data_count > KB_MACRO_BUFFER) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }
    lock(p);
    memcpy(&p->macro_edit[r.offset], r.data, r.data_count);
    unlock(p);
    return 0;
}

static size_t h_apply_macros(const struct kb_protocol *p, const uint8_t *in, size_t len,
                             uint8_t *out, uint8_t *status)
{
    (void)in;
    (void)out;
    if (!p->features || !p->macro_edit) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (len != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    lock(p);
    if (kb_macros_valid(p->macro_edit, KB_MACRO_BUFFER, p->keymap->layers)) {
        memcpy(p->features->macros, p->macro_edit, KB_MACRO_BUFFER);
        features_changed(p);
    } else {
        /* Refused: the edit is dropped, so reads show the live macros again. */
        memcpy(p->macro_edit, p->features->macros, KB_MACRO_BUFFER);
        *status = KB_PROTO_BAD_VALUE;
    }
    unlock(p);
    return 0;
}

static void settings_changed(const struct kb_protocol *p)
{
    if (p->ops && p->ops->settings_changed) {
        p->ops->settings_changed(p->ctx);
    }
}

static size_t h_get_settings(const struct kb_protocol *p, const uint8_t *in, size_t len,
                             uint8_t *out, uint8_t *status)
{
    (void)in;
    if (!p->settings) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (len != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }

    struct kb_proto_get_settings_resp r;
    memset(&r, 0, sizeof(r));
    lock(p);
    r.debounce_algorithm = p->settings->debounce_algo;
    r.debounce_ms = p->settings->debounce_ms;
    r.tapping_term_ms = p->settings->tapping_term_ms;
    r.nkro = p->settings->nkro ? 1u : 0u;
    r.sleep_timeout_min = p->settings->sleep_timeout_min;
    r.indicator_brightness = p->settings->indicator_brightness;
    copy_text(r.name, sizeof(r.name), p->settings->name);
    unlock(p);
    return kb_proto_get_settings_resp_encode(&r, out);
}

static size_t h_set_settings(const struct kb_protocol *p, const uint8_t *in, size_t len,
                             uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_set_settings_req r;
    if (!p->settings) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (kb_proto_set_settings_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }

    struct kb_settings next;
    memset(&next, 0, sizeof(next));
    next.debounce_algo = r.debounce_algorithm;
    next.debounce_ms = r.debounce_ms;
    next.tapping_term_ms = r.tapping_term_ms;
    next.nkro = r.nkro != 0;
    next.sleep_timeout_min = r.sleep_timeout_min;
    next.indicator_brightness = r.indicator_brightness;
    copy_text(next.name, sizeof(next.name), r.name);
    if (r.nkro > 1 || !kb_settings_valid(&next)) {
        *status = KB_PROTO_BAD_VALUE;
        return 0;
    }

    lock(p);
    *p->settings = next;
    settings_changed(p);
    unlock(p);
    return 0;
}

static uint32_t rgb_to_u32(struct kb_rgb c)
{
    return (uint32_t)c.r << 16 | (uint32_t)c.g << 8 | c.b;
}

static struct kb_rgb u32_to_rgb(uint32_t v)
{
    return (struct kb_rgb){(uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
}

static size_t h_get_indicators(const struct kb_protocol *p, const uint8_t *in, size_t len,
                               uint8_t *out, uint8_t *status)
{
    (void)in;
    if (!p->indicators) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (len != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }

    struct kb_proto_get_indicators_resp r;
    lock(p);
    r.caps = rgb_to_u32(p->indicators->caps);
    r.num = rgb_to_u32(p->indicators->num);
    r.scroll = rgb_to_u32(p->indicators->scroll);
    r.battery = rgb_to_u32(p->indicators->battery);
    r.battery_low = rgb_to_u32(p->indicators->battery_low);
    r.battery_low_percent = p->indicators->battery_low_pct;
    unlock(p);
    return kb_proto_get_indicators_resp_encode(&r, out);
}

static size_t h_set_indicators(const struct kb_protocol *p, const uint8_t *in, size_t len,
                               uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_set_indicators_req r;
    if (!p->indicators) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (kb_proto_set_indicators_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (((r.caps | r.num | r.scroll | r.battery | r.battery_low) >> 24) != 0 ||
        r.battery_low_percent > 100) {
        *status = KB_PROTO_BAD_VALUE;
        return 0;
    }

    lock(p);
    p->indicators->caps = u32_to_rgb(r.caps);
    p->indicators->num = u32_to_rgb(r.num);
    p->indicators->scroll = u32_to_rgb(r.scroll);
    p->indicators->battery = u32_to_rgb(r.battery);
    p->indicators->battery_low = u32_to_rgb(r.battery_low);
    p->indicators->battery_low_pct = r.battery_low_percent;
    settings_changed(p);
    unlock(p);
    return 0;
}

static size_t h_get_layer_colour(const struct kb_protocol *p, const uint8_t *in, size_t len,
                                 uint8_t *out, uint8_t *status)
{
    struct kb_proto_get_layer_colour_req r;
    if (!p->indicators) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (kb_proto_get_layer_colour_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (r.layer >= p->keymap->layers || r.layer >= KB_MAX_LAYERS) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }

    struct kb_proto_get_layer_colour_resp resp;
    resp.layer = r.layer;
    lock(p);
    resp.colour = rgb_to_u32(p->indicators->layer[r.layer]);
    unlock(p);
    return kb_proto_get_layer_colour_resp_encode(&resp, out);
}

static size_t h_set_layer_colour(const struct kb_protocol *p, const uint8_t *in, size_t len,
                                 uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_set_layer_colour_req r;
    if (!p->indicators) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (kb_proto_set_layer_colour_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (r.layer >= p->keymap->layers || r.layer >= KB_MAX_LAYERS) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }
    if ((r.colour >> 24) != 0) {
        *status = KB_PROTO_BAD_VALUE;
        return 0;
    }

    lock(p);
    p->indicators->layer[r.layer] = u32_to_rgb(r.colour);
    settings_changed(p);
    unlock(p);
    return 0;
}

static bool has_bluetooth(const struct kb_protocol *p)
{
    return p->ops && p->ops->get_connection && p->ops->set_output && p->ops->select_profile &&
           p->ops->clear_profile;
}

static size_t h_get_connection(const struct kb_protocol *p, const uint8_t *in, size_t len,
                               uint8_t *out, uint8_t *status)
{
    (void)in;
    if (!has_bluetooth(p)) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (len != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }

    struct kb_proto_get_connection_resp r;
    memset(&r, 0, sizeof(r));
    p->ops->get_connection(&r, p->ctx);
    r.profile_count = p->ble_profiles;
    return kb_proto_get_connection_resp_encode(&r, out);
}

static size_t h_set_output(const struct kb_protocol *p, const uint8_t *in, size_t len,
                           uint8_t *out, uint8_t *status)
{
    (void)out;
    struct kb_proto_set_output_req r;
    if (!has_bluetooth(p)) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (kb_proto_set_output_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (r.output == KB_OUTPUT_DONGLE) {
        *status = KB_PROTO_NOT_SUPPORTED; /* not in this build (yet) */
        return 0;
    }
    if (!kb_output_preferable(r.output)) {
        *status = KB_PROTO_BAD_VALUE;
        return 0;
    }
    *status = p->ops->set_output(r.output, p->ctx);
    return 0;
}

/* select_profile and clear_profile share a request layout. */
static size_t profile_command(const struct kb_protocol *p, const uint8_t *in, size_t len,
                              uint8_t *status, uint8_t (*op)(uint8_t, void *))
{
    struct kb_proto_select_profile_req r;
    if (!has_bluetooth(p)) {
        *status = KB_PROTO_NOT_SUPPORTED;
        return 0;
    }
    if (kb_proto_select_profile_req_decode(in, len, &r) != 0) {
        *status = KB_PROTO_BAD_LENGTH;
        return 0;
    }
    if (r.profile >= p->ble_profiles) {
        *status = KB_PROTO_OUT_OF_RANGE;
        return 0;
    }
    *status = op(r.profile, p->ctx);
    return 0;
}

static size_t h_select_profile(const struct kb_protocol *p, const uint8_t *in, size_t len,
                               uint8_t *out, uint8_t *status)
{
    (void)out;
    return profile_command(p, in, len, status, has_bluetooth(p) ? p->ops->select_profile : NULL);
}

static size_t h_clear_profile(const struct kb_protocol *p, const uint8_t *in, size_t len,
                              uint8_t *out, uint8_t *status)
{
    (void)out;
    return profile_command(p, in, len, status, has_bluetooth(p) ? p->ops->clear_profile : NULL);
}

static handler_fn handler_for(uint8_t command)
{
    switch (command) {
    case KB_PROTO_CMD_HANDSHAKE: return h_handshake;
    case KB_PROTO_CMD_REBOOT: return h_reboot;
    case KB_PROTO_CMD_IDENTIFY: return h_identify;
    case KB_PROTO_CMD_GET_SERIAL: return h_get_serial;
    case KB_PROTO_CMD_GET_KEY: return h_get_key;
    case KB_PROTO_CMD_SET_KEY: return h_set_key;
    case KB_PROTO_CMD_GET_KEYMAP_CHUNK: return h_get_keymap_chunk;
    case KB_PROTO_CMD_SET_KEYMAP_CHUNK: return h_set_keymap_chunk;
    case KB_PROTO_CMD_SAVE: return h_save;
    case KB_PROTO_CMD_REVERT: return h_revert;
    case KB_PROTO_CMD_FACTORY_RESET: return h_factory_reset;
    case KB_PROTO_CMD_GET_FEATURE: return h_get_feature;
    case KB_PROTO_CMD_SET_FEATURE: return h_set_feature;
    case KB_PROTO_CMD_GET_MACRO_CHUNK: return h_get_macro_chunk;
    case KB_PROTO_CMD_SET_MACRO_CHUNK: return h_set_macro_chunk;
    case KB_PROTO_CMD_APPLY_MACROS: return h_apply_macros;
    case KB_PROTO_CMD_GET_SETTINGS: return h_get_settings;
    case KB_PROTO_CMD_SET_SETTINGS: return h_set_settings;
    case KB_PROTO_CMD_GET_INDICATORS: return h_get_indicators;
    case KB_PROTO_CMD_SET_INDICATORS: return h_set_indicators;
    case KB_PROTO_CMD_GET_LAYER_COLOUR: return h_get_layer_colour;
    case KB_PROTO_CMD_SET_LAYER_COLOUR: return h_set_layer_colour;
    case KB_PROTO_CMD_GET_CONNECTION: return h_get_connection;
    case KB_PROTO_CMD_SET_OUTPUT: return h_set_output;
    case KB_PROTO_CMD_SELECT_PROFILE: return h_select_profile;
    case KB_PROTO_CMD_CLEAR_PROFILE: return h_clear_profile;
    default: return NULL;
    }
}

bool kb_protocol_handle(const struct kb_protocol *p, const uint8_t *req, size_t req_len,
                        uint8_t resp[KB_PROTO_REPORT_SIZE])
{
    memset(resp, 0, KB_PROTO_REPORT_SIZE);
    if (req_len < KB_PROTO_HEADER_SIZE) {
        return false;
    }
    if (req_len > KB_PROTO_REPORT_SIZE) {
        req_len = KB_PROTO_REPORT_SIZE;
    }

    uint8_t command = req[KB_PROTO_OFF_COMMAND];
    uint8_t len = req[KB_PROTO_OFF_LENGTH];
    uint8_t status = KB_PROTO_OK;
    size_t out_len = 0;
    resp[KB_PROTO_OFF_COMMAND] = command;
    resp[KB_PROTO_OFF_SEQUENCE] = req[KB_PROTO_OFF_SEQUENCE];

    handler_fn handler = handler_for(command);
    if (len > KB_PROTO_MAX_PAYLOAD || KB_PROTO_HEADER_SIZE + (size_t)len > req_len) {
        status = KB_PROTO_BAD_LENGTH;
    } else if (command != KB_PROTO_CMD_HANDSHAKE && req[KB_PROTO_OFF_VERSION] != KB_PROTO_VERSION) {
        status = KB_PROTO_VERSION_MISMATCH;
    } else if (!handler) {
        status = KB_PROTO_UNKNOWN_COMMAND;
    } else {
        out_len = handler(p, &req[KB_PROTO_HEADER_SIZE], len, &resp[KB_PROTO_HEADER_SIZE], &status);
    }

    if (status != KB_PROTO_OK) {
        /* Error responses carry no payload. */
        out_len = 0;
        memset(&resp[KB_PROTO_HEADER_SIZE], 0, KB_PROTO_MAX_PAYLOAD);
    }
    resp[KB_PROTO_OFF_LENGTH] = (uint8_t)out_len;
    resp[KB_PROTO_OFF_STATUS] = status;
    return true;
}
