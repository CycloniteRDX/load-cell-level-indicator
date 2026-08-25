
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "app.h"
#include "app_fault.h"
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
#include "hal_watchdog.h"


static scale_measurement_t latest_measurement = {
    0,
    0,
    0.0F
};
static bool measurement_available = false;
static bool tare_available = false;

static app_fault_code_t active_fault_code =
    APP_FAULT_NONE;

static uint8_t fault_recovery_attempt_count = 0U;
static uint32_t fault_recovery_phase_started_ms = 0UL;
static bool startup_configuration_loaded = false;

static uint32_t scale_runtime_activity_ms = 0UL;

static button_t tare_button;
static button_t calibration_button;

static uint32_t last_print_ms = 0;

static bool diagnostic_capture_active = false;
static uint32_t diagnostic_sequence = 0UL;


/*
 * Cooperative application states.
 *
 * Every long-lived application condition, including a
 * temporary result pattern, has an explicit state.
 */
typedef enum
{
    APP_STATE_STARTUP_WAIT_FOR_SCALE,
    APP_STATE_STARTUP_LOAD_CONFIGURATION,
    APP_STATE_TARE_REQUIRED,
    APP_STATE_NORMAL_OPERATION,
    APP_STATE_TARE_SAMPLING,
    APP_STATE_CALIBRATION_WAITING_FOR_ZERO,
    APP_STATE_CALIBRATION_ZERO_SAMPLING,
    APP_STATE_CALIBRATION_WAITING_FOR_MASS,
    APP_STATE_CALIBRATION_MASS_SAMPLING,
    APP_STATE_RESULT_PATTERN,
    APP_STATE_FAULT_RECOVERY_BACKOFF,
    APP_STATE_FAULT_RECOVERY_WAIT_FOR_SCALE,
    APP_STATE_TERMINAL_FAULT
} app_state_t;


static app_state_t app_state =
    APP_STATE_STARTUP_WAIT_FOR_SCALE;

static uint32_t operation_started_ms = 0UL;

/*
 * Idle state to restore if an operational tare is
 * cancelled or fails.
 */
static app_state_t tare_return_state =
    APP_STATE_TARE_REQUIRED;

/*
 * State entered after a finite success or error
 * indication releases the shared LEDs.
 */
static app_state_t state_after_result =
    APP_STATE_TARE_REQUIRED;


static void stop_diagnostic_capture(void)
{
    if (!diagnostic_capture_active)
    {
        return;
    }

    diagnostic_capture_active = false;
    last_print_ms = hal_time_millis();

    console_newline();
    CONSOLE_PRINTLN("Diagnostic capture stopped.");
    console_newline();
}


static void toggle_diagnostic_capture(void)
{
    if (diagnostic_capture_active)
    {
        stop_diagnostic_capture();
        return;
    }

    if ((app_state != APP_STATE_NORMAL_OPERATION) ||
        !tare_available)
    {
        console_newline();
        CONSOLE_PRINTLN(
            "Diagnostic capture requires normal "
            "measurement and a valid tare."
        );
        console_newline();
        return;
    }

    diagnostic_capture_active = true;
    diagnostic_sequence = 0UL;
    last_print_ms = hal_time_millis();

    console_newline();
    CONSOLE_PRINTLN("Diagnostic capture started.");
    CONSOLE_PRINTLN(
        "DATA,sequence,timestamp_ms,raw_counts,"
        "tare_offset,net_counts,weight_grams"
    );
}


static void print_diagnostic_measurement(
    const scale_measurement_t *measurement,
    uint32_t timestamp_ms
)
{
    if (!diagnostic_capture_active ||
        (measurement == NULL))
    {
        return;
    }

    CONSOLE_PRINT("DATA,");
    console_print_uint32(diagnostic_sequence);
    CONSOLE_PRINT(",");
    console_print_uint32(timestamp_ms);
    CONSOLE_PRINT(",");
    console_print_int32(measurement->raw_counts);
    CONSOLE_PRINT(",");
    console_print_int32(scale_get_offset());
    CONSOLE_PRINT(",");
    console_print_int32(measurement->net_counts);
    CONSOLE_PRINT(",");
    console_print_float(
        measurement->weight_grams,
        6U
    );
    console_newline();

    ++diagnostic_sequence;
}


static void print_reset_causes(void)
{
    const hal_reset_cause_t reset_causes =
        hal_watchdog_get_reset_cause();

    if (reset_causes == HAL_RESET_CAUSE_UNKNOWN)
    {
        CONSOLE_PRINTLN(
            "Reset cause visible to application: unknown."
        );

        return;
    }

    if ((reset_causes &
            HAL_RESET_CAUSE_POWER_ON) != 0U)
    {
        CONSOLE_PRINTLN(
            "Reset cause visible to application: power-on."
        );
    }

    if ((reset_causes &
            HAL_RESET_CAUSE_EXTERNAL) != 0U)
    {
        CONSOLE_PRINTLN(
            "Reset cause visible to application: external."
        );
    }

    if ((reset_causes &
            HAL_RESET_CAUSE_BROWN_OUT) != 0U)
    {
        CONSOLE_PRINTLN(
            "Reset cause visible to application: brown-out."
        );
    }

    if ((reset_causes &
            HAL_RESET_CAUSE_WATCHDOG) != 0U)
    {
        CONSOLE_PRINTLN(
            "Reset cause visible to application: watchdog."
        );
    }
}


static void enter_normal_operation_state(void)
{
    app_state = APP_STATE_NORMAL_OPERATION;

    /*
     * A state transition into normal operation starts a
     * fresh sensor-health window. Time deliberately spent
     * in startup, recovery, tare, calibration or a result
     * pattern must not count as missing runtime activity.
     */
    scale_runtime_activity_ms = hal_time_millis();
}


static app_state_t get_idle_application_state(void)
{
    if (tare_available)
    {
        return APP_STATE_NORMAL_OPERATION;
    }

    return APP_STATE_TARE_REQUIRED;
}


static bool fault_handling_is_active(void)
{
    return
        (app_state ==
            APP_STATE_FAULT_RECOVERY_BACKOFF) ||
        (app_state ==
            APP_STATE_FAULT_RECOVERY_WAIT_FOR_SCALE) ||
        (app_state == APP_STATE_TERMINAL_FAULT);
}


static operation_indicator_mode_t
get_idle_operation_mode(void)
{
    if (tare_available)
    {
        return OPERATION_INDICATOR_NONE;
    }

    return OPERATION_INDICATOR_TARE_REQUIRED;
}


static void restore_idle_application_state(void)
{
    if (tare_available)
    {
        enter_normal_operation_state();
    }
    else
    {
        app_state = APP_STATE_TARE_REQUIRED;
    }

    const operation_indicator_mode_t idle_mode =
        get_idle_operation_mode();

    if (idle_mode == OPERATION_INDICATOR_NONE)
    {
        operation_indicator_clear();
        return;
    }

    operation_indicator_set_mode(idle_mode);
}


static void print_fault_diagnostic(void)
{
    CONSOLE_PRINT("FAULT ");

    if (active_fault_code < 10)
    {
        CONSOLE_PRINT("0");
    }

    console_print_int32(
        (int32_t)active_fault_code
    );

    CONSOLE_PRINT(": ");

    switch (active_fault_code)
    {
        case APP_FAULT_HX711_INITIALIZATION:
            CONSOLE_PRINTLN(
                "HX711 initialization failed."
            );
            break;

        case APP_FAULT_HX711_STARTUP_TIMEOUT:
            CONSOLE_PRINTLN(
                "HX711 startup conversion timeout."
            );
            break;

        case APP_FAULT_HX711_RUNTIME_TIMEOUT:
            CONSOLE_PRINTLN(
                "HX711 runtime conversion timeout."
            );
            break;

        case APP_FAULT_HX711_READ:
            CONSOLE_PRINTLN(
                "HX711 conversion read failed."
            );
            break;

        case APP_FAULT_SAMPLE_COLLECTION_TIMEOUT:
            CONSOLE_PRINTLN(
                "HX711 sample collection timeout."
            );
            break;

        case APP_FAULT_SAMPLE_COLLECTION_STATE:
            CONSOLE_PRINTLN(
                "Invalid sample collection state."
            );
            break;

        case APP_FAULT_INVALID_ACTIVE_CALIBRATION:
            CONSOLE_PRINTLN(
                "Invalid active calibration factor."
            );
            break;

        case APP_FAULT_PERSISTENT_STORAGE_ACCESS:
            CONSOLE_PRINTLN(
                "Persistent storage access failed."
            );
            break;

        case APP_FAULT_INTERNAL_STATE:
        case APP_FAULT_NONE:
        default:
            CONSOLE_PRINTLN(
                "Invalid internal application state."
            );
            break;
    }
}


static void print_recovery_attempt_schedule(void)
{
    CONSOLE_PRINT("Recovery attempt ");
    console_print_int32(
        (int32_t)(fault_recovery_attempt_count + 1U)
    );
    CONSOLE_PRINT(" of ");
    console_print_int32(
        (int32_t)FAULT_RECOVERY_MAX_ATTEMPTS
    );
    CONSOLE_PRINT(" in ");
    console_print_int32(
        (int32_t)FAULT_RECOVERY_BACKOFF_MS
    );
    CONSOLE_PRINTLN(" ms.");
}


static void enter_terminal_fault_state(
    bool recovery_attempts_exhausted
)
{
    app_state = APP_STATE_TERMINAL_FAULT;

    operation_indicator_set_mode(
        OPERATION_INDICATOR_FAULT
    );

    if (recovery_attempts_exhausted)
    {
        print_fault_diagnostic();
        CONSOLE_PRINTLN(
            "Recovery attempts exhausted."
        );
    }

    CONSOLE_PRINTLN("Reset required.");
}


static void enter_fault_state(
    app_fault_code_t fault_code
)
{
    stop_diagnostic_capture();

    active_fault_code =
        app_fault_normalize_code(fault_code);

    app_fault_policy_t fault_policy =
        app_fault_get_policy(active_fault_code);

    if (fault_policy == APP_FAULT_POLICY_NONE)
    {
        active_fault_code =
            APP_FAULT_INTERNAL_STATE;

        fault_policy = APP_FAULT_POLICY_TERMINAL;
    }

    measurement_available = false;

    scale_cancel_sample_collection();
    level_indicator_reset();

    button_suppress_hold_until_release(
        &tare_button
    );

    button_suppress_hold_until_release(
        &calibration_button
    );

    console_discard_input();

    print_fault_diagnostic();

    CONSOLE_PRINTLN(
        "Normal level indication disabled."
    );

    fault_recovery_attempt_count = 0U;
    fault_recovery_phase_started_ms =
        hal_time_millis();

    if (fault_policy ==
        APP_FAULT_POLICY_RECOVER_SENSOR)
    {
        operation_indicator_set_mode(
            OPERATION_INDICATOR_RECOVERY
        );

        app_state =
            APP_STATE_FAULT_RECOVERY_BACKOFF;

        print_recovery_attempt_schedule();
        return;
    }

    enter_terminal_fault_state(false);
}


static operation_indicator_mode_t
get_operation_mode_for_idle_state(
    app_state_t idle_state
)
{
    if (idle_state == APP_STATE_NORMAL_OPERATION)
    {
        return OPERATION_INDICATOR_NONE;
    }

    return OPERATION_INDICATOR_TARE_REQUIRED;
}


static void start_error_result_pattern(
    app_state_t next_state,
    operation_indicator_mode_t next_mode
)
{
    state_after_result = next_state;
    app_state = APP_STATE_RESULT_PATTERN;

    operation_indicator_show_error(next_mode);
}


static void start_success_result_pattern(
    app_state_t next_state
)
{
    state_after_result = next_state;
    app_state = APP_STATE_RESULT_PATTERN;

    operation_indicator_show_success();
}


static void restore_tare_return_state(void)
{
    if (tare_return_state ==
        APP_STATE_NORMAL_OPERATION)
    {
        enter_normal_operation_state();
    }
    else
    {
        app_state = tare_return_state;
    }

    const operation_indicator_mode_t return_mode =
        get_operation_mode_for_idle_state(
            tare_return_state
        );

    if (return_mode == OPERATION_INDICATOR_NONE)
    {
        operation_indicator_clear();
        return;
    }

    operation_indicator_set_mode(return_mode);
}


static void finish_tare_with_error(void)
{
    scale_cancel_sample_collection();

    measurement_available = false;
    level_indicator_reset();

    start_error_result_pattern(
        tare_return_state,
        get_operation_mode_for_idle_state(
            tare_return_state
        )
    );

    /*
     * Input received while the bounded failure handling
     * was running must not become an idle-state action.
     */
    button_suppress_hold_until_release(
        &tare_button
    );

    button_suppress_hold_until_release(
        &calibration_button
    );

    console_discard_input();
}


static void cancel_tare(void)
{
    scale_cancel_sample_collection();

    measurement_available = false;
    level_indicator_reset();

    restore_tare_return_state();

    console_discard_input();

    console_newline();
    CONSOLE_PRINTLN("Tare cancelled.");

    CONSOLE_PRINTLN(
        "The previous tare offset remains active."
    );

    console_newline();
}


static void start_tare(void)
{
    stop_diagnostic_capture();

    tare_return_state =
        get_idle_application_state();

    measurement_available = false;
    level_indicator_reset();

    console_newline();
    CONSOLE_PRINTLN("Taring...");
    CONSOLE_PRINTLN(
        "Leave only the empty platform or container."
    );

    CONSOLE_PRINTLN(
        "Press TARE again or send 'q' to cancel."
    );

    if (!scale_start_sample_collection(
            TARE_SAMPLES))
    {
        enter_fault_state(
            APP_FAULT_SAMPLE_COLLECTION_STATE
        );
        return;
    }

    operation_started_ms = hal_time_millis();

    operation_indicator_set_mode(
        OPERATION_INDICATOR_TARE
    );

    app_state = APP_STATE_TARE_SAMPLING;
}


static bool process_tare_sampling_input(void)
{
    const bool tare_pressed =
        button_was_pressed(&tare_button);

    /*
     * Suppress both a debounced press and a candidate
     * press that has not completed its debounce interval.
     * The initiating D4 hold also remains harmless until
     * the user releases it.
     */
    button_suppress_hold_until_release(
        &tare_button
    );

    (void)button_was_pressed(
        &calibration_button
    );

    button_suppress_hold_until_release(
        &calibration_button
    );

    if (tare_pressed)
    {
        cancel_tare();
        return true;
    }

    if (!console_input_available())
    {
        return false;
    }

    char command = '\0';
    const bool command_read =
        console_read_char(&command);

    console_discard_input();

    if (!command_read)
    {
        return false;
    }

    if ((command == 'q') ||
        (command == 'Q'))
    {
        cancel_tare();
        return true;
    }

    CONSOLE_PRINTLN(
        "Tare is in progress. Send 'q' to cancel."
    );

    return false;
}


static void complete_tare(
    int32_t candidate_tare_offset
)
{
    /*
     * Persist and verify the candidate before changing
     * the active runtime offset. A failed save therefore
     * needs no RAM rollback.
     */
    if (!tare_storage_save(
            candidate_tare_offset))
    {
        CONSOLE_PRINTLN(
            "ERROR: New tare offset could not be saved."
        );

        CONSOLE_PRINTLN(
            "The previous tare offset remains active."
        );

        finish_tare_with_error();
        console_newline();
        return;
    }

    scale_set_offset(candidate_tare_offset);

    tare_available = true;
    enter_normal_operation_state();

    measurement_available = false;
    level_indicator_reset();

    operation_indicator_clear();

    /*
     * Do not reinterpret input received during the
     * bounded EEPROM save and verification step.
     */
    button_suppress_hold_until_release(
        &tare_button
    );

    button_suppress_hold_until_release(
        &calibration_button
    );

    console_discard_input();

    CONSOLE_PRINT("New tare offset: ");
    console_print_int32(candidate_tare_offset);
    console_newline();

    CONSOLE_PRINTLN("Tare completed and saved.");
    console_newline();
}


static void update_tare_sampling(void)
{
    if (process_tare_sampling_input())
    {
        return;
    }

    const scale_sample_collection_status_t status =
        scale_update_sample_collection();

    if (status == SCALE_SAMPLE_COLLECTION_COMPLETE)
    {
        int32_t candidate_tare_offset = 0;

        if (!scale_take_sample_average(
                &candidate_tare_offset))
        {
            enter_fault_state(
                APP_FAULT_SAMPLE_COLLECTION_STATE
            );
            return;
        }

        complete_tare(candidate_tare_offset);
        return;
    }

    if (status == SCALE_SAMPLE_COLLECTION_ERROR)
    {
        enter_fault_state(
            APP_FAULT_HX711_READ
        );
        return;
    }

    if (status != SCALE_SAMPLE_COLLECTION_IN_PROGRESS)
    {
        enter_fault_state(
            APP_FAULT_SAMPLE_COLLECTION_STATE
        );
        return;
    }

    const uint32_t now = hal_time_millis();

    if ((uint32_t)(now - operation_started_ms) <
        SCALE_SAMPLE_COLLECTION_TIMEOUT_MS)
    {
        return;
    }

    enter_fault_state(
        APP_FAULT_SAMPLE_COLLECTION_TIMEOUT
    );
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


static void clear_stored_tare(void)
{
    console_newline();

    if (!tare_storage_clear())
    {
        CONSOLE_PRINTLN(
            "ERROR: Stored tare offset could not be cleared."
        );

        console_newline();
        return;
    }

    stop_diagnostic_capture();

    /*
     * A persisted tare no longer exists, so normal
     * measurement must stop immediately.
     *
     * The offset currently held by scale remains in RAM,
     * but it is deliberately ignored until a new tare is
     * measured and saved.
     */
    tare_available = false;
    app_state = APP_STATE_TARE_REQUIRED;
    measurement_available = false;

    level_indicator_reset();

    operation_indicator_set_mode(
        OPERATION_INDICATOR_TARE_REQUIRED
    );

    CONSOLE_PRINTLN(
        "Stored tare offset cleared."
    );

    CONSOLE_PRINTLN(
        "Normal level indication is disabled."
    );

    CONSOLE_PRINTLN(
        "Place the empty container on the platform."
    );

    CONSOLE_PRINTLN(
        "Hold TARE for 3 seconds or send 't' to establish zero."
    );

    console_newline();
}


static bool calibration_is_active(void)
{
    return
        (app_state ==
            APP_STATE_CALIBRATION_WAITING_FOR_ZERO) ||
        (app_state ==
            APP_STATE_CALIBRATION_ZERO_SAMPLING) ||
        (app_state ==
            APP_STATE_CALIBRATION_WAITING_FOR_MASS) ||
        (app_state ==
            APP_STATE_CALIBRATION_MASS_SAMPLING);
}


static void start_calibration(void)
{
    stop_diagnostic_capture();

    measurement_available = false;
    level_indicator_reset();

    app_state =
        APP_STATE_CALIBRATION_WAITING_FOR_ZERO;

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

    scale_cancel_sample_collection();

    measurement_available = false;

    level_indicator_reset();
    restore_idle_application_state();

    button_suppress_hold_until_release(
        &tare_button
    );

    button_suppress_hold_until_release(
        &calibration_button
    );

    console_discard_input();

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


static bool process_calibration_sampling_input(void)
{
    const bool tare_pressed =
        button_was_pressed(&tare_button);

    button_suppress_hold_until_release(
        &tare_button
    );

    (void)button_was_pressed(
        &calibration_button
    );

    button_suppress_hold_until_release(
        &calibration_button
    );

    if (tare_pressed)
    {
        cancel_calibration();
        return true;
    }

    if (!console_input_available())
    {
        return false;
    }

    char command = '\0';
    const bool command_read =
        console_read_char(&command);

    console_discard_input();

    if (!command_read)
    {
        return false;
    }

    if ((command == 'q') ||
        (command == 'Q'))
    {
        cancel_calibration();
        return true;
    }

    CONSOLE_PRINTLN(
        "Calibration sampling is in progress. "
        "Send 'q' to cancel."
    );

    return false;
}


static void finish_calibration_sampling_with_error(
    app_state_t retry_state,
    operation_indicator_mode_t retry_mode
)
{
    scale_cancel_sample_collection();

    measurement_available = false;
    level_indicator_reset();

    start_error_result_pattern(
        retry_state,
        retry_mode
    );

    button_suppress_hold_until_release(
        &tare_button
    );

    button_suppress_hold_until_release(
        &calibration_button
    );

    console_discard_input();
}


static void start_calibration_zero_sampling(void)
{
    console_newline();
    CONSOLE_PRINTLN(
        "Collecting calibration tare samples..."
    );

    CONSOLE_PRINTLN(
        "Press TARE or send 'q' to cancel calibration."
    );

    if (!scale_start_sample_collection(
            TARE_SAMPLES))
    {
        enter_fault_state(
            APP_FAULT_SAMPLE_COLLECTION_STATE
        );
        return;
    }

    operation_started_ms = hal_time_millis();

    operation_indicator_set_mode(
        OPERATION_INDICATOR_TARE
    );

    app_state =
        APP_STATE_CALIBRATION_ZERO_SAMPLING;
}


static void complete_calibration_zero(
    int32_t candidate_tare_offset
)
{
    /*
     * Save and verify the candidate before changing the
     * active offset. Failure therefore leaves both the
     * previous runtime offset and tare availability
     * untouched.
     */
    if (!tare_storage_save(
            candidate_tare_offset))
    {
        CONSOLE_PRINTLN(
            "ERROR: Calibration tare could not be saved."
        );

        CONSOLE_PRINTLN(
            "The previous tare offset remains active."
        );

        CONSOLE_PRINTLN(
            "Keep the empty container in place "
            "and confirm again."
        );

        finish_calibration_sampling_with_error(
            APP_STATE_CALIBRATION_WAITING_FOR_ZERO,
            OPERATION_INDICATOR_CALIBRATION_ZERO
        );

        console_newline();
        return;
    }

    scale_set_offset(candidate_tare_offset);
    tare_available = true;

    measurement_available = false;
    level_indicator_reset();

    app_state =
        APP_STATE_CALIBRATION_WAITING_FOR_MASS;

    operation_indicator_set_mode(
        OPERATION_INDICATOR_CALIBRATION_MASS
    );

    button_suppress_hold_until_release(
        &tare_button
    );

    button_suppress_hold_until_release(
        &calibration_button
    );

    console_discard_input();

    CONSOLE_PRINT("Tare offset: ");
    console_print_int32(
        candidate_tare_offset
    );
    console_newline();

    CONSOLE_PRINTLN(
        "Calibration tare saved successfully."
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
}


static void update_calibration_zero_sampling(void)
{
    if (process_calibration_sampling_input())
    {
        return;
    }

    const scale_sample_collection_status_t status =
        scale_update_sample_collection();

    if (status == SCALE_SAMPLE_COLLECTION_COMPLETE)
    {
        int32_t candidate_tare_offset = 0;

        if (!scale_take_sample_average(
                &candidate_tare_offset))
        {
            enter_fault_state(
                APP_FAULT_SAMPLE_COLLECTION_STATE
            );
            return;
        }

        complete_calibration_zero(
            candidate_tare_offset
        );
        return;
    }

    if (status == SCALE_SAMPLE_COLLECTION_ERROR)
    {
        enter_fault_state(
            APP_FAULT_HX711_READ
        );
        return;
    }

    if (status != SCALE_SAMPLE_COLLECTION_IN_PROGRESS)
    {
        enter_fault_state(
            APP_FAULT_SAMPLE_COLLECTION_STATE
        );
        return;
    }

    const uint32_t now = hal_time_millis();

    if ((uint32_t)(now - operation_started_ms) <
        SCALE_SAMPLE_COLLECTION_TIMEOUT_MS)
    {
        return;
    }

    enter_fault_state(
        APP_FAULT_SAMPLE_COLLECTION_TIMEOUT
    );
}


static void start_calibration_mass_sampling(void)
{
    console_newline();
    CONSOLE_PRINTLN(
        "Collecting calibration samples..."
    );

    CONSOLE_PRINTLN(
        "Press TARE or send 'q' to cancel calibration."
    );

    if (!scale_start_sample_collection(
            CALIBRATION_SAMPLES))
    {
        enter_fault_state(
            APP_FAULT_SAMPLE_COLLECTION_STATE
        );
        return;
    }

    operation_started_ms = hal_time_millis();

    operation_indicator_set_mode(
        OPERATION_INDICATOR_CALIBRATION_MASS
    );

    app_state =
        APP_STATE_CALIBRATION_MASS_SAMPLING;
}


static void complete_calibration_mass(
    int32_t average_raw_counts
)
{
    const float net_counts =
        (float)average_raw_counts -
        (float)scale_get_offset();

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

        finish_calibration_sampling_with_error(
            APP_STATE_CALIBRATION_WAITING_FOR_MASS,
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

        finish_calibration_sampling_with_error(
            APP_STATE_CALIBRATION_WAITING_FOR_MASS,
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

        measurement_available = false;

        level_indicator_reset();

        start_error_result_pattern(
            get_idle_application_state(),
            get_idle_operation_mode()
        );

        button_suppress_hold_until_release(
            &tare_button
        );

        button_suppress_hold_until_release(
            &calibration_button
        );

        console_discard_input();

        CONSOLE_PRINTLN(
            "Calibration cancelled."
        );

        console_newline();
        return;
    }

    measurement_available = false;

    level_indicator_reset();

    start_success_result_pattern(
        APP_STATE_NORMAL_OPERATION
    );

    button_suppress_hold_until_release(
        &tare_button
    );

    button_suppress_hold_until_release(
        &calibration_button
    );

    console_discard_input();

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


static void update_calibration_mass_sampling(void)
{
    if (process_calibration_sampling_input())
    {
        return;
    }

    const scale_sample_collection_status_t status =
        scale_update_sample_collection();

    if (status == SCALE_SAMPLE_COLLECTION_COMPLETE)
    {
        int32_t average_raw_counts = 0;

        if (!scale_take_sample_average(
                &average_raw_counts))
        {
            enter_fault_state(
                APP_FAULT_SAMPLE_COLLECTION_STATE
            );
            return;
        }

        complete_calibration_mass(
            average_raw_counts
        );
        return;
    }

    if (status == SCALE_SAMPLE_COLLECTION_ERROR)
    {
        enter_fault_state(
            APP_FAULT_HX711_READ
        );
        return;
    }

    if (status != SCALE_SAMPLE_COLLECTION_IN_PROGRESS)
    {
        enter_fault_state(
            APP_FAULT_SAMPLE_COLLECTION_STATE
        );
        return;
    }

    const uint32_t now = hal_time_millis();

    if ((uint32_t)(now - operation_started_ms) <
        SCALE_SAMPLE_COLLECTION_TIMEOUT_MS)
    {
        return;
    }

    enter_fault_state(
        APP_FAULT_SAMPLE_COLLECTION_TIMEOUT
    );
}


static void process_calibration_confirmation(void)
{
    switch (app_state)
    {
        case APP_STATE_TARE_REQUIRED:
        case APP_STATE_NORMAL_OPERATION:
            start_calibration();
            break;

        case APP_STATE_CALIBRATION_WAITING_FOR_ZERO:
            start_calibration_zero_sampling();
            break;

        case APP_STATE_CALIBRATION_WAITING_FOR_MASS:
            start_calibration_mass_sampling();
            break;

        default:
            CONSOLE_PRINTLN(
                "Calibration confirmation is unavailable "
                "in the current state."
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
                start_tare();
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

        case 'z':
        case 'Z':

            if (calibration_is_active())
            {
                CONSOLE_PRINTLN(
                    "Stored tare cannot be cleared "
                    "during calibration."
                );
            }
            else
            {
                clear_stored_tare();
            }

            break;

        case 'd':
        case 'D':
            toggle_diagnostic_capture();
            break;

        default:
            CONSOLE_PRINTLN("Unknown command.");
            CONSOLE_PRINTLN("Available commands:");
            CONSOLE_PRINTLN("  t = tare");
            CONSOLE_PRINTLN("  c = start/confirm calibration");
            CONSOLE_PRINTLN(
                "  q = cancel current tare/calibration"
            );
            CONSOLE_PRINTLN("  s = save active calibration");
            CONSOLE_PRINTLN("  x = clear stored calibration");
            CONSOLE_PRINTLN("  z = clear stored tare");
            CONSOLE_PRINTLN("  d = toggle diagnostic capture");
            break;
    }
}


static void update_weight_measurement(void)
{
    scale_measurement_t measurement;

    const scale_read_status_t read_status =
        scale_try_read_measurement(&measurement);

    if (read_status == SCALE_READ_NO_DATA)
    {
        const uint32_t now = hal_time_millis();

        if ((uint32_t)(
                now - scale_runtime_activity_ms) <
            SCALE_RUNTIME_READY_TIMEOUT_MS)
        {
            return;
        }

        enter_fault_state(
            APP_FAULT_HX711_RUNTIME_TIMEOUT
        );

        return;
    }

    if (read_status != SCALE_READ_VALUE)
    {
        enter_fault_state(
            APP_FAULT_HX711_READ
        );

        return;
    }

    latest_measurement = measurement;
    measurement_available = true;

    const uint32_t measurement_time_ms =
        hal_time_millis();

    scale_runtime_activity_ms = measurement_time_ms;

    level_indicator_update(
        latest_measurement.weight_grams
    );

    print_diagnostic_measurement(
        &latest_measurement,
        measurement_time_ms
    );
}


static void print_weight_periodically(void)
{
    if (diagnostic_capture_active)
    {
        return;
    }

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
        latest_measurement.weight_grams,
        2U
    );
    CONSOLE_PRINT(" g | Level: ");
    console_println(
        level_indicator_get_state_name()
    );
}


static void print_runtime_information(void)
{
    console_newline();
    CONSOLE_PRINTLN("Controls:");
    CONSOLE_PRINTLN(
        "  Hold button on D4 for 3 s = tare"
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
        "  Serial command 'z'    = clear tare"
    );
    CONSOLE_PRINTLN(
        "  Serial command 'c'    = calibrate/confirm"
    );

    CONSOLE_PRINTLN(
        "  Serial command 'q'    = cancel current tare/calibration"
    );

    CONSOLE_PRINTLN(
        "  Serial command 'd'    = toggle diagnostic capture"
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

    CONSOLE_PRINTLN(
        " HIGH blinking = reset-required fault"
    );

    CONSOLE_PRINTLN(
        " LOW/HIGH alternating = HX711 recovery"
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


static void consume_busy_buttons(void)
{
    /*
     * Sample both debouncers, but never let a press that
     * began in this state become a later hold event.
     *
     * Suppression is unconditional so a button already
     * held during initialization is covered as well.
     */
    (void)button_was_pressed(&tare_button);
    (void)button_was_pressed(&calibration_button);

    button_suppress_hold_until_release(
        &tare_button
    );

    button_suppress_hold_until_release(
        &calibration_button
    );
}


static void process_startup_input(void)
{
    consume_busy_buttons();

    if (!console_input_available())
    {
        return;
    }

    char ignored_command = '\0';
    const bool command_read =
        console_read_char(&ignored_command);

    console_discard_input();

    if (command_read)
    {
        CONSOLE_PRINTLN(
            "Startup is not complete."
        );
    }
}


static void process_recovery_input(void)
{
    consume_busy_buttons();

    if (!console_input_available())
    {
        return;
    }

    char ignored_command = '\0';
    const bool command_read =
        console_read_char(&ignored_command);

    console_discard_input();

    if (command_read)
    {
        CONSOLE_PRINTLN(
            "Recovery in progress."
        );
    }
}


static void process_terminal_fault_input(void)
{
    consume_busy_buttons();

    if (!console_input_available())
    {
        return;
    }

    char ignored_command = '\0';
    const bool command_read =
        console_read_char(&ignored_command);

    console_discard_input();

    if (command_read)
    {
        print_fault_diagnostic();
        CONSOLE_PRINTLN("Reset required.");
    }
}


static void finish_successful_scale_recovery(void)
{
    /*
     * Reserve the transition iteration and make every
     * input observed during recovery harmless in the
     * state selected below.
     */
    consume_busy_buttons();
    console_discard_input();

    fault_recovery_attempt_count = 0U;
    active_fault_code = APP_FAULT_NONE;

    operation_indicator_clear();

    CONSOLE_PRINTLN("HX711 recovery succeeded.");
    CONSOLE_PRINTLN(
        "Previous tare and calibration remain active."
    );

    if (!startup_configuration_loaded)
    {
        app_state =
            APP_STATE_STARTUP_LOAD_CONFIGURATION;

        CONSOLE_PRINTLN(
            "Startup configuration will now be loaded."
        );
        return;
    }

    restore_idle_application_state();

    if (tare_available)
    {
        CONSOLE_PRINTLN(
            "Normal measurement resumed."
        );
        return;
    }

    CONSOLE_PRINTLN(
        "Tare is still required before normal measurement."
    );
}


static void schedule_next_recovery_attempt(
    uint32_t now
)
{
    if (fault_recovery_attempt_count >=
        FAULT_RECOVERY_MAX_ATTEMPTS)
    {
        enter_terminal_fault_state(true);
        return;
    }

    app_state =
        APP_STATE_FAULT_RECOVERY_BACKOFF;

    fault_recovery_phase_started_ms = now;
    print_recovery_attempt_schedule();
}


static void update_fault_recovery_backoff(void)
{
    process_recovery_input();

    const uint32_t now = hal_time_millis();

    if ((uint32_t)(
            now - fault_recovery_phase_started_ms) <
        FAULT_RECOVERY_BACKOFF_MS)
    {
        return;
    }

    ++fault_recovery_attempt_count;

    if (!scale_recover())
    {
        schedule_next_recovery_attempt(now);
        return;
    }

    app_state =
        APP_STATE_FAULT_RECOVERY_WAIT_FOR_SCALE;

    fault_recovery_phase_started_ms = now;

    CONSOLE_PRINTLN(
        "HX711 power cycle completed. Waiting for a conversion."
    );
}


static void update_fault_recovery_wait_for_scale(void)
{
    process_recovery_input();

    if (scale_is_ready())
    {
        finish_successful_scale_recovery();
        return;
    }

    const uint32_t now = hal_time_millis();

    if ((uint32_t)(
            now - fault_recovery_phase_started_ms) <
        FAULT_RECOVERY_READY_TIMEOUT_MS)
    {
        return;
    }

    CONSOLE_PRINTLN(
        "HX711 did not become ready after recovery."
    );

    schedule_next_recovery_attempt(now);
}


static void process_result_pattern(void)
{
    /*
     * The result indication owns the shared LEDs and the
     * application state until completion. Sample both
     * buttons on every iteration and prevent any press
     * begun here from becoming a later hold action.
     */
    consume_busy_buttons();

    if (console_input_available())
    {
        char ignored_command = '\0';
        const bool command_read =
            console_read_char(&ignored_command);

        console_discard_input();

        if (command_read)
        {
            CONSOLE_PRINTLN(
                "Result indication is active."
            );
        }
    }

    if (operation_indicator_is_temporary_active())
    {
        return;
    }

    /*
     * Reserve this whole iteration for leaving the busy
     * state. Normal work begins on the next app_update(),
     * so input sampled at the completion boundary cannot
     * be reinterpreted under the restored state.
     */
    if (state_after_result ==
        APP_STATE_NORMAL_OPERATION)
    {
        enter_normal_operation_state();
        return;
    }

    app_state = state_after_result;
}


static void load_startup_configuration(void)
{
    float calibration_factor =
        DEFAULT_CALIBRATION_FACTOR;

    const storage_load_status_t
        calibration_load_status =
        calibration_storage_load(
            &calibration_factor
        );

    switch (calibration_load_status)
    {
        case STORAGE_LOAD_VALID:
            CONSOLE_PRINTLN(
                "Stored calibration loaded from EEPROM."
            );
            break;

        case STORAGE_LOAD_ABSENT:
            CONSOLE_PRINTLN(
                "No stored calibration found."
            );

            CONSOLE_PRINTLN(
                "Using default calibration factor."
            );
            break;

        case STORAGE_LOAD_INVALID:
            CONSOLE_PRINTLN(
                "Stored calibration is invalid or corrupt."
            );

            CONSOLE_PRINTLN(
                "Using default calibration factor."
            );
            break;

        case STORAGE_LOAD_ACCESS_ERROR:
            CONSOLE_PRINTLN(
                "ERROR: Calibration storage could not be read."
            );

            enter_fault_state(
                APP_FAULT_PERSISTENT_STORAGE_ACCESS
            );
            return;

        default:
            enter_fault_state(
                APP_FAULT_INTERNAL_STATE
            );
            return;
    }

    if (!scale_set_calibration_factor(
            calibration_factor))
    {
        enter_fault_state(
            APP_FAULT_INVALID_ACTIVE_CALIBRATION
        );
        return;
    }

    CONSOLE_PRINT("Calibration factor: ");
    console_print_float(
        scale_get_calibration_factor(),
        6U
    );
    CONSOLE_PRINTLN(" counts/g");

    int32_t stored_tare_offset = 0;

    const storage_load_status_t tare_load_status =
        tare_storage_load(
            &stored_tare_offset
        );

    if (tare_load_status ==
        STORAGE_LOAD_ACCESS_ERROR)
    {
        CONSOLE_PRINTLN(
            "ERROR: Tare storage could not be read."
        );

        enter_fault_state(
            APP_FAULT_PERSISTENT_STORAGE_ACCESS
        );
        return;
    }

    if ((tare_load_status != STORAGE_LOAD_VALID) &&
        (tare_load_status != STORAGE_LOAD_ABSENT) &&
        (tare_load_status != STORAGE_LOAD_INVALID))
    {
        enter_fault_state(
            APP_FAULT_INTERNAL_STATE
        );
        return;
    }

    if (tare_load_status == STORAGE_LOAD_VALID)
    {
        scale_set_offset(
            stored_tare_offset
        );

        /*
         * Do not carry input received during the bounded
         * configuration step into normal operation.
         */
        consume_busy_buttons();
        console_discard_input();

        tare_available = true;
        enter_normal_operation_state();

        operation_indicator_clear();

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
        /*
         * Do not reinterpret startup input as a tare or
         * calibration request after this transition.
         */
        consume_busy_buttons();
        console_discard_input();

        tare_available = false;
        app_state = APP_STATE_TARE_REQUIRED;

        measurement_available = false;
        level_indicator_reset();

        operation_indicator_set_mode(
            OPERATION_INDICATOR_TARE_REQUIRED
        );

        if (tare_load_status == STORAGE_LOAD_ABSENT)
        {
            CONSOLE_PRINTLN(
                "No stored tare offset found."
            );
        }
        else
        {
            CONSOLE_PRINTLN(
                "Stored tare offset is invalid or corrupt."
            );
        }

        CONSOLE_PRINTLN(
            "Normal level indication is disabled."
        );

        CONSOLE_PRINTLN(
            "Place the empty container on the platform."
        );

        CONSOLE_PRINTLN(
            "Hold TARE for 3 seconds or send 't' to establish zero."
        );
    }

    startup_configuration_loaded = true;

    print_runtime_information();
}


static void update_startup_wait_for_scale(void)
{
    process_startup_input();

    if (scale_is_ready())
    {
        app_state =
            APP_STATE_STARTUP_LOAD_CONFIGURATION;

        return;
    }

    const uint32_t now = hal_time_millis();

    if ((uint32_t)(now - operation_started_ms) <
        SCALE_STARTUP_TIMEOUT_MS)
    {
        return;
    }

    enter_fault_state(
        APP_FAULT_HX711_STARTUP_TIMEOUT
    );
}


static bool process_cooperative_base_state(void)
{
    switch (app_state)
    {
        case APP_STATE_STARTUP_WAIT_FOR_SCALE:
            update_startup_wait_for_scale();
            return true;

        case APP_STATE_STARTUP_LOAD_CONFIGURATION:
            process_startup_input();
            load_startup_configuration();
            return true;

        case APP_STATE_FAULT_RECOVERY_BACKOFF:
            update_fault_recovery_backoff();
            return true;

        case APP_STATE_FAULT_RECOVERY_WAIT_FOR_SCALE:
            update_fault_recovery_wait_for_scale();
            return true;

        case APP_STATE_TERMINAL_FAULT:
            process_terminal_fault_input();
            return true;

        case APP_STATE_TARE_SAMPLING:
            update_tare_sampling();
            return true;

        case APP_STATE_CALIBRATION_ZERO_SAMPLING:
            update_calibration_zero_sampling();
            return true;

        case APP_STATE_CALIBRATION_MASS_SAMPLING:
            update_calibration_mass_sampling();
            return true;

        case APP_STATE_RESULT_PATTERN:
            process_result_pattern();
            return true;

        case APP_STATE_TARE_REQUIRED:
        case APP_STATE_NORMAL_OPERATION:
        case APP_STATE_CALIBRATION_WAITING_FOR_ZERO:
        case APP_STATE_CALIBRATION_WAITING_FOR_MASS:
            return false;

        default:
            enter_fault_state(
                APP_FAULT_INTERNAL_STATE
            );
            return true;
    }
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

    latest_measurement.raw_counts = 0;
    latest_measurement.net_counts = 0;
    latest_measurement.weight_grams = 0.0F;
    measurement_available = false;
    tare_available = false;
    active_fault_code = APP_FAULT_NONE;
    fault_recovery_attempt_count = 0U;
    fault_recovery_phase_started_ms = 0UL;
    startup_configuration_loaded = false;
    scale_runtime_activity_ms = 0UL;

    last_print_ms = 0UL;
    diagnostic_capture_active = false;
    diagnostic_sequence = 0UL;
    operation_started_ms = hal_time_millis();

    tare_return_state =
        APP_STATE_TARE_REQUIRED;

    state_after_result =
        APP_STATE_TARE_REQUIRED;

    app_state = APP_STATE_STARTUP_WAIT_FOR_SCALE;

    console_newline();
    CONSOLE_PRINTLN(
        "=== Load cell level indicator ==="
    );

    print_reset_causes();

    if (!scale_init())
    {
        enter_fault_state(
            APP_FAULT_HX711_INITIALIZATION
        );
        return;
    }

    operation_started_ms = hal_time_millis();

    CONSOLE_PRINTLN(
        "Waiting for the first HX711 conversion..."
    );
}


void app_update(void)
{
    /*
     * Update the current visual pattern first.
     */
    operation_indicator_update();

    /*
     * Startup and fault handling keep the superloop
     * cooperative but do not permit operational work.
     */
    if (process_cooperative_base_state())
    {
        return;
    }

    process_console_commands();

    if (fault_handling_is_active())
    {
        return;
    }

    /*
     * A serial command may have started an incremental
     * tare or calibration collection in this iteration.
     */
    if ((app_state == APP_STATE_TARE_SAMPLING) ||
        (app_state ==
            APP_STATE_CALIBRATION_ZERO_SAMPLING) ||
        (app_state ==
            APP_STATE_CALIBRATION_MASS_SAMPLING) ||
        (app_state == APP_STATE_RESULT_PATTERN))
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
            /*
             * The press cancels calibration immediately.
             * It must not later become a three-second
             * tare event if the user keeps holding D4.
             */
            button_suppress_hold_until_release(
                &tare_button
            );

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

    if (fault_handling_is_active())
    {
        return;
    }

    if (calibration_is_active())
    {
        return;
    }

    /*
     * Outside calibration, a deliberate long press is
     * required before the physical TARE button can
     * redefine the operational zero.
     *
     * Short presses are intentionally ignored.
     * The serial 't' command remains an immediate
     * service operation.
     */
    if (button_was_held(
            &tare_button,
            TARE_START_HOLD_MS))
    {
        start_tare();

        if (app_state == APP_STATE_RESULT_PATTERN)
        {
            return;
        }

        if (app_state == APP_STATE_TARE_SAMPLING)
        {
            return;
        }

        if (fault_handling_is_active())
        {
            return;
        }
    }

    if (!tare_available)
    {
        measurement_available = false;
        return;
    }

    update_weight_measurement();

    if (fault_handling_is_active())
    {
        return;
    }

    /*
    * Update normal level visual effects.
    *
    * This point is only reached when no calibration or
    * temporary operation pattern owns the LEDs.
    */
    level_indicator_update_visual();

    print_weight_periodically();
}
