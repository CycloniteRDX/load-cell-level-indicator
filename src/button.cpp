#include "button.h"

#include "hal_gpio.h"
#include "hal_time.h"


/*
 * Reads the physical input, applies debounce and updates
 * the stable state stored inside the button object.
 *
 * Returns true only when a new debounced press begins.
 */
static bool button_update_state(button_t *button)
{
    if (button == nullptr)
    {
        return false;
    }

    const uint32_t now = hal_time_millis();

    const bool raw_state =
        hal_gpio_read(button->pin);

    /*
     * Every raw transition restarts the debounce timer.
     */
    if (raw_state != button->last_raw_state)
    {
        button->last_raw_state = raw_state;
        button->last_change_ms = now;
    }

    /*
     * The physical input has not remained stable for
     * the complete debounce interval yet.
     */
    if ((now - button->last_change_ms) <
        button->debounce_ms)
    {
        return false;
    }

    /*
     * There is no new stable transition.
     */
    if (raw_state == button->stable_state)
    {
        return false;
    }

    /*
     * Accept the new debounced state.
     */
    button->stable_state = raw_state;

    /*
     * The input uses an internal pull-up:
     *
     * false = LOW  = pressed
     * true  = HIGH = released
     */
    if (!button->stable_state)
    {
        /*
         * A new debounced press has started.
         */
        button->pressed_since_ms = now;
        button->hold_event_reported = false;

        return true;
    }

    /*
     * The button has been released.
     *
     * A future press may generate another hold event.
     */
    button->hold_event_reported = false;

    return false;
}


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

    hal_gpio_configure_input_pullup(pin);

    const bool initial_state =
        hal_gpio_read(pin);

    const uint32_t now =
        hal_time_millis();

    button->pin = pin;

    button->last_raw_state = initial_state;
    button->stable_state = initial_state;

    button->last_change_ms = now;
    button->debounce_ms = debounce_ms;

    button->pressed_since_ms = now;
    button->hold_event_reported = false;
}


bool button_was_pressed(button_t *button)
{
    return button_update_state(button);
}


bool button_was_held(
    button_t *button,
    uint32_t hold_ms
)
{
    if (button == nullptr)
    {
        return false;
    }

    /*
     * Update the debounce state.
     *
     * The ordinary press event returned here is ignored
     * because this function is only interested in the
     * hold event.
     */
    button_update_state(button);

    /*
     * A hold is only possible while the debounced state
     * says that the button remains pressed.
     */
    if (button->stable_state)
    {
        return false;
    }

    /*
     * This hold was already reported.
     */
    if (button->hold_event_reported)
    {
        return false;
    }

    const uint32_t now =
        hal_time_millis();

    /*
     * Unsigned subtraction also works correctly when
     * the millisecond counter eventually overflows.
     */
    if ((now - button->pressed_since_ms) <
        hold_ms)
    {
        return false;
    }

    button->hold_event_reported = true;

    return true;
}

void button_suppress_hold_until_release(
    button_t *button
)
{
    if (button == nullptr)
    {
        return;
    }

    /*
     * button_update_state() clears this flag when a new
     * debounced press begins after a release.
     */
    button->hold_event_reported = true;
}
