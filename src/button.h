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
} button_t;


/*
 * Configures the button pin using INPUT_PULLUP
 * and initializes the debounce state.
 */
void button_init(
    button_t *button,
    uint8_t pin,
    uint32_t debounce_ms
);


/*
 * Returns true once when a valid press is detected.
 *
 * The button is considered pressed when the pin
 * changes from HIGH to LOW and remains stable for
 * the configured debounce period.
 */
bool button_was_pressed(button_t *button);

#endif