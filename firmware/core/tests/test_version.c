#include <stddef.h>
#include <string.h>

#include "kb/version.h"
#include "kb_test.h"

static void test_bcd_layout(void)
{
    KB_CHECK_EQ(kb_version_bcd_device(0, 1, 0), 0x0010);
    KB_CHECK_EQ(kb_version_bcd_device(1, 2, 3), 0x0123);
    KB_CHECK_EQ(kb_version_bcd_device(12, 3, 4), 0x1234);
    KB_CHECK_EQ(kb_version_bcd_device(0, 0, 0), 0x0000);
}

static void test_bcd_clamps(void)
{
    KB_CHECK_EQ(kb_version_bcd_device(0, 12, 0), 0x0090);
    KB_CHECK_EQ(kb_version_bcd_device(0, 0, 15), 0x0009);
    KB_CHECK_EQ(kb_version_bcd_device(150, 0, 0), 0x9900);
}

static void test_fw_info_layout(void)
{
    /* host-lib's uf2.ts reads these offsets. */
    KB_CHECK_EQ(strlen(KB_FW_INFO_MAGIC), 8);
    KB_CHECK_EQ(offsetof(struct kb_fw_info, layout), 8);
    KB_CHECK_EQ(offsetof(struct kb_fw_info, protocol_version), 9);
    KB_CHECK_EQ(offsetof(struct kb_fw_info, major), 10);
    KB_CHECK_EQ(offsetof(struct kb_fw_info, minor), 11);
    KB_CHECK_EQ(offsetof(struct kb_fw_info, patch), 12);
    KB_CHECK_EQ(offsetof(struct kb_fw_info, model), 16);
    KB_CHECK_EQ(sizeof(struct kb_fw_info), KB_FW_INFO_SIZE);
}

int main(void)
{
    printf("test_version\n");
    KB_RUN(test_bcd_layout);
    KB_RUN(test_bcd_clamps);
    KB_RUN(test_fw_info_layout);
    return KB_TEST_RESULT();
}
