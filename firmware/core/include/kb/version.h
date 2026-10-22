/*
 * Firmware version helpers, shared by USB descriptors and (later) the
 * protocol handshake.
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

#endif /* KB_VERSION_H */
