#include "kb/output.h"
#include "kb_test.h"

static enum kb_output pick(uint8_t preferred, bool usb, uint8_t ble)
{
    struct kb_output_status s = {.preferred = preferred, .usb_ready = usb, .ble = ble};
    return kb_output_select(&s);
}

static void test_prefers_usb(void)
{
    /* Plugged in: USB. Unplugged: carries on over Bluetooth. */
    KB_CHECK_EQ(pick(KB_OUTPUT_USB, true, KB_BLE_SECURE), KB_OUTPUT_USB);
    KB_CHECK_EQ(pick(KB_OUTPUT_USB, false, KB_BLE_SECURE), KB_OUTPUT_BLE);
    KB_CHECK_EQ(pick(KB_OUTPUT_USB, true, KB_BLE_OFF), KB_OUTPUT_USB);
}

static void test_prefers_ble(void)
{
    /* OUT_BLE while charging from the computer: keys go over Bluetooth. */
    KB_CHECK_EQ(pick(KB_OUTPUT_BLE, true, KB_BLE_SECURE), KB_OUTPUT_BLE);
    /* Until the Bluetooth host goes away. */
    KB_CHECK_EQ(pick(KB_OUTPUT_BLE, true, KB_BLE_ADVERTISING), KB_OUTPUT_USB);
}

static void test_ble_needs_encryption(void)
{
    /* Connected but not yet paired or encrypted: no keys go out. */
    KB_CHECK_EQ(pick(KB_OUTPUT_BLE, false, KB_BLE_CONNECTED), KB_OUTPUT_NONE);
    KB_CHECK_EQ(pick(KB_OUTPUT_USB, false, KB_BLE_CONNECTED), KB_OUTPUT_NONE);
    KB_CHECK_EQ(pick(KB_OUTPUT_BLE, true, KB_BLE_CONNECTED), KB_OUTPUT_USB);
}

static void test_nothing_ready(void)
{
    KB_CHECK_EQ(pick(KB_OUTPUT_USB, false, KB_BLE_OFF), KB_OUTPUT_NONE);
    KB_CHECK_EQ(pick(KB_OUTPUT_BLE, false, KB_BLE_ADVERTISING), KB_OUTPUT_NONE);
}

static void test_bad_preference(void)
{
    /* The dongle isn't built yet, and nothing else is valid: treated as USB first. */
    KB_CHECK(!kb_output_preferable(KB_OUTPUT_NONE));
    KB_CHECK(!kb_output_preferable(KB_OUTPUT_DONGLE));
    KB_CHECK(!kb_output_preferable(200));
    KB_CHECK(kb_output_preferable(KB_OUTPUT_USB));
    KB_CHECK(kb_output_preferable(KB_OUTPUT_BLE));
    KB_CHECK_EQ(pick(KB_OUTPUT_DONGLE, true, KB_BLE_SECURE), KB_OUTPUT_USB);
    KB_CHECK_EQ(pick(0xFF, false, KB_BLE_SECURE), KB_OUTPUT_BLE);
}

int main(void)
{
    KB_RUN(test_prefers_usb);
    KB_RUN(test_prefers_ble);
    KB_RUN(test_ble_needs_encryption);
    KB_RUN(test_nothing_ready);
    KB_RUN(test_bad_preference);
    return KB_TEST_RESULT();
}
