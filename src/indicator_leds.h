#ifndef INDICATOR_LEDS_H
#define INDICATOR_LEDS_H

#include <stdbool.h>


/*
 * Configures the three indicator LED pins as outputs
 * and initially turns all LEDs off.
 */
void indicator_leds_init(void);


/*
 * Controls the three indicator LEDs independently.
 *
 * true  = LED on
 * false = LED off
 */
void indicator_leds_set(
    bool low_led_on,
    bool medium_led_on,
    bool high_led_on
);


/*
 * Turns all indicator LEDs off.
 */
void indicator_leds_off(void);


#endif