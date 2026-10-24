/*
 * Firmware version helpers, shared by USB descriptors and the protocol
 * handshake, and the firmware info block an updater reads from an image.
 */
#ifndef KB_VERSION_H
#define KB_VERSION_H

#include <stdint.h>

/*
 * USB bcdDevice for a semver version: 0xJJMN, with JJ the major version in
 * two BCD digits, M the minor and N the patch in one BCD digit each. Parts
 * too large for their digits are clamped to 99, 9 and 9.
 */
uint16_t kb_version_bcd_device(uint32_t major, uint32_t minor, uint32_t patch);

/*
 * Firmware info block: somewhere in every firmware image, so Doghouse can
 * tell what a .uf2 file holds before flashing it (host-lib's uf2.ts finds
 * it by its magic). Bytes only, so the layout has no padding. Fields are
 * only ever appended; layout counts the versions.
 */
#define KB_FW_INFO_MAGIC "KBFWINFO" /* 8 bytes, without its NUL */
#define KB_FW_INFO_LAYOUT 1u
#define KB_FW_INFO_MODEL_MAX 16u

struct kb_fw_info {
    char magic[8];
    uint8_t layout;
    uint8_t protocol_version;
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint8_t reserved[3];
    char model[KB_FW_INFO_MODEL_MAX]; /* keyboard definition name, NUL-padded */
};

#define KB_FW_INFO_SIZE 32u

_Static_assert(sizeof(struct kb_fw_info) == KB_FW_INFO_SIZE, "kb_fw_info must have no padding");

#endif /* KB_VERSION_H */
