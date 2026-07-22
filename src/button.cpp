#include <Arduino.h>

#include "button.h"


void button_init(
    button_t *button,
    uint8_t pin,
    uint32_t debounce_ms
)
{
    if (button == nullptr)
    {
        return;
    }

    pinMode(pin, INPUT_PULLUP);

    const bool initial_state = digitalRead(pin);

    button->pin = pin;

    button->last_raw_state = initial_state;
    button->stable_state = initial_state;

    button->last_change_ms = millis();
    button->debounce_ms = debounce_ms;
}


bool button_was_pressed(button_t *button)
{
    if (button == nullptr)
    {
        return false;
    }

    const uint32_t now = millis();

    const bool raw_state =
        digitalRead(button->pin);

    /*
     * A raw-state transition restarts the debounce timer.
     */
    if (raw_state != button->last_raw_state)
    {
        button->last_raw_state = raw_state;
        button->last_change_ms = now;
    }

    /*
     * Accept the new state only after it has remained
     * stable for the configured debounce period.
     */
    if ((now - button->last_change_ms) >=
        button->debounce_ms)
    {
        if (raw_state != button->stable_state)
        {
            button->stable_state = raw_state;

            /*
             * INPUT_PULLUP:
             *
             * HIGH = released
             * LOW  = pressed
             */
            if (button->stable_state == LOW)
            {
                return true;
            }
        }
    }

    return false;
}