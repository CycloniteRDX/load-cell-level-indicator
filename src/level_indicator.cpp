#include <Arduino.h>

#include "config.h"
#include "level_indicator.h"
#include "indicator_leds.h"


typedef enum
{
    LEVEL_UNKNOWN,
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
static level_state_t current_level = LEVEL_UNKNOWN;


static void set_level_leds(
    level_state_t level
)
{
    switch (level)
    {
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


void level_indicator_init(void)
{
    level_indicator_reset();
}


void level_indicator_reset(void)
{
    current_level = LEVEL_UNKNOWN;
    set_level_leds(current_level);
}


void level_indicator_update(float weight_grams)
{
    /*
     * There is no previous state after startup or reset.
     * Select the initial level without hysteresis.
     */
    if (current_level == LEVEL_UNKNOWN)
    {
        if (weight_grams < LOW_MEDIUM_THRESHOLD_GRAMS)
        {
            current_level = LEVEL_LOW;
        }
        else if (weight_grams < MEDIUM_HIGH_THRESHOLD_GRAMS)
        {
            current_level = LEVEL_MEDIUM;
        }
        else
        {
            current_level = LEVEL_HIGH;
        }

        set_level_leds(current_level);
        return;
    }

    switch (current_level)
    {
        case LEVEL_LOW:

            /*
             * Allow a direct jump from LOW to HIGH when
             * the weight changes significantly.
             */
            if (weight_grams >=
                (MEDIUM_HIGH_THRESHOLD_GRAMS +
                 LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_HIGH;
            }
            else if (weight_grams >=
                     (LOW_MEDIUM_THRESHOLD_GRAMS +
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_MEDIUM;
            }

            break;

        case LEVEL_MEDIUM:

            if (weight_grams <=
                (LOW_MEDIUM_THRESHOLD_GRAMS -
                 LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_LOW;
            }
            else if (weight_grams >=
                     (MEDIUM_HIGH_THRESHOLD_GRAMS +
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_HIGH;
            }

            break;

        case LEVEL_HIGH:

            /*
             * Allow a direct jump from HIGH to LOW when
             * most of the load is removed.
             */
            if (weight_grams <=
                (LOW_MEDIUM_THRESHOLD_GRAMS -
                 LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_LOW;
            }
            else if (weight_grams <=
                     (MEDIUM_HIGH_THRESHOLD_GRAMS -
                      LEVEL_HYSTERESIS_GRAMS))
            {
                current_level = LEVEL_MEDIUM;
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

    set_level_leds(current_level);
}


const char *level_indicator_get_state_name(void)
{
    switch (current_level)
    {
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