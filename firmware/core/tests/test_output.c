#include "kb/output.h"
#include "kb_test.h"

static enum kb_output pick_all(uint8_t preferred, bool usb, uint8_t ble, bool dongle)
{
    struct kb_output_status s = {
        .preferred = preferred, .usb_ready = usb, .ble = ble, .dongle_ready = dongle};
    return kb_output_select(&s);
}

static enum kb_output pick(uint8_t preferred, bool usb, uint8_t ble)
{
    return pick_all(preferred, usb, ble, false);
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

static void test_prefers_dongle(void)
{
    /* OUT_2G4: the dongle while it is in range and its host is ready. */
    KB_CHECK_EQ(pick_all(KB_OUTPUT_DONGLE, true, KB_BLE_SECURE, true), KB_OUTPUT_DONGLE);
    /* Out of range: the cable, then Bluetooth. */
    KB_CHECK_EQ(pick_all(KB_OUTPUT_DONGLE, true, KB_BLE_SECURE, false), KB_OUTPUT_USB);
    KB_CHECK_EQ(pick_all(KB_OUTPUT_DONGLE, false, KB_BLE_SECURE, false), KB_OUTPUT_BLE);
}

static void test_fallback_order(void)
{
    /* The preferred output gone: USB, then the dongle, then Bluetooth. */
    KB_CHECK_EQ(pick_all(KB_OUTPUT_BLE, false, KB_BLE_ADVERTISING, true), KB_OUTPUT_DONGLE);
    KB_CHECK_EQ(pick_all(KB_OUTPUT_BLE, true, KB_BLE_ADVERTISING, true), KB_OUTPUT_USB);
    KB_CHECK_EQ(pick_all(KB_OUTPUT_USB, false, KB_BLE_SECURE, true), KB_OUTPUT_DONGLE);
    KB_CHECK_EQ(pick_all(KB_OUTPUT_USB, false, KB_BLE_OFF, false), KB_OUTPUT_NONE);
}

static void test_bad_preference(void)
{
    /* Nothing else is valid: treated as no preference, so USB first. */
    KB_CHECK(!kb_output_preferable(KB_OUTPUT_NONE));
    KB_CHECK(!kb_output_preferable(4));
    KB_CHECK(!kb_output_preferable(200));
    KB_CHECK(kb_output_preferable(KB_OUTPUT_USB));
    KB_CHECK(kb_output_preferable(KB_OUTPUT_BLE));
    KB_CHECK(kb_output_preferable(KB_OUTPUT_DONGLE));
    KB_CHECK_EQ(pick(4, true, KB_BLE_SECURE), KB_OUTPUT_USB);
    KB_CHECK_EQ(pick(0xFF, false, KB_BLE_SECURE), KB_OUTPUT_BLE);
}

int main(void)
{
    KB_RUN(test_prefers_usb);
    KB_RUN(test_prefers_ble);
    KB_RUN(test_ble_needs_encryption);
    KB_RUN(test_nothing_ready);
    KB_RUN(test_prefers_dongle);
    KB_RUN(test_fallback_order);
    KB_RUN(test_bad_preference);
    return KB_TEST_RESULT();
}
