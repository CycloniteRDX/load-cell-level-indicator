#include <Arduino.h>

#include "config.h"
#include "indicator_leds.h"
#include "operation_indicator.h"


static operation_indicator_mode_t current_mode =
    OPERATION_INDICATOR_NONE;

static bool blink_led_on = false;

static unsigned long last_blink_change_ms = 0UL;


/*
 * Applies the LED output corresponding to the current
 * mode and current blink phase.
 */
static void apply_current_output(void)
{
    switch (current_mode)
    {
        case OPERATION_INDICATOR_TARE:
            indicator_leds_set(
                true,
                true,
                true
            );
            break;

        case OPERATION_INDICATOR_CALIBRATION_ZERO:
            indicator_leds_set(
                blink_led_on,
                false,
                false
            );
            break;

        case OPERATION_INDICATOR_CALIBRATION_MASS:
            indicator_leds_set(
                false,
                blink_led_on,
                false
            );
            break;

        case OPERATION_INDICATOR_NONE:
        default:
            indicator_leds_off();
            break;
    }
}


void operation_indicator_init(void)
{
    current_mode =
        OPERATION_INDICATOR_NONE;

    blink_led_on = false;
    last_blink_change_ms = millis();

    indicator_leds_off();
}


void operation_indicator_set_mode(
    operation_indicator_mode_t mode
)
{
    current_mode = mode;

    /*
     * Blinking modes begin with their LED illuminated,
     * providing immediate feedback when the state changes.
     */
    blink_led_on = true;
    last_blink_change_ms = millis();

    apply_current_output();
}


void operation_indicator_update(void)
{
    /*
     * NONE has no pattern.
     *
     * TARE is continuously illuminated and therefore
     * does not need periodic updates.
     */
    if ((current_mode !=
         OPERATION_INDICATOR_CALIBRATION_ZERO) &&
        (current_mode !=
         OPERATION_INDICATOR_CALIBRATION_MASS))
    {
        return;
    }

    const unsigned long now = millis();

    if ((now - last_blink_change_ms) <
        OPERATION_INDICATOR_BLINK_PERIOD_MS)
    {
        return;
    }

    last_blink_change_ms = now;
    blink_led_on = !blink_led_on;

    apply_current_output();
}


void operation_indicator_clear(void)
{
    current_mode =
        OPERATION_INDICATOR_NONE;

    blink_led_on = false;

    indicator_leds_off();
}