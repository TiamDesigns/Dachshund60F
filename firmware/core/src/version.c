#include "kb/version.h"

static uint32_t clamp(uint32_t value, uint32_t max)
{
    return value > max ? max : value;
}

uint16_t kb_version_bcd_device(uint32_t major, uint32_t minor, uint32_t patch)
{
    major = clamp(major, 99u);
    minor = clamp(minor, 9u);
    patch = clamp(patch, 9u);

    return (uint16_t)(((major / 10u) << 12) | ((major % 10u) << 8) | (minor << 4) | patch);
}
