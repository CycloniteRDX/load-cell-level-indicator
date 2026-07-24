#include <Arduino.h>
#include <math.h>
#include <stdint.h>

#include "app.h"
#include "button.h"
#include "config.h"
#include "level_indicator.h"
#include "scale.h"
#include "calibration_storage.h"
#include "indicator_leds.h"
#include "operation_indicator.h"
#include "hal_time.h"


static float latest_weight_grams = 0.0F;
static bool measurement_available = false;

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


static void clear_serial_input(void)
{
    while (Serial.available() > 0)
    {
        Serial.read();
    }
}


static void perform_tare(void)
{
    Serial.println();
    Serial.println("Taring...");
    Serial.println(
        "Leave only the empty platform or container."
    );

    operation_indicator_set_mode(
        OPERATION_INDICATOR_TARE
    );

    scale_tare();

    operation_indicator_clear();

    Serial.print("New tare offset: ");
    Serial.println(scale_get_offset());

    /*
     * The previous measurement is no longer valid
     * because the tare offset has changed.
     */
    measurement_available = false;
    level_indicator_reset();

    Serial.println("Tare completed.");
    Serial.println();
}


static void save_current_calibration(void)
{
    const float calibration_factor =
        scale_get_calibration_factor();

    Serial.println();
    Serial.print(
        "Saving calibration factor: "
    );
    Serial.print(calibration_factor, 6);
    Serial.println(" counts/g");

    if (!calibration_storage_save(
            calibration_factor))
    {
        Serial.println(
            "ERROR: Calibration could not be saved."
        );

        Serial.println();
        return;
    }

    Serial.println(
        "Calibration saved successfully."
    );

    Serial.println();
}


static void clear_stored_calibration(void)
{
    Serial.println();

    if (!calibration_storage_clear())
    {
        Serial.println(
            "ERROR: Stored calibration "
            "could not be cleared."
        );

        Serial.println();
        return;
    }

    Serial.println(
        "Stored calibration cleared."
    );

    Serial.println(
        "The active factor remains unchanged "
        "until the next restart."
    );

    Serial.println();
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

    Serial.println();
    Serial.println(F(
        "=== Calibration started ==="
    ));

    Serial.println(F(
        "Remove the measured load."
    ));

    Serial.println(F(
        "Leave only the empty platform or container."
    ));

    Serial.println(F(
        "Wait until the system is stable."
    ));

    Serial.println(F(
        "Send 'c' or press CAL to confirm the zero condition."
    ));

    Serial.println(F(
        "Press TARE or send 'q' to cancel calibration."
    ));

    Serial.println();
}


static void confirm_calibration_zero(void)
{
    Serial.println();
    Serial.println(F("Performing calibration tare..."));

    operation_indicator_set_mode(
        OPERATION_INDICATOR_TARE
    );

    scale_tare();

    Serial.print("Tare offset: ");
    Serial.println(scale_get_offset());

    calibration_state =
        CALIBRATION_WAITING_FOR_MASS;

    operation_indicator_set_mode(
        OPERATION_INDICATOR_CALIBRATION_MASS
    );

    Serial.println();
    Serial.print(F("Place the reference mass: "));
    Serial.print(CALIBRATION_MASS_GRAMS, 2);
    Serial.println(F(" g"));

    Serial.println(F(
        "Wait until the measurement is stable."
    ));

    Serial.println(F(
        "Send 'c' or press CAL to calculate and save calibration."
    ));

    Serial.println(F(
        "Press TARE or send 'q' to cancel calibration."
    ));

    Serial.println();
}


static void complete_calibration(void)
{
    Serial.println();
    Serial.println(F(
        "Collecting calibration samples..."
    ));

    float net_counts = 0.0F;

    if (!scale_read_net_counts(
            &net_counts,
            CALIBRATION_SAMPLES))
    {
        Serial.println(F(
            "ERROR: Calibration samples "
            "could not be read."
        ));

        Serial.println(F(
            "Keep the mass in place and confirm again."
        ));

        operation_indicator_show_error(
            OPERATION_INDICATOR_CALIBRATION_MASS
        );

        Serial.println();
        return;
    }

    Serial.print(F("Net ADC counts: "));
    Serial.println(net_counts, 2);

    /*
     * Reject a calibration where the applied mass
     * produced no meaningful signal.
     */
    if (fabsf(net_counts) <
        MINIMUM_CALIBRATION_SIGNAL_COUNTS)
    {
        Serial.println(F(
            "ERROR: Calibration signal is too small."
        ));

        Serial.println(F(
            "Check that the reference mass is applied."
        ));

        Serial.println(F(
            "Send 'c' to try again or 'q' to cancel."
        ));

        operation_indicator_show_error(
            OPERATION_INDICATOR_CALIBRATION_MASS
        );

        Serial.println();
        return;
    }

    const float new_calibration_factor =
        net_counts / CALIBRATION_MASS_GRAMS;

    const float previous_calibration_factor =
        scale_get_calibration_factor();

    Serial.print(F("Calculated factor: "));
    Serial.print(new_calibration_factor, 6);
    Serial.println(F(" counts/g"));

    /*
     * Validate and apply the new factor first.
     */
    if (!scale_set_calibration_factor(
            new_calibration_factor))
    {
        Serial.println(F(
            "ERROR: Calculated calibration "
            "factor is invalid."
        ));

        operation_indicator_show_error(
            OPERATION_INDICATOR_CALIBRATION_MASS
        );

        Serial.println();
        return;
    }

    /*
     * Save it permanently.
     */
    if (!calibration_storage_save(
            new_calibration_factor))
    {
        Serial.println(F(
            "ERROR: New calibration could "
            "not be saved."
        ));

        /*
         * Restore the previous active factor so that
         * a storage failure does not silently change
         * the current operating configuration.
         */
        if (!scale_set_calibration_factor(
                previous_calibration_factor))
        {
            Serial.println(F(
                "ERROR: Previous factor could "
                "not be restored."
            ));
        }

        calibration_state =
            CALIBRATION_IDLE;

        measurement_available = false;

        level_indicator_reset();

        operation_indicator_show_error(
            OPERATION_INDICATOR_NONE
        );

        Serial.println(F(
            "Calibration cancelled."
        ));

        Serial.println();
        return;
    }

    calibration_state =
        CALIBRATION_IDLE;

    measurement_available = false;

    level_indicator_reset();

    operation_indicator_show_success();

    Serial.println();
    Serial.println(F(
        "Calibration completed successfully."
    ));

    Serial.print(F("Active factor: "));
    Serial.print(
        scale_get_calibration_factor(),
        6
    );
    Serial.println(F(" counts/g"));

    Serial.println(F(
        "The factor has been saved to EEPROM."
    ));

    Serial.println(F(
        "Normal measurement resumed."
    ));

    Serial.println();
}


static void cancel_calibration(void)
{
    if (!calibration_is_active())
    {
        Serial.println();
        Serial.println(F(
            "No calibration is currently active."
        ));
        Serial.println();
        return;
    }

    calibration_state =
        CALIBRATION_IDLE;

    measurement_available = false;

    operation_indicator_clear();
    level_indicator_reset();

    Serial.println();
    Serial.println(F("Calibration cancelled."));

    Serial.println(F(
        "The active calibration factor "
        "has not been changed."
    ));

    Serial.println(F(
        "Normal measurement resumed."
    ));

    Serial.println();
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

            operation_indicator_clear();
            level_indicator_reset();

            Serial.println(F(
                "ERROR: Invalid calibration state."
            ));

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


static void process_serial_commands(void)
{
    if (Serial.available() == 0)
    {
        return;
    }

    const char command = Serial.read();

    clear_serial_input();

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
                Serial.println(F(
                    "Tare is unavailable during calibration."
                ));
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
                Serial.println(F(
                    "Manual save is unavailable "
                    "during calibration."
                ));
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
                Serial.println(F(
                    "Stored calibration cannot be cleared "
                    "during calibration."
                ));
            }
            else
            {
                clear_stored_calibration();
            }

            break;

        default:
            Serial.println(F("Unknown command."));
            Serial.println(F("Available commands:"));
            Serial.println(F("  t = tare"));
            Serial.println(F("  c = start/confirm calibration"));
            Serial.println(F("  q = cancel calibration"));
            Serial.println(F("  s = save active calibration"));
            Serial.println(F("  x = clear stored calibration"));
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
        Serial.println(F(
            "Waiting for weight measurement..."
        ));
        return;
    }

    Serial.print("Weight: ");
    Serial.print(latest_weight_grams, 2);
    Serial.print(" g | Level: ");
    Serial.println(
        level_indicator_get_state_name()
    );
}


void app_init(void)
{
    Serial.begin(115200);

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

    Serial.println();
    Serial.println(F(
        "=== Load cell level indicator ==="
    ));

    if (!scale_init())
    {
        Serial.println(F("ERROR: HX711 not found."));

        while (true)
        {
            delay(1000);
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
        Serial.println(F(
            "Stored calibration loaded from EEPROM."
        ));
    }
    else
    {
        Serial.println(F(
            "No valid stored calibration found."
        ));

        Serial.println(F(
            "Using default calibration factor."
        ));
    }

    if (!scale_set_calibration_factor(
            calibration_factor))
    {
        Serial.println(F(
            "ERROR: Invalid calibration factor."
        ));

        while (true)
        {
            delay(1000);
        }
    }


    Serial.print("Calibration factor: ");
    Serial.print(
        scale_get_calibration_factor(),
        6
    );
    Serial.println(F(" counts/g"));

    Serial.println();
    Serial.println(F(
        "Automatic tare will start in 3 seconds."
    ));
    Serial.println(F(
        "Leave the scale unloaded or "
        "with the empty container."
    ));

    delay(3000);

    perform_tare();

    Serial.println(F("Controls:"));
    Serial.println(F(
        "  Physical button on D4 = tare"
    ));
    Serial.println(F(
        "  Serial command 't'    = tare"
    ));
    Serial.println(F(
    "  Serial command 's'    = save calibration"
    ));
    Serial.println(
        "  Serial command 'x'    = clear calibration"
    );
    Serial.println(F(
        "  Serial command 'c'    = calibrate/confirm"
    ));

    Serial.println(F(
        "  Serial command 'q'    = cancel calibration"
    ));

    Serial.println(F(
        " Hold button on D8 for 3 s = start calibration"
    ));

    Serial.println(F(
        " Press button on D8 = confirm calibration step"
    ));

    Serial.println(F("LED operation status:"));

    Serial.println(F(
        " All LEDs on = tare in progress"
    ));

    Serial.println(F(
        " LOW blinking = waiting for empty platform"
    ));

    Serial.println(F(
        " MEDIUM blinking = waiting for reference mass"
    ));

    Serial.println();
    Serial.println(F("Provisional levels:"));

    Serial.println(F(
        " VERY_LOW: below 100 g, LOW LED blinking"
    ));

    Serial.println(F(
        " LOW: 100 to 500 g, LOW LED steady"
    ));

    Serial.println(F(
        " MEDIUM: 500 to 1000 g"
    ));

    Serial.println(F(
        " HIGH: above 1000 g"
    ));
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
        return;
    }

    process_serial_commands();

    /*
     * A serial command may have started a temporary
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