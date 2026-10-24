#include "kb/output.h"

#include "kb/screen.h"

_Static_assert((int)KB_OUTPUT_USB == (int)KB_TRANSPORT_USB &&
                   (int)KB_OUTPUT_BLE == (int)KB_TRANSPORT_BLE &&
                   (int)KB_OUTPUT_DONGLE == (int)KB_TRANSPORT_DONGLE,
               "outputs and screen transports share their values");

bool kb_output_preferable(uint8_t output)
{
    return output == KB_OUTPUT_USB || output == KB_OUTPUT_BLE || output == KB_OUTPUT_DONGLE;
}

static bool ready(const struct kb_output_status *s, uint8_t output)
{
    switch (output) {
    case KB_OUTPUT_USB:
        return s->usb_ready;
    case KB_OUTPUT_BLE:
        return s->ble == KB_BLE_SECURE;
    case KB_OUTPUT_DONGLE:
        return s->dongle_ready;
    default:
        return false;
    }
}

enum kb_output kb_output_select(const struct kb_output_status *s)
{
    /* The cable first: plugging it in is the clearest sign of where keys should go. */
    static const uint8_t fallback[] = {KB_OUTPUT_USB, KB_OUTPUT_DONGLE, KB_OUTPUT_BLE};

    if (kb_output_preferable(s->preferred) && ready(s, s->preferred)) {
        return (enum kb_output)s->preferred;
    }
    for (unsigned i = 0; i < sizeof(fallback); i++) {
        if (ready(s, fallback[i])) {
            return (enum kb_output)fallback[i];
        }
    }
    return KB_OUTPUT_NONE;
}
