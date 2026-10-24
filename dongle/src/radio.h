/*
 * The 2.4 GHz link, dongle end: Enhanced ShockBurst as PRX on the
 * dongle's own address (from its device ID, kb/link.h). Reports from the
 * keyboard go straight to USB; the host's lock LEDs and state, and config
 * requests from Doghouse, ride back in acknowledgements.
 *
 * Pairing (the button): for 30 s the dongle also listens on the fixed
 * pairing address and offers its address to the first keyboard that asks.
 */
#ifndef RADIO_H
#define RADIO_H

#include <stdbool.h>

int radio_init(void);

/* The USB host's lock LEDs or state changed: tell the keyboard. Safe from any thread. */
void radio_status_changed(void);

/* Start pairing for 30 s, or stop it if it is on. */
void radio_toggle_pairing(void);

#endif /* RADIO_H */
