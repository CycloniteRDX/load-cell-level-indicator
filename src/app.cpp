
#include <math.h>
#include <stdint.h>

#include "app.h"
#include "button.h"
#include "config.h"
#include "console.h"
#include "level_indicator.h"
#include "scale.h"
#include "calibration_storage.h"
#include "tare_storage.h"
#include "indicator_leds.h"
#include "operation_indicator.h"
#include "hal_time.h"


static float latest_weight_grams = 0.0F;
static bool measurement_available = false;
static bool tare_available = false;

static button_t tare_button;
static button_t calibration_button;

static uint32_t last_print_ms = 0;


typedef enum
{
    CALIBRATION_IDLE,

    /*
     * Waiting for the user to remove the measured load
     * and confirm the zero condition.
     */
    CALIBRATION_WAITING_FOR_ZERO,

    /*
     * The tare has been completed.
     * Waiting for the reference mass.
     */
    CALIBRATION_WAITING_FOR_MASS

} calibration_state_t;


static calibration_state_t calibration_state =
    CALIBRATION_IDLE;


static operation_indicator_mode_t
get_idle_operation_mode(void)
{
    if (tare_available)
    {
        return OPERATION_INDICATOR_NONE;
    }

    return OPERATION_INDICATOR_TARE_REQUIRED;
}


static void restore_idle_operation_mode(void)
{
    const operation_indicator_mode_t idle_mode =
        get_idle_operation_mode();

    if (idle_mode == OPERATION_INDICATOR_NONE)
    {
        operation_indicator_clear();
        return;
    }

    operation_indicator_set_mode(idle_mode);
}


static bool perform_tare(void)
{
    console_newline();
    CONSOLE_PRINTLN("Taring...");
    CONSOLE_PRINTLN("Leave only the empty platform or container.");

    const int32_t previous_tare_offset =
        scale_get_offset();

    const bool previous_tare_available =
        tare_available;

    const operation_indicator_mode_t return_mode =
        get_idle_operation_mode();

    operation_indicator_set_mode(
        OPERATION_INDICATOR_TARE
    );

    if (!scale_tare())
    {
        measurement_available = false;
        level_indicator_reset();

        operation_indicator_show_error(
            return_mode
        );

        CONSOLE_PRINTLN(
            "ERROR: Tare samples could not be read."
        );

        CONSOLE_PRINTLN(
            "The previous tare offset remains active."
        );

        console_newline();
        console_discard_input();

        return false;
    }

    const int32_t new_tare_offset =
        scale_get_offset();

    if (!tare_storage_save(
            new_tare_offset))
    {
        scale_set_offset(
            previous_tare_offset
        );

        tare_available =
            previous_tare_available;

        measurement_available = false;
        level_indicator_reset();

        operation_indicator_show_error(
            return_mode
        );

        CONSOLE_PRINTLN(
            "ERROR: New tare offset could not be saved."
        );

        CONSOLE_PRINTLN(
            "The previous tare offset has been restored."
        );

        console_newline();
        console_discard_input();

        return false;
    }

    tare_available = true;

    operation_indicator_clear();

    CONSOLE_PRINT("New tare offset: ");
    console_print_int32(
        scale_get_offset()
    );
    console_newline();

    /*
     * The previous measurement is no longer valid
     * because the tare offset has changed.
     */
    measurement_available = false;
    level_indicator_reset();

    CONSOLE_PRINTLN("Tare completed and saved.");
    console_newline();

    /*
     * scale_tare() blocks the application loop, but
     * USART reception continues through its interrupt.
     *
     * Discard commands received while the tare was in
     * progress so they are not executed afterwards.
     *
     * This makes serial input consistent with physical
     * button presses, which are not processed while the
     * application is blocked.
     */
    console_discard_input();

    return true;
}


static void save_current_calibration(void)
{
    const float calibration_factor =
        scale_get_calibration_factor();

    console_newline();
    CONSOLE_PRINT("Saving calibration factor: ");
    console_print_float(
        calibration_factor,
        6U
    );
    CONSOLE_PRINTLN(" counts/g");

    if (!calibration_storage_save(
            calibration_factor))
    {
        CONSOLE_PRINTLN("ERROR: Calibration could not be saved.");

        console_newline();
        return;
    }

    CONSOLE_PRINTLN("Calibration saved successfully.");

    console_newline();
}


static void clear_stored_calibration(void)
{
    console_newline();

    if (!calibration_storage_clear())
    {
        CONSOLE_PRINTLN("ERROR: Stored calibration "
            "could not be cleared.");

        console_newline();
        return;
    }

    CONSOLE_PRINTLN("Stored calibration cleared.");

    CONSOLE_PRINTLN("The active factor remains unchanged "
        "until the next restart.");

    console_newline();
}


static bool calibration_is_active(void)
{
    return calibration_state !=
           CALIBRATION_IDLE;
}


static void start_calibration(void)
{
    measurement_available = false;
    level_indicator_reset();

    calibration_state =
        CALIBRATION_WAITING_FOR_ZERO;

    operation_indicator_set_mode(
        OPERATION_INDICATOR_CALIBRATION_ZERO
    );

    console_newline();
    CONSOLE_PRINTLN(
        "=== Calibration started ==="
    );

    CONSOLE_PRINTLN(
        "Remove the measured load."
    );

    CONSOLE_PRINTLN(
        "Leave only the empty platform or container."
    );

    CONSOLE_PRINTLN(
        "Wait until the system is stable."
    );

    CONSOLE_PRINTLN(
        "Send 'c' or press CAL to confirm the zero condition."
    );

    CONSOLE_PRINTLN(
        "Press TARE or send 'q' to cancel calibration."
    );

    console_newline();
}


static void confirm_calibration_zero(void)
{
    console_newline();
    CONSOLE_PRINTLN("Performing calibration tare...");

    operation_indicator_set_mode(
        OPERATION_INDICATOR_TARE
    );

    if (!scale_tare())
    {
        operation_indicator_show_error(
            OPERATION_INDICATOR_CALIBRATION_ZERO
        );

        CONSOLE_PRINTLN(
            "ERROR: Calibration tare samples "
            "could not be read."
        );

        CONSOLE_PRINTLN(
            "Keep the empty container in place "
            "and confirm again."
        );

        console_newline();
        console_discard_input();
        return;
    }

    tare_available = true;

    CONSOLE_PRINT("Tare offset: ");
    console_print_int32(
        scale_get_offset()
    );
    console_newline();

    calibration_state =
        CALIBRATION_WAITING_FOR_MASS;

    operation_indicator_set_mode(
        OPERATION_INDICATOR_CALIBRATION_MASS
    );

    console_newline();
    CONSOLE_PRINT("Place the reference mass: ");
    console_print_float(
        CALIBRATION_MASS_GRAMS,
        2U
    );
    CONSOLE_PRINTLN(" g");

    CONSOLE_PRINTLN(
        "Wait until the measurement is stable."
    );

    CONSOLE_PRINTLN(
        "Send 'c' or press CAL to calculate and save calibration."
    );

    CONSOLE_PRINTLN(
        "Press TARE or send 'q' to cancel calibration."
    );

    console_newline();

    /*
     * Ignore commands accumulated while the blocking
     * calibration tare was running.
     *
     * In particular, a second queued 'c' must not
     * advance immediately to calibration completion
     * before the reference mass has been placed.
     */
    console_discard_input();
}


static void complete_calibration(void)
{
    console_newline();
    CONSOLE_PRINTLN(
        "Collecting calibration samples..."
    );

    float net_counts = 0.0F;

    const bool samples_read =
        scale_read_net_counts(
            &net_counts,
            CALIBRATION_SAMPLES
        );

    /*
     * The sample collection is blocking. Ignore any
     * commands received while it was in progress so
     * they cannot affect the new calibration state
     * after the operation finishes.
     */
    console_discard_input();

    if (!samples_read)
    {
        CONSOLE_PRINTLN(
            "ERROR: Calibration samples "
            "could not be read."
        );

        CONSOLE_PRINTLN(
            "Keep the mass in place and confirm again."
        );

        operation_indicator_show_error(
            OPERATION_INDICATOR_CALIBRATION_MASS
        );

        console_newline();
        return;
    }

    CONSOLE_PRINT("Net ADC counts: ");
    console_print_float(
        net_counts,
        2U
    );
    console_newline();

    /*
     * Reject a calibration where the applied mass
     * produced no meaningful signal.
     */
    if (fabsf(net_counts) <
        MINIMUM_CALIBRATION_SIGNAL_COUNTS)
    {
        CONSOLE_PRINTLN(
            "ERROR: Calibration signal is too small."
        );

        CONSOLE_PRINTLN(
            "Check that the reference mass is applied."
        );

        CONSOLE_PRINTLN(
            "Send 'c' to try again or 'q' to cancel."
        );

        operation_indicator_show_error(
            OPERATION_INDICATOR_CALIBRATION_MASS
        );

        console_newline();
        return;
    }

    const float new_calibration_factor =
        net_counts / CALIBRATION_MASS_GRAMS;

    const float previous_calibration_factor =
        scale_get_calibration_factor();

    CONSOLE_PRINT("Calculated factor: ");
    console_print_float(
        new_calibration_factor,
        6U
    );
    CONSOLE_PRINTLN(" counts/g");

    /*
     * Validate and apply the new factor first.
     */
    if (!scale_set_calibration_factor(
            new_calibration_factor))
    {
        CONSOLE_PRINTLN(
            "ERROR: Calculated calibration "
            "factor is invalid."
        );

        operation_indicator_show_error(
            OPERATION_INDICATOR_CALIBRATION_MASS
        );

        console_newline();
        return;
    }

    /*
     * Save it permanently.
     */
    if (!calibration_storage_save(
            new_calibration_factor))
    {
        CONSOLE_PRINTLN(
            "ERROR: New calibration could "
            "not be saved."
        );

        /*
         * Restore the previous active factor so that
         * a storage failure does not silently change
         * the current operating configuration.
         */
        if (!scale_set_calibration_factor(
                previous_calibration_factor))
        {
            CONSOLE_PRINTLN(
                "ERROR: Previous factor could "
                "not be restored."
            );
        }

        calibration_state =
            CALIBRATION_IDLE;

        measurement_available = false;

        level_indicator_reset();

        operation_indicator_show_error(
            OPERATION_INDICATOR_NONE
        );

        CONSOLE_PRINTLN(
            "Calibration cancelled."
        );

        console_newline();
        return;
    }

    calibration_state =
        CALIBRATION_IDLE;

    measurement_available = false;

    level_indicator_reset();

    operation_indicator_show_success();

    console_newline();
    CONSOLE_PRINTLN(
        "Calibration completed successfully."
    );

    CONSOLE_PRINT("Active factor: ");
    console_print_float(
        scale_get_calibration_factor(),
        6U
    );
    CONSOLE_PRINTLN(" counts/g");

    CONSOLE_PRINTLN(
        "The factor has been saved to EEPROM."
    );

    CONSOLE_PRINTLN(
        "Normal measurement resumed."
    );

    console_newline();
}


static void cancel_calibration(void)
{
    if (!calibration_is_active())
    {
        console_newline();
        CONSOLE_PRINTLN(
            "No calibration is currently active."
        );
        console_newline();
        return;
    }

    calibration_state =
        CALIBRATION_IDLE;

    measurement_available = false;

    restore_idle_operation_mode();
    level_indicator_reset();

    console_newline();
    CONSOLE_PRINTLN("Calibration cancelled.");

    CONSOLE_PRINTLN(
        "The active calibration factor "
        "has not been changed."
    );

    if (tare_available)
    {
        CONSOLE_PRINTLN(
            "Normal measurement resumed."
        );
    }
    else
    {
        CONSOLE_PRINTLN(
            "Tare is still required before "
            "normal measurement."
        );
    }

    console_newline();
}


static void process_calibration_confirmation(void)
{
    switch (calibration_state)
    {
        case CALIBRATION_IDLE:
            start_calibration();
            break;

        case CALIBRATION_WAITING_FOR_ZERO:
            confirm_calibration_zero();
            break;

        case CALIBRATION_WAITING_FOR_MASS:
            complete_calibration();
            break;

        default:
            calibration_state =
                CALIBRATION_IDLE;

            measurement_available = false;

            restore_idle_operation_mode();
            level_indicator_reset();

            CONSOLE_PRINTLN(
                "ERROR: Invalid calibration state."
            );

            break;
    }
}


static void process_calibration_button(void)
{
    /*
     * While calibration is active, every ordinary
     * press confirms the current calibration step.
     */
    if (calibration_is_active())
    {
        if (button_was_pressed(
                &calibration_button))
        {
            process_calibration_confirmation();
        }

        return;
    }

    /*
     * During normal operation, a long press is required
     * to start the calibration workflow.
     */
    if (button_was_held(
            &calibration_button,
            CALIBRATION_START_HOLD_MS))
    {
        process_calibration_confirmation();
    }
}


static void process_console_commands(void)
{
    if (!console_input_available())
    {
        return;
    }

    char command = '\0';

    if (!console_read_char(&command))
    {
        return;
    }

    console_discard_input();

    switch (command)
    {
        case 'c':
        case 'C':
            process_calibration_confirmation();
            break;

        case 'q':
        case 'Q':
            cancel_calibration();
            break;

        case 't':
        case 'T':

            if (calibration_is_active())
            {
                CONSOLE_PRINTLN(
                    "Tare is unavailable during calibration."
                );
            }
            else
            {
                perform_tare();
            }

            break;

        case 's':
        case 'S':

            if (calibration_is_active())
            {
                CONSOLE_PRINTLN(
                    "Manual save is unavailable "
                    "during calibration."
                );
            }
            else
            {
                save_current_calibration();
            }

            break;

        case 'x':
        case 'X':

            if (calibration_is_active())
            {
                CONSOLE_PRINTLN(
                    "Stored calibration cannot be cleared "
                    "during calibration."
                );
            }
            else
            {
                clear_stored_calibration();
            }

            break;

        default:
            CONSOLE_PRINTLN("Unknown command.");
            CONSOLE_PRINTLN("Available commands:");
            CONSOLE_PRINTLN("  t = tare");
            CONSOLE_PRINTLN("  c = start/confirm calibration");
            CONSOLE_PRINTLN("  q = cancel calibration");
            CONSOLE_PRINTLN("  s = save active calibration");
            CONSOLE_PRINTLN("  x = clear stored calibration");
            break;
    }
}


static void update_weight_measurement(void)
{
    float weight_grams = 0.0F;

    if (!scale_read_weight(&weight_grams))
    {
        return;
    }

    latest_weight_grams = weight_grams;
    measurement_available = true;

    level_indicator_update(latest_weight_grams);
}


static void print_weight_periodically(void)
{
    const uint32_t now = hal_time_millis();

    if ((now - last_print_ms) < PRINT_PERIOD_MS)
    {
        return;
    }

    last_print_ms = now;

    if (!measurement_available)
    {
        CONSOLE_PRINTLN(
            "Waiting for weight measurement..."
        );
        return;
    }

    CONSOLE_PRINT("Weight: ");
    console_print_float(
        latest_weight_grams,
        2U
    );
    CONSOLE_PRINT(" g | Level: ");
    console_println(
        level_indicator_get_state_name()
    );
}


void app_init(void)
{

    /*
     * Initialize the active time backend before any
     * module reads the project millisecond counter.
     */
    hal_time_init();

    console_init(CONSOLE_BAUD_RATE);

    button_init(
        &tare_button,
        TARE_BUTTON_PIN,
        BUTTON_DEBOUNCE_MS
    );
    button_init(
        &calibration_button,
        CALIBRATION_BUTTON_PIN,
        BUTTON_DEBOUNCE_MS
    );

    indicator_leds_init();
    level_indicator_init();
    operation_indicator_init();

    console_newline();
    CONSOLE_PRINTLN(
        "=== Load cell level indicator ==="
    );

    if (!scale_init())
    {
        CONSOLE_PRINTLN("ERROR: HX711 not found.");

        while (true)
        {
            hal_time_delay_ms(1000UL);
        }
    }

    float calibration_factor =
        DEFAULT_CALIBRATION_FACTOR;

    const bool stored_calibration_loaded =
        calibration_storage_load(
            &calibration_factor
        );

    if (stored_calibration_loaded)
    {
        CONSOLE_PRINTLN(
            "Stored calibration loaded from EEPROM."
        );
    }
    else
    {
        CONSOLE_PRINTLN(
            "No valid stored calibration found."
        );

        CONSOLE_PRINTLN(
            "Using default calibration factor."
        );
    }

    if (!scale_set_calibration_factor(
            calibration_factor))
    {
        CONSOLE_PRINTLN(
            "ERROR: Invalid calibration factor."
        );

        while (true)
        {
            hal_time_delay_ms(1000UL);
        }
    }


    CONSOLE_PRINT("Calibration factor: ");
    console_print_float(
        scale_get_calibration_factor(),
        6U
    );
    CONSOLE_PRINTLN(" counts/g");

    int32_t stored_tare_offset = 0;

    if (tare_storage_load(
            &stored_tare_offset))
    {
        scale_set_offset(
            stored_tare_offset
        );

        tare_available = true;

        CONSOLE_PRINT("Stored tare offset loaded: ");
        console_print_int32(
            scale_get_offset()
        );
        console_newline();

        CONSOLE_PRINTLN(
            "Normal measurement started."
        );
    }
    else
    {
        tare_available = false;
        measurement_available = false;
        level_indicator_reset();

        operation_indicator_set_mode(
            OPERATION_INDICATOR_TARE_REQUIRED
        );

        CONSOLE_PRINTLN(
            "No valid tare offset is stored."
        );

        CONSOLE_PRINTLN(
            "Normal level indication is disabled."
        );

        CONSOLE_PRINTLN(
            "Place the empty container on the platform."
        );

        CONSOLE_PRINTLN(
            "Press TARE or send 't' to establish zero."
        );
    }

    console_newline();
    CONSOLE_PRINTLN("Controls:");
    CONSOLE_PRINTLN(
        "  Physical button on D4 = tare"
    );
    CONSOLE_PRINTLN(
        "  Serial command 't'    = tare"
    );
    CONSOLE_PRINTLN(
        "  Serial command 's'    = save calibration"
    );
    CONSOLE_PRINTLN(
        "  Serial command 'x'    = clear calibration"
    );
    CONSOLE_PRINTLN(
        "  Serial command 'c'    = calibrate/confirm"
    );

    CONSOLE_PRINTLN(
        "  Serial command 'q'    = cancel calibration"
    );

    CONSOLE_PRINTLN(
        " Hold button on D8 for 3 s = start calibration"
    );

    CONSOLE_PRINTLN(
        " Press button on D8 = confirm calibration step"
    );

    CONSOLE_PRINTLN("LED operation status:");

    CONSOLE_PRINTLN(
        " All LEDs on = tare in progress"
    );

    CONSOLE_PRINTLN(
        " All LEDs blinking slowly = tare required"
    );

    CONSOLE_PRINTLN(
        " LOW blinking = waiting for empty platform"
    );

    CONSOLE_PRINTLN(
        " MEDIUM blinking = waiting for reference mass"
    );

    console_newline();
    CONSOLE_PRINTLN("Provisional levels:");

    CONSOLE_PRINTLN(
        " VERY_LOW: below 100 g, LOW LED blinking"
    );

    CONSOLE_PRINTLN(
        " LOW: 100 to 500 g, LOW LED steady"
    );

    CONSOLE_PRINTLN(
        " MEDIUM: 500 to 1000 g"
    );

    CONSOLE_PRINTLN(
        " HIGH: above 1000 g"
    );
}


void app_update(void)
{
    /*
     * Update the current visual pattern first.
     */
    operation_indicator_update();

    /*
     * While a finite success or error pattern is being
     * displayed, normal operation remains paused.
     */
    if (operation_indicator_is_temporary_active())
    {
        /*
         * Physical buttons are not processed while a
         * finite success or error pattern is active.
         *
         * Discard serial input as well so both input
         * mechanisms follow the same busy-state policy.
         */
        console_discard_input();
        return;
    }

    process_console_commands();

    /*
     * A console command may have started a temporary
     * success or error pattern.
     */
    if (operation_indicator_is_temporary_active())
    {
        return;
    }

    /*
     * During calibration, D4 changes its role:
     *
     * normal operation → tare
     * calibration      → cancel
     */
    if (calibration_is_active())
    {
        if (button_was_pressed(&tare_button))
        {
            cancel_calibration();
            return;
        }

        process_calibration_button();
        return;
    }

    /*
     * Outside calibration, a long press of D8 may start
     * the calibration workflow.
     */
    process_calibration_button();

    if (calibration_is_active())
    {
        return;
    }

    if (button_was_pressed(&tare_button))
    {
        perform_tare();

        if (operation_indicator_is_temporary_active())
        {
            return;
        }
    }

    if (!tare_available)
    {
        measurement_available = false;
        level_indicator_reset();
        return;
    }

    update_weight_measurement();

    /*
    * Update normal level visual effects.
    *
    * This point is only reached when no calibration or
    * temporary operation pattern owns the LEDs.
    */
    level_indicator_update_visual();

    print_weight_periodically();
}