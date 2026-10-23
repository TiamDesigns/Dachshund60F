/*
 * The keyboard loop: scans the matrix at a fixed rate, debounces it, runs
 * the keymap engine from firmware/core and hands reports to USB.
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Set up the matrix and keymap and start the scan thread. */
int keyboard_start(void);

#endif /* KEYBOARD_H */
