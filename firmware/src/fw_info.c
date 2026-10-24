/*
 * This firmware's info block (kb/version.h), read by Doghouse from the .uf2
 * file before an update. The handshake reports the same version and model.
 */
#include "fw_info.h"

#include <zephyr/sys/util.h>

#include <app_version.h>

#include "kb/protocol_gen.h"

BUILD_ASSERT(sizeof(CONFIG_KB_MODEL) - 1 <= KB_FW_INFO_MODEL_MAX, "CONFIG_KB_MODEL is too long");

const struct kb_fw_info fw_info = {
    .magic = {'K', 'B', 'F', 'W', 'I', 'N', 'F', 'O'},
    .layout = KB_FW_INFO_LAYOUT,
    .protocol_version = KB_PROTO_VERSION,
    .major = APP_VERSION_MAJOR,
    .minor = APP_VERSION_MINOR,
    .patch = APP_PATCHLEVEL,
    .model = CONFIG_KB_MODEL,
};
