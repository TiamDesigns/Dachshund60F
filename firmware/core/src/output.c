#include "kb/output.h"

#include "kb/screen.h"

_Static_assert((int)KB_OUTPUT_USB == (int)KB_TRANSPORT_USB &&
                   (int)KB_OUTPUT_BLE == (int)KB_TRANSPORT_BLE &&
                   (int)KB_OUTPUT_DONGLE == (int)KB_TRANSPORT_DONGLE,
               "outputs and screen transports share their values");

bool kb_output_preferable(uint8_t output)
{
    return output == KB_OUTPUT_USB || output == KB_OUTPUT_BLE;
}

static bool ready(const struct kb_output_status *s, uint8_t output)
{
    switch (output) {
    case KB_OUTPUT_USB:
        return s->usb_ready;
    case KB_OUTPUT_BLE:
        return s->ble == KB_BLE_SECURE;
    default:
        return false;
    }
}

enum kb_output kb_output_select(const struct kb_output_status *s)
{
    uint8_t preferred = kb_output_preferable(s->preferred) ? s->preferred : KB_OUTPUT_USB;
    uint8_t other = preferred == KB_OUTPUT_USB ? KB_OUTPUT_BLE : KB_OUTPUT_USB;

    if (ready(s, preferred)) {
        return (enum kb_output)preferred;
    }
    if (ready(s, other)) {
        return (enum kb_output)other;
    }
    return KB_OUTPUT_NONE;
}
