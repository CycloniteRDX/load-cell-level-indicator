#include <Arduino.h>

#include "config.h"
#include "indicator_leds.h"
#include "level_indicator.h"


typedef enum
{
    LEVEL_UNKNOWN,
    LEVEL_VERY_LOW,
    LEVEL_LOW,
    LEVEL_MEDIUM,
    LEVEL_HIGH

} level_state_t;


/*
 * Internal state of the module.
 *
 * static prevents other source files from accessing
 * this variable directly.
 */
static level_state_t current_level =
    LEVEL_UNKNOWN;

/*
 * Current phase of the VERY_LOW blinking pattern.
 *
 * true  = LOW LED illuminated
 * false = LOW LED off
 */
static bool very_low_led_on = false;


/*
 * Time at which the VERY_LOW LED last changed state.
 */
static unsigned long
    last_very_low_blink_change_ms = 0UL;


/*
 * Applies the physical LED output associated with
 * the current logical level and visual phase.
 */
static void set_level_leds(
    level_state_t level
)
{
    switch (level)
    {
        case LEVEL_VERY_LOW:
            indicator_leds_set(
                very_low_led_on,
                false,
                false
            );
            break;

        case LEVEL_LOW:
            indicator_leds_set(
                true,
                false,
                false
            );
            break;

        case LEVEL_MEDIUM:
            indicator_leds_set(
                false,
                true,
                false
            );
            break;

        case LEVEL_HIGH:
            indicator_leds_set(
                false,
                false,
                true
            );
            break;

        case LEVEL_UNKNOWN:
        default:
            indicator_leds_off();
            break;
    }
}


/*
 * Initializes the visual representation when the
 * logical level changes.
 */
static void start_level_visual(
    level_state_t level
)
{
    if (level == LEVEL_VERY_LOW)
    {
        /*
         * Begin the warning with the LED illuminated,
         * giving immediate feedback when VERY_LOW
         * becomes active.
         */
        very_low_led_on = true;

        last_very_low_blink_change_ms =
            millis();
    }
    else
    {
        /*
         * Other levels do not use the blinking phase.
         */
        very_low_led_on = false;
    }

    set_level_leds(level);
}


/*
 * Selects a level without hysteresis.
 *
 * This is used only after startup or reset, when there
 * is no previous level from which to apply hysteresis.
 */
static level_state_t select_initial_level(
    float weight_grams
)
{
    if (weight_grams <
        VERY_LOW_LOW_THRESHOLD_GRAMS)
    {
        return LEVEL_VERY_LOW;
    }

    if (weight_grams <
        LOW_MEDIUM_THRESHOLD_GRAMS)
    {
        return LEVEL_LOW;
    }

    if (weight_grams <
        MEDIUM_HIGH_THRESHOLD_GRAMS)
    {
        return LEVEL_MEDIUM;
    }

    return LEVEL_HIGH;
}


void level_indicator_init(void)
{
    level_indicator_reset();
}


void level_indicator_reset(void)
{
    current_level =
        LEVEL_UNKNOWN;

    very_low_led_on = false;

    last_very_low_blink_change_ms =
        millis();

    set_level_leds(current_level);
}


void level_indicator_update(
    float weight_grams
)
{
    /*
     * There is no previous state after startup or reset.
     * Select the initial level without hysteresis.
     */
    if (current_level == LEVEL_UNKNOWN)
    {
        current_level =
            select_initial_level(weight_grams);

        start_level_visual(current_level);
        return;
    }

    const level_state_t previous_level =
        current_level;

    switch (current_level)
    {
        case LEVEL_VERY_LOW:

            /*
             * Allow direct jumps to any higher level
             * when a significant load is added.
             */
            if (weight_grams >=
                (MEDIUM_HIGH_THRESHOLD_GRAMS +
                 LEVEL_HYSTERESIS_GRAMS))
            {
                current_level =
                    LEVEL_HIGH;
            }
            else if (weight_grams >=
                     (LOW_MEDIUM_THRESHOLD_GRAMS +
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level =
                    LEVEL_MEDIUM;
            }
            else if (weight_grams >=
                     (VERY_LOW_LOW_THRESHOLD_GRAMS +
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level =
                    LEVEL_LOW;
            }

            break;

        case LEVEL_LOW:

            /*
             * Allow a direct jump from LOW to HIGH.
             */
            if (weight_grams >=
                (MEDIUM_HIGH_THRESHOLD_GRAMS +
                 LEVEL_HYSTERESIS_GRAMS))
            {
                current_level =
                    LEVEL_HIGH;
            }
            else if (weight_grams >=
                     (LOW_MEDIUM_THRESHOLD_GRAMS +
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level =
                    LEVEL_MEDIUM;
            }
            else if (weight_grams <=
                     (VERY_LOW_LOW_THRESHOLD_GRAMS -
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level =
                    LEVEL_VERY_LOW;
            }

            break;

        case LEVEL_MEDIUM:

            if (weight_grams >=
                (MEDIUM_HIGH_THRESHOLD_GRAMS +
                 LEVEL_HYSTERESIS_GRAMS))
            {
                current_level =
                    LEVEL_HIGH;
            }
            else if (weight_grams <=
                     (VERY_LOW_LOW_THRESHOLD_GRAMS -
                      LEVEL_HYSTERESIS_GRAMS))
            {
                /*
                 * Allow a direct jump from MEDIUM
                 * to VERY_LOW.
                 */
                current_level =
                    LEVEL_VERY_LOW;
            }
            else if (weight_grams <=
                     (LOW_MEDIUM_THRESHOLD_GRAMS -
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level =
                    LEVEL_LOW;
            }

            break;

        case LEVEL_HIGH:

            /*
             * Allow direct jumps to lower levels when
             * most or all of the load is removed.
             */
            if (weight_grams <=
                (VERY_LOW_LOW_THRESHOLD_GRAMS -
                 LEVEL_HYSTERESIS_GRAMS))
            {
                current_level =
                    LEVEL_VERY_LOW;
            }
            else if (weight_grams <=
                     (LOW_MEDIUM_THRESHOLD_GRAMS -
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level =
                    LEVEL_LOW;
            }
            else if (weight_grams <=
                     (MEDIUM_HIGH_THRESHOLD_GRAMS -
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level =
                    LEVEL_MEDIUM;
            }

            break;

        case LEVEL_UNKNOWN:
        default:

            /*
             * Recover safely from an unexpected state.
             */
            level_indicator_reset();
            return;
    }

    /*
    * Restart the visual representation only when the
    * logical state actually changes.
    */
    if (current_level != previous_level)
    {
        start_level_visual(current_level);
    }
}

void level_indicator_update_visual(void)
{
    /*
     * Only VERY_LOW has a time-dependent pattern.
     */
    if (current_level != LEVEL_VERY_LOW)
    {
        return;
    }

    const unsigned long now = millis();

    /*
     * Unsigned subtraction remains correct when
     * millis() eventually overflows.
     */
    if ((now - last_very_low_blink_change_ms) <
        VERY_LOW_BLINK_PERIOD_MS)
    {
        return;
    }

    last_very_low_blink_change_ms = now;

    very_low_led_on =
        !very_low_led_on;

    set_level_leds(current_level);
}

const char *level_indicator_get_state_name(void)
{
    switch (current_level)
    {
        case LEVEL_VERY_LOW:
            return "VERY_LOW";

        case LEVEL_LOW:
            return "LOW";

        case LEVEL_MEDIUM:
            return "MEDIUM";

        case LEVEL_HIGH:
            return "HIGH";

        case LEVEL_UNKNOWN:
        default:
            return "UNKNOWN";
    }
}