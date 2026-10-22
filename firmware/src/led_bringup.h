/*
 * Phase 2 indicator bring-up: powers the SK6812 chain and walks a colour
 * along it so chain order and colour order can be checked by eye. Replaced by
 * the real indicator logic in phase 10.
 */
#ifndef LED_BRINGUP_H
#define LED_BRINGUP_H

int led_bringup_start(void);

#endif /* LED_BRINGUP_H */
