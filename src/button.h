#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>


typedef struct
{
    uint8_t pin;

    bool last_raw_state;
    bool stable_state;

    uint32_t last_change_ms;
    uint32_t debounce_ms;

    /*
     * Time at which the current debounced press began.
     */
    uint32_t pressed_since_ms;

    /*
     * Prevents the same long press from being reported
     * repeatedly while the button remains held.
     */
    bool hold_event_reported;

} button_t;


/*
 * Configures the button pin using INPUT_PULLUP
 * and initializes its internal state.
 */
void button_init(
    button_t *button,
    uint8_t pin,
    uint32_t debounce_ms
);


/*
 * Returns true once when a valid press is detected.
 *
 * INPUT_PULLUP:
 *
 * HIGH = released
 * LOW  = pressed
 */
bool button_was_pressed(button_t *button);


/*
 * Returns true once when the button has remained
 * pressed for at least hold_ms.
 *
 * It will not return true again until the button
 * has been released and pressed again.
 */
bool button_was_held(
    button_t *button,
    uint32_t hold_ms
);


#endif