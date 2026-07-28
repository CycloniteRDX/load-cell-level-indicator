#include <stdint.h>
#include "hal_time.h"

#include "config.h"
#include "indicator_leds.h"
#include "operation_indicator.h"


static operation_indicator_mode_t current_mode =
    OPERATION_INDICATOR_NONE;


/*
 * Mode restored after a temporary success or
 * error pattern finishes.
 */
static operation_indicator_mode_t return_mode =
    OPERATION_INDICATOR_NONE;


static bool blink_led_on = false;

static uint32_t last_blink_change_ms = 0UL;


/*
 * Temporary-pattern progress.
 */
static uint8_t completed_flashes = 0U;
static uint8_t target_flashes = 0U;


static bool mode_is_temporary(
    operation_indicator_mode_t mode
)
{
    return
        (mode == OPERATION_INDICATOR_SUCCESS) ||
        (mode == OPERATION_INDICATOR_ERROR);
}


static bool mode_is_blinking(
    operation_indicator_mode_t mode
)
{
    return
        (mode ==
         OPERATION_INDICATOR_TARE_REQUIRED) ||

        (mode ==
         OPERATION_INDICATOR_CALIBRATION_ZERO) ||

        (mode ==
         OPERATION_INDICATOR_CALIBRATION_MASS) ||

        mode_is_temporary(mode);
}


/*
 * Applies the physical LED output corresponding to
 * the current mode and current blinking phase.
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

        case OPERATION_INDICATOR_TARE_REQUIRED:
            indicator_leds_set(
                blink_led_on,
                blink_led_on,
                blink_led_on
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

        case OPERATION_INDICATOR_SUCCESS:
            indicator_leds_set(
                blink_led_on,
                blink_led_on,
                blink_led_on
            );
            break;

        case OPERATION_INDICATOR_ERROR:
            indicator_leds_set(
                false,
                false,
                blink_led_on
            );
            break;

        case OPERATION_INDICATOR_NONE:
        default:
            indicator_leds_off();
            break;
    }
}


static void start_temporary_pattern(
    operation_indicator_mode_t temporary_mode,
    operation_indicator_mode_t mode_after_pattern,
    uint8_t flash_count
)
{
    current_mode = temporary_mode;
    return_mode = mode_after_pattern;

    completed_flashes = 0U;
    target_flashes = flash_count;

    /*
     * Begin immediately with the selected LEDs on.
     */
    blink_led_on = true;
    last_blink_change_ms = hal_time_millis();

    apply_current_output();
}


static void finish_temporary_pattern(
    uint32_t now
)
{
    current_mode = return_mode;
    return_mode = OPERATION_INDICATOR_NONE;

    completed_flashes = 0U;
    target_flashes = 0U;

    /*
     * A persistent blinking mode restored after an
     * error begins with its LED illuminated.
     */
    blink_led_on = true;
    last_blink_change_ms = now;

    apply_current_output();
}


void operation_indicator_init(void)
{
    current_mode =
        OPERATION_INDICATOR_NONE;

    return_mode =
        OPERATION_INDICATOR_NONE;

    blink_led_on = false;

    last_blink_change_ms = hal_time_millis();

    completed_flashes = 0U;
    target_flashes = 0U;

    indicator_leds_off();
}


void operation_indicator_set_mode(
    operation_indicator_mode_t mode
)
{
    current_mode = mode;

    return_mode =
        OPERATION_INDICATOR_NONE;

    completed_flashes = 0U;
    target_flashes = 0U;

    /*
     * Blinking modes begin illuminated so that the
     * state change is immediately visible.
     */
    blink_led_on = true;
    last_blink_change_ms = hal_time_millis();

    apply_current_output();
}


void operation_indicator_show_success(void)
{
    start_temporary_pattern(
        OPERATION_INDICATOR_SUCCESS,
        OPERATION_INDICATOR_NONE,
        CALIBRATION_SUCCESS_FLASH_COUNT
    );
}


void operation_indicator_show_error(
    operation_indicator_mode_t mode_after_error
)
{
    start_temporary_pattern(
        OPERATION_INDICATOR_ERROR,
        mode_after_error,
        CALIBRATION_ERROR_FLASH_COUNT
    );
}


bool operation_indicator_is_temporary_active(void)
{
    return mode_is_temporary(current_mode);
}


void operation_indicator_update(void)
{
    if (!mode_is_blinking(current_mode))
    {
        return;
    }

    const uint32_t now = hal_time_millis();

    uint32_t blink_period_ms =
        OPERATION_INDICATOR_BLINK_PERIOD_MS;

    if (current_mode ==
        OPERATION_INDICATOR_TARE_REQUIRED)
    {
        blink_period_ms =
            TARE_REQUIRED_BLINK_PERIOD_MS;
    }
    else if (mode_is_temporary(current_mode))
    {
        blink_period_ms =
            OPERATION_RESULT_BLINK_PERIOD_MS;
    }

    if ((now - last_blink_change_ms) <
        blink_period_ms)
    {
        return;
    }

    last_blink_change_ms = now;

    blink_led_on = !blink_led_on;

    apply_current_output();

    /*
     * One complete visible flash finishes whenever
     * an illuminated phase changes to off.
     */
    if (mode_is_temporary(current_mode) &&
        !blink_led_on)
    {
        ++completed_flashes;

        if (completed_flashes >= target_flashes)
        {
            finish_temporary_pattern(now);
        }
    }
}


void operation_indicator_clear(void)
{
    current_mode =
        OPERATION_INDICATOR_NONE;

    return_mode =
        OPERATION_INDICATOR_NONE;

    blink_led_on = false;

    completed_flashes = 0U;
    target_flashes = 0U;

    indicator_leds_off();
}