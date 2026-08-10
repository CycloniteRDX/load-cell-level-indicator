#include <stdint.h>
#include <string.h>
#include <unity.h>

#include "app.h"
#include "config.h"
#include "fake_app_support.h"


void setUp(void)
{
    fake_app_reset();
}


void tearDown(void)
{
}


static void assert_console_contains(
    const char *expected_text
)
{
    TEST_ASSERT_NOT_NULL(
        strstr(
            fake_app_console_output(),
            expected_text
        )
    );
}


static void reach_configuration_state(void)
{
    app_init();

    fake_app_set_scale_ready(true);
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_calibration_load_call_count()
    );
}


static void load_configuration(void)
{
    reach_configuration_state();
    app_update();
}


static void load_configuration_with_tare(
    int32_t tare_offset
)
{
    fake_app_set_tare_record(
        true,
        tare_offset
    );

    load_configuration();
}


static void start_serial_tare(void)
{
    fake_app_queue_console_command('t');
    app_update();
}


static void start_calibration_waiting_for_zero(void)
{
    fake_app_queue_console_command('c');
    app_update();
}


static void start_calibration_zero_sampling(void)
{
    start_calibration_waiting_for_zero();

    fake_app_queue_console_command('c');
    app_update();
}


static void complete_calibration_zero(
    int32_t tare_offset
)
{
    fake_app_set_scale_sample_average(
        true,
        tare_offset
    );

    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_COMPLETE
    );

    app_update();
}


static void reach_calibration_mass_wait(
    int32_t tare_offset
)
{
    start_calibration_zero_sampling();
    complete_calibration_zero(tare_offset);
}


static void start_calibration_mass_sampling(
    int32_t tare_offset
)
{
    reach_calibration_mass_wait(tare_offset);

    fake_app_queue_console_command('c');
    app_update();
}


static void enter_startup_timeout_recovery(void)
{
    app_init();

    fake_app_advance_time_ms(
        SCALE_STARTUP_TIMEOUT_MS
    );

    app_update();
}


static void enter_runtime_read_recovery_with_tare(
    int32_t tare_offset
)
{
    load_configuration_with_tare(tare_offset);

    fake_app_set_scale_ready(false);
    fake_app_set_scale_read_status(
        SCALE_READ_ERROR
    );

    app_update();
}


static void start_recovery_power_cycle(void)
{
    fake_app_advance_time_ms(
        FAULT_RECOVERY_BACKOFF_MS
    );

    app_update();
}


static void test_init_configures_scale_without_polling_or_loading_storage(void)
{
    app_init();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_init_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_calibration_load_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_tare_load_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "Waiting for the first HX711 conversion..."
    );
}


static void test_immediate_scale_initialization_failure_enters_fault(void)
{
    fake_app_set_scale_init_result(false);

    app_init();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_FAULT,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_level_reset_call_count()
    );

    assert_console_contains(
        "FAULT 01: HX711 initialization failed."
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_operation_indicator_update_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_ready_call_count()
    );
}


static void test_startup_wait_returns_without_loading_configuration(void)
{
    app_init();

    fake_app_set_time_ms(
        SCALE_STARTUP_TIMEOUT_MS - 1UL
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_calibration_load_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_operation_indicator_update_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );
}


static void test_startup_timeout_enters_recovery_at_exact_deadline(void)
{
    app_init();

    fake_app_set_time_ms(
        SCALE_STARTUP_TIMEOUT_MS
    );

    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_calibration_load_call_count()
    );

    assert_console_contains(
        "FAULT 02: HX711 startup conversion timeout."
    );
}


static void test_ready_scale_wins_when_detected_at_timeout_deadline(void)
{
    app_init();

    fake_app_set_scale_ready(true);
    fake_app_set_time_ms(
        SCALE_STARTUP_TIMEOUT_MS
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_calibration_load_call_count()
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_calibration_load_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_TARE_REQUIRED,
        fake_app_operation_indicator_mode()
    );
}


static void test_startup_timeout_handles_millisecond_overflow(void)
{
    fake_app_set_time_ms(
        UINT32_MAX - 500UL
    );

    app_init();

    fake_app_advance_time_ms(
        SCALE_STARTUP_TIMEOUT_MS - 1UL
    );

    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    fake_app_advance_time_ms(1UL);
    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );
}


static void test_valid_stored_configuration_is_loaded_once(void)
{
    fake_app_set_calibration_record(
        true,
        50.25F
    );

    fake_app_set_tare_record(
        true,
        -172706
    );

    load_configuration();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_calibration_load_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_tare_load_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_factor_set_call_count()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        50.25F,
        fake_app_last_scale_factor()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_offset_set_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_weight_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_calibration_load_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_tare_load_call_count()
    );
}


static void test_invalid_calibration_uses_default_with_corruption_diagnostic(void)
{
    fake_app_set_calibration_load_status(
        STORAGE_LOAD_INVALID
    );

    load_configuration();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_factor_set_call_count()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        DEFAULT_CALIBRATION_FACTOR,
        fake_app_last_scale_factor()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_TARE_REQUIRED,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "Stored calibration is invalid or corrupt."
    );

    assert_console_contains(
        "Using default calibration factor."
    );
}


static void test_calibration_storage_access_error_enters_terminal_fault(void)
{
    fake_app_set_calibration_load_status(
        STORAGE_LOAD_ACCESS_ERROR
    );

    load_configuration();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_calibration_load_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_tare_load_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_factor_set_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_FAULT,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "ERROR: Calibration storage could not be read."
    );

    assert_console_contains(
        "FAULT 09: Persistent storage access failed."
    );
}


static void test_runtime_read_error_enters_recovery_with_stable_code(void)
{
    load_configuration_with_tare(-172706);

    fake_app_set_scale_read_status(
        SCALE_READ_ERROR
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_weight_read_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    assert_console_contains(
        "FAULT 04: HX711 conversion read failed."
    );
}


static void test_runtime_no_data_is_tolerated_before_timeout(void)
{
    load_configuration_with_tare(-172706);

    fake_app_set_scale_read_status(
        SCALE_READ_NO_DATA
    );

    fake_app_advance_time_ms(
        SCALE_RUNTIME_READY_TIMEOUT_MS - 1UL
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_weight_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_recover_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );
}


static void test_runtime_no_data_enters_recovery_at_exact_timeout(void)
{
    load_configuration_with_tare(-172706);

    fake_app_set_scale_read_status(
        SCALE_READ_NO_DATA
    );

    fake_app_advance_time_ms(
        SCALE_RUNTIME_READY_TIMEOUT_MS
    );

    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_recover_call_count()
    );

    assert_console_contains(
        "FAULT 03: HX711 runtime conversion timeout."
    );
}


static void test_runtime_value_wins_at_timeout_and_renews_deadline(void)
{
    load_configuration_with_tare(-172706);

    fake_app_set_scale_read_status(
        SCALE_READ_VALUE
    );

    fake_app_advance_time_ms(
        SCALE_RUNTIME_READY_TIMEOUT_MS
    );

    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    fake_app_set_scale_read_status(
        SCALE_READ_NO_DATA
    );

    fake_app_advance_time_ms(
        SCALE_RUNTIME_READY_TIMEOUT_MS - 1UL
    );

    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    fake_app_advance_time_ms(1UL);
    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_UINT32(
        3UL,
        fake_app_scale_weight_read_call_count()
    );

    assert_console_contains(
        "FAULT 03: HX711 runtime conversion timeout."
    );
}


static void test_runtime_no_data_timeout_handles_millisecond_overflow(void)
{
    fake_app_set_time_ms(
        UINT32_MAX - 1000UL
    );

    load_configuration_with_tare(-172706);

    fake_app_set_scale_read_status(
        SCALE_READ_NO_DATA
    );

    fake_app_advance_time_ms(
        SCALE_RUNTIME_READY_TIMEOUT_MS - 1UL
    );

    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    fake_app_advance_time_ms(1UL);
    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "FAULT 03: HX711 runtime conversion timeout."
    );
}


static void test_runtime_supervision_restarts_after_recovery(void)
{
    enter_runtime_read_recovery_with_tare(-172706);

    start_recovery_power_cycle();

    fake_app_set_scale_ready(true);
    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    fake_app_set_scale_ready(false);
    fake_app_set_scale_read_status(
        SCALE_READ_NO_DATA
    );

    fake_app_advance_time_ms(
        SCALE_RUNTIME_READY_TIMEOUT_MS - 1UL
    );

    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    fake_app_advance_time_ms(1UL);
    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "FAULT 03: HX711 runtime conversion timeout."
    );
}


static void test_runtime_timeout_is_inactive_during_calibration_wait(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_waiting_for_zero();

    fake_app_advance_time_ms(
        SCALE_RUNTIME_READY_TIMEOUT_MS
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_recover_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_CALIBRATION_ZERO,
        fake_app_operation_indicator_mode()
    );
}


static void test_missing_tare_selects_tare_required_and_disables_measurement(void)
{
    load_configuration();

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        DEFAULT_CALIBRATION_FACTOR,
        fake_app_last_scale_factor()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_TARE_REQUIRED,
        fake_app_operation_indicator_mode()
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );

    assert_console_contains(
        "Normal level indication is disabled."
    );

    assert_console_contains(
        "No stored calibration found."
    );

    assert_console_contains(
        "No stored tare offset found."
    );
}


static void test_invalid_tare_selects_tare_required_with_corruption_diagnostic(void)
{
    fake_app_set_tare_load_status(
        STORAGE_LOAD_INVALID
    );

    load_configuration();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_offset_set_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_TARE_REQUIRED,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "Stored tare offset is invalid or corrupt."
    );

    assert_console_contains(
        "Normal level indication is disabled."
    );
}


static void test_tare_storage_access_error_enters_terminal_fault(void)
{
    fake_app_set_tare_load_status(
        STORAGE_LOAD_ACCESS_ERROR
    );

    load_configuration();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_factor_set_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_tare_load_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_offset_set_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_FAULT,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "ERROR: Tare storage could not be read."
    );

    assert_console_contains(
        "FAULT 09: Persistent storage access failed."
    );
}


static void test_invalid_active_calibration_factor_enters_fault_before_tare_load(void)
{
    fake_app_set_calibration_record(
        true,
        0.0F
    );

    fake_app_set_scale_factor_result(false);

    load_configuration();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_calibration_load_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_tare_load_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_FAULT,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    assert_console_contains(
        "FAULT 07: Invalid active calibration factor."
    );
}


static void test_startup_commands_and_presses_are_consumed(void)
{
    app_init();

    fake_app_queue_console_command('t');
    fake_app_press_tare_button();
    fake_app_press_calibration_button();

    app_update();

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_tare_button_suppression_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_calibration_button_suppression_count()
    );

    assert_console_contains(
        "Startup is not complete."
    );
}


static void test_input_received_during_configuration_is_not_executed_later(void)
{
    fake_app_set_tare_record(
        true,
        -172706
    );

    fake_app_queue_console_command_during_tare_load(
        't'
    );

    load_configuration();

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    app_update();
}


static void test_terminal_fault_is_latched_and_rejects_commands(void)
{
    fake_app_set_scale_init_result(false);

    app_init();

    fake_app_queue_console_command('t');

    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_FAULT,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_recover_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_calibration_load_call_count()
    );

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    assert_console_contains(
        "FAULT 01: HX711 initialization failed."
    );

    assert_console_contains(
        "Reset required."
    );
}


static void test_recovery_backoff_is_exact_and_handles_millisecond_overflow(void)
{
    fake_app_set_time_ms(UINT32_MAX - 250UL);

    enter_startup_timeout_recovery();

    fake_app_advance_time_ms(
        FAULT_RECOVERY_BACKOFF_MS - 1UL
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_recover_call_count()
    );

    fake_app_advance_time_ms(1UL);
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_recover_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_ready_call_count()
    );
}


static void test_failed_power_cycles_exhaust_exactly_three_attempts(void)
{
    enter_runtime_read_recovery_with_tare(-172706);

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    fake_app_set_scale_recover_result(false);

    for (uint8_t attempt = 0U;
         attempt < FAULT_RECOVERY_MAX_ATTEMPTS;
         ++attempt)
    {
        start_recovery_power_cycle();

        TEST_ASSERT_EQUAL_UINT32(
            (uint32_t)attempt + 1UL,
            fake_app_scale_recover_call_count()
        );
    }

    assert_console_contains(
        "Recovery attempts exhausted."
    );

    assert_console_contains("Reset required.");

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_FAULT,
        fake_app_operation_indicator_mode()
    );

    fake_app_advance_time_ms(
        FAULT_RECOVERY_BACKOFF_MS
    );
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        FAULT_RECOVERY_MAX_ATTEMPTS,
        fake_app_scale_recover_call_count()
    );
}


static void test_ready_conversion_wins_at_recovery_timeout_deadline(void)
{
    enter_runtime_read_recovery_with_tare(-172706);
    start_recovery_power_cycle();

    fake_app_set_scale_ready(true);
    fake_app_advance_time_ms(
        FAULT_RECOVERY_READY_TIMEOUT_MS
    );

    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_NULL(
        strstr(
            fake_app_console_output(),
            "Recovery attempts exhausted."
        )
    );

    assert_console_contains(
        "HX711 recovery succeeded."
    );
}


static void test_recovery_ready_timeout_schedules_next_attempt_across_overflow(void)
{
    fake_app_set_time_ms(UINT32_MAX - 250UL);

    enter_runtime_read_recovery_with_tare(-172706);
    start_recovery_power_cycle();

    fake_app_advance_time_ms(
        FAULT_RECOVERY_READY_TIMEOUT_MS - 1UL
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_recover_call_count()
    );

    fake_app_advance_time_ms(1UL);
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_recover_call_count()
    );

    assert_console_contains(
        "Recovery attempt 2 of 3 in 500 ms."
    );

    start_recovery_power_cycle();

    TEST_ASSERT_EQUAL_UINT32(
        2UL,
        fake_app_scale_recover_call_count()
    );
}


static void test_startup_recovery_loads_configuration_on_following_update(void)
{
    fake_app_set_calibration_record(true, 50.25F);
    fake_app_set_tare_record(true, -172706);

    enter_startup_timeout_recovery();
    start_recovery_power_cycle();

    fake_app_set_scale_ready(true);
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_calibration_load_call_count()
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_calibration_load_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_tare_load_call_count()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        50.25F,
        fake_app_last_scale_factor()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );
}


static void test_runtime_recovery_returns_to_normal_on_following_update(void)
{
    fake_app_set_calibration_record(true, 50.25F);
    enter_runtime_read_recovery_with_tare(-172706);

    start_recovery_power_cycle();
    fake_app_set_scale_ready(true);
    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        50.25F,
        fake_app_last_scale_factor()
    );

    const uint32_t reads_before =
        fake_app_scale_weight_read_call_count();

    fake_app_set_scale_read_status(
        SCALE_READ_NO_DATA
    );
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        reads_before + 1UL,
        fake_app_scale_weight_read_call_count()
    );
}


static void test_recovery_without_valid_tare_returns_to_tare_required(void)
{
    load_configuration();
    start_serial_tare();

    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_ERROR
    );
    app_update();

    start_recovery_power_cycle();
    fake_app_set_scale_ready(true);
    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_TARE_REQUIRED,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );
}


static void test_recovery_discards_input_and_suppresses_button_holds(void)
{
    enter_runtime_read_recovery_with_tare(-172706);

    const uint32_t tare_suppressions_before =
        fake_app_tare_button_suppression_count();

    const uint32_t calibration_suppressions_before =
        fake_app_calibration_button_suppression_count();

    fake_app_queue_console_command('t');
    fake_app_hold_tare_button();
    fake_app_hold_calibration_button();

    app_update();

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    TEST_ASSERT_EQUAL_UINT32(
        tare_suppressions_before + 1UL,
        fake_app_tare_button_suppression_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        calibration_suppressions_before + 1UL,
        fake_app_calibration_button_suppression_count()
    );

    assert_console_contains("Recovery in progress.");

    start_recovery_power_cycle();

    fake_app_set_scale_ready(true);
    fake_app_queue_console_command('c');
    fake_app_hold_tare_button();
    fake_app_hold_calibration_button();
    app_update();

    const uint32_t collections_before =
        fake_app_scale_collection_start_call_count();

    fake_app_set_scale_read_status(
        SCALE_READ_NO_DATA
    );
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        collections_before,
        fake_app_scale_collection_start_call_count()
    );
}


static void test_recovery_cancels_tare_and_preserves_committed_values(void)
{
    fake_app_set_calibration_record(true, 50.25F);
    load_configuration_with_tare(-172706);
    start_serial_tare();

    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_ERROR
    );
    app_update();

    start_recovery_power_cycle();
    fake_app_set_scale_ready(true);
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_tare_save_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        50.25F,
        fake_app_last_scale_factor()
    );

    fake_app_set_scale_read_status(
        SCALE_READ_NO_DATA
    );
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_collection_start_call_count()
    );
}


static void test_recovery_cancels_calibration_and_returns_to_normal(void)
{
    fake_app_set_calibration_record(true, 50.25F);
    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-172802);

    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_ERROR
    );
    app_update();

    start_recovery_power_cycle();
    fake_app_set_scale_ready(true);
    app_update();

    TEST_ASSERT_EQUAL_INT32(
        -172802,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        50.25F,
        fake_app_last_scale_factor()
    );

    fake_app_queue_console_command('c');
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        2UL,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_CALIBRATION_ZERO,
        fake_app_operation_indicator_mode()
    );
}


static void test_serial_tare_starts_incremental_collection_without_blocking(void)
{
    load_configuration_with_tare(-172706);

    start_serial_tare();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_UINT8(
        TARE_SAMPLES,
        fake_app_last_requested_sample_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_TARE,
        fake_app_operation_indicator_mode()
    );
}


static void test_physical_tare_hold_starts_same_incremental_collection(void)
{
    load_configuration();

    fake_app_hold_tare_button();
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_UINT8(
        TARE_SAMPLES,
        fake_app_last_requested_sample_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_TARE,
        fake_app_operation_indicator_mode()
    );
}


static void test_tare_sampling_updates_once_and_rejects_other_input(void)
{
    load_configuration_with_tare(-172706);
    start_serial_tare();

    const uint32_t indicator_updates_before =
        fake_app_operation_indicator_update_call_count();

    fake_app_press_calibration_button();
    fake_app_queue_console_command('s');

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        indicator_updates_before + 1UL,
        fake_app_operation_indicator_update_call_count()
    );

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
        1UL,
        fake_app_calibration_button_suppression_count()
    );

    assert_console_contains(
        "Tare is in progress. Send 'q' to cancel."
    );
}


static void test_serial_q_cancels_before_next_sample_and_restores_normal(void)
{
    load_configuration_with_tare(-172706);
    start_serial_tare();

    fake_app_queue_console_command('q');
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains("Tare cancelled.");

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_weight_read_call_count()
    );
}


static void test_button_cancels_before_sample_and_discards_queued_console_input(void)
{
    load_configuration_with_tare(-172706);
    start_serial_tare();

    fake_app_press_tare_button();
    fake_app_queue_console_command('s');

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
        1UL,
        fake_app_tare_button_suppression_count()
    );

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );
}


static void test_cancelled_tare_restores_tare_required_state(void)
{
    load_configuration();
    start_serial_tare();

    fake_app_queue_console_command('q');
    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_TARE_REQUIRED,
        fake_app_operation_indicator_mode()
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );
}


static void test_completed_tare_saves_before_applying_and_discards_save_input(void)
{
    load_configuration();
    start_serial_tare();

    fake_app_set_scale_sample_average(
        true,
        -172802
    );

    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_COMPLETE
    );

    fake_app_queue_console_command_during_tare_save(
        't'
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_average_take_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_tare_save_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172802,
        fake_app_last_saved_tare_offset()
    );

    TEST_ASSERT_EQUAL_INT32(
        0,
        fake_app_scale_offset_when_tare_was_saved()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_offset_set_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172802,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_weight_read_call_count()
    );
}


static void test_tare_save_failure_preserves_previous_runtime_offset(void)
{
    load_configuration_with_tare(-172706);
    start_serial_tare();

    fake_app_set_tare_save_result(false);
    fake_app_set_scale_sample_average(true, -172802);
    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_COMPLETE
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_tare_save_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_scale_offset_when_tare_was_saved()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_offset_set_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_ERROR,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_return_mode()
    );

    assert_console_contains(
        "The previous tare offset remains active."
    );
}


static void test_tare_read_error_enters_recovery_and_preserves_offset(void)
{
    load_configuration_with_tare(-172706);
    start_serial_tare();

    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_ERROR
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_tare_save_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_offset_set_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "FAULT 04: HX711 conversion read failed."
    );
}


static void test_tare_timeout_is_exact_and_handles_millisecond_overflow(void)
{
    fake_app_set_time_ms(
        UINT32_MAX - 1000UL
    );

    load_configuration_with_tare(-172706);
    start_serial_tare();

    fake_app_advance_time_ms(
        SCALE_SAMPLE_COLLECTION_TIMEOUT_MS - 1UL
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_cancel_call_count()
    );

    fake_app_advance_time_ms(1UL);
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "FAULT 05: HX711 sample collection timeout."
    );
}


static void test_completed_tare_wins_at_timeout_deadline(void)
{
    load_configuration_with_tare(-172706);
    start_serial_tare();

    fake_app_advance_time_ms(
        SCALE_SAMPLE_COLLECTION_TIMEOUT_MS
    );

    fake_app_set_scale_sample_average(true, -172802);
    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_COMPLETE
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_tare_save_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172802,
        fake_app_last_scale_offset()
    );
}


static void test_tare_start_failure_enters_internal_fault(void)
{
    load_configuration_with_tare(-172706);

    fake_app_set_scale_collection_start_result(false);
    start_serial_tare();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_FAULT,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_return_mode()
    );

    assert_console_contains(
        "FAULT 06: Invalid sample collection state."
    );
}


static void test_result_pattern_rejects_input_and_suppresses_holds(void)
{
    load_configuration_with_tare(-172706);
    start_serial_tare();

    fake_app_set_tare_save_result(false);
    fake_app_set_scale_sample_average(true, -172802);
    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_COMPLETE
    );

    app_update();

    const uint32_t collection_starts_before =
        fake_app_scale_collection_start_call_count();

    const uint32_t tare_suppressions_before =
        fake_app_tare_button_suppression_count();

    const uint32_t calibration_suppressions_before =
        fake_app_calibration_button_suppression_count();

    fake_app_queue_console_command('c');
    fake_app_hold_tare_button();
    fake_app_hold_calibration_button();

    app_update();

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    TEST_ASSERT_EQUAL_UINT32(
        collection_starts_before,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        tare_suppressions_before + 1UL,
        fake_app_tare_button_suppression_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        calibration_suppressions_before + 1UL,
        fake_app_calibration_button_suppression_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );

    assert_console_contains(
        "Result indication is active."
    );

    fake_app_complete_operation_pattern();
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        collection_starts_before,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        collection_starts_before,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_weight_read_call_count()
    );
}


static void test_calibration_entry_waits_for_zero_without_starting_collection(void)
{
    load_configuration_with_tare(-172706);

    start_calibration_waiting_for_zero();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_CALIBRATION_ZERO,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );
}


static void test_zero_confirmation_starts_incremental_collection(void)
{
    load_configuration_with_tare(-172706);

    start_calibration_zero_sampling();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_UINT8(
        TARE_SAMPLES,
        fake_app_last_requested_sample_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_TARE,
        fake_app_operation_indicator_mode()
    );
}


static void test_zero_sampling_updates_once_and_rejects_other_input(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_zero_sampling();

    fake_app_press_calibration_button();
    fake_app_queue_console_command('s');

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
        1UL,
        fake_app_calibration_button_suppression_count()
    );

    assert_console_contains(
        "Calibration sampling is in progress."
    );
}


static void test_serial_q_cancels_zero_before_next_sample(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_zero_sampling();

    fake_app_queue_console_command('q');
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains("Calibration cancelled.");
}


static void test_tare_button_cancels_zero_and_restores_tare_required(void)
{
    load_configuration();
    start_calibration_zero_sampling();

    fake_app_press_tare_button();
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_TARE_REQUIRED,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
        1UL,
        fake_app_tare_button_suppression_count()
    );
}


static void test_completed_zero_saves_before_applying_and_survives_later_cancel(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_zero_sampling();

    fake_app_queue_console_command_during_tare_save(
        'c'
    );

    complete_calibration_zero(-172802);

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_tare_save_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172802,
        fake_app_last_saved_tare_offset()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_scale_offset_when_tare_was_saved()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172802,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_CALIBRATION_MASS,
        fake_app_operation_indicator_mode()
    );

    fake_app_queue_console_command('q');
    app_update();

    TEST_ASSERT_EQUAL_INT32(
        -172802,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );
}


static void test_zero_save_failure_preserves_previous_offset_and_allows_retry(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_zero_sampling();

    fake_app_set_tare_save_result(false);
    complete_calibration_zero(-172802);

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_offset_set_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_ERROR,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_CALIBRATION_ZERO,
        fake_app_operation_indicator_return_mode()
    );

    fake_app_complete_operation_pattern();
    app_update();

    fake_app_set_tare_save_result(true);
    fake_app_queue_console_command('c');
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        2UL,
        fake_app_scale_collection_start_call_count()
    );
}


static void test_zero_read_error_enters_recovery_and_preserves_offset(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_zero_sampling();

    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_ERROR
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_tare_save_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172706,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "FAULT 04: HX711 conversion read failed."
    );
}


static void test_zero_timeout_is_exact_and_handles_millisecond_overflow(void)
{
    fake_app_set_time_ms(UINT32_MAX - 1000UL);

    load_configuration_with_tare(-172706);
    start_calibration_zero_sampling();

    fake_app_advance_time_ms(
        SCALE_SAMPLE_COLLECTION_TIMEOUT_MS - 1UL
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_cancel_call_count()
    );

    fake_app_advance_time_ms(1UL);
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "FAULT 05: HX711 sample collection timeout."
    );
}


static void test_completed_zero_wins_at_timeout_deadline(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_zero_sampling();

    fake_app_advance_time_ms(
        SCALE_SAMPLE_COLLECTION_TIMEOUT_MS
    );

    complete_calibration_zero(-172802);

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_tare_save_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_CALIBRATION_MASS,
        fake_app_operation_indicator_mode()
    );
}


static void test_zero_collection_start_failure_enters_internal_fault(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_waiting_for_zero();

    fake_app_set_scale_collection_start_result(false);
    fake_app_queue_console_command('c');
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_FAULT,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "FAULT 06: Invalid sample collection state."
    );
}


static void test_mass_confirmation_starts_incremental_collection(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-172802);

    TEST_ASSERT_EQUAL_UINT32(
        2UL,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_UINT8(
        CALIBRATION_SAMPLES,
        fake_app_last_requested_sample_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_CALIBRATION_MASS,
        fake_app_operation_indicator_mode()
    );
}


static void test_mass_sampling_updates_once_and_rejects_other_input(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-172802);

    const uint32_t updates_before =
        fake_app_scale_collection_update_call_count();

    fake_app_press_calibration_button();
    fake_app_queue_console_command('x');
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        updates_before + 1UL,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    assert_console_contains(
        "Calibration sampling is in progress."
    );
}


static void test_serial_q_cancels_mass_without_changing_factor_or_new_tare(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-172802);

    const uint32_t updates_before =
        fake_app_scale_collection_update_call_count();

    fake_app_queue_console_command('q');
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        updates_before,
        fake_app_scale_collection_update_call_count()
    );

    TEST_ASSERT_EQUAL_INT32(
        -172802,
        fake_app_last_scale_offset()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        DEFAULT_CALIBRATION_FACTOR,
        fake_app_last_scale_factor()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_mode()
    );
}


static void test_completed_mass_calculates_saves_and_applies_factor(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-1000);

    fake_app_set_scale_sample_average(true, 59000);
    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_COMPLETE
    );

    fake_app_queue_console_command_during_calibration_save(
        't'
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_calibration_save_call_count()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        40.0F,
        fake_app_last_saved_calibration_factor()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        40.0F,
        fake_app_scale_factor_when_calibration_was_saved()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        40.0F,
        fake_app_last_scale_factor()
    );

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_SUCCESS,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "Calibration completed successfully."
    );
}


static void test_small_mass_signal_is_rejected_and_allows_retry(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-1000);

    fake_app_set_scale_sample_average(true, 3999);
    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_COMPLETE
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_calibration_save_call_count()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        DEFAULT_CALIBRATION_FACTOR,
        fake_app_last_scale_factor()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_CALIBRATION_MASS,
        fake_app_operation_indicator_return_mode()
    );

    fake_app_complete_operation_pattern();
    app_update();

    fake_app_queue_console_command('c');
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        3UL,
        fake_app_scale_collection_start_call_count()
    );
}


static void test_result_completion_consumes_boundary_input_before_retry(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-1000);

    fake_app_set_scale_sample_average(true, 3999);
    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_COMPLETE
    );

    app_update();

    const uint32_t collection_starts_before =
        fake_app_scale_collection_start_call_count();

    fake_app_complete_operation_pattern();
    fake_app_queue_console_command('c');

    app_update();

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    TEST_ASSERT_EQUAL_UINT32(
        collection_starts_before,
        fake_app_scale_collection_start_call_count()
    );

    assert_console_contains(
        "Result indication is active."
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        collection_starts_before,
        fake_app_scale_collection_start_call_count()
    );

    fake_app_queue_console_command('c');
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        collection_starts_before + 1UL,
        fake_app_scale_collection_start_call_count()
    );
}


static void test_invalid_calculated_factor_preserves_previous_factor(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-1000);

    fake_app_set_scale_factor_result(false);
    fake_app_set_scale_sample_average(true, 59000);
    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_COMPLETE
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_calibration_save_call_count()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        DEFAULT_CALIBRATION_FACTOR,
        fake_app_last_scale_factor()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_CALIBRATION_MASS,
        fake_app_operation_indicator_return_mode()
    );
}


static void test_calibration_save_failure_restores_previous_factor_and_exits(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-1000);

    fake_app_set_calibration_save_result(false);
    fake_app_set_scale_sample_average(true, 59000);
    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_COMPLETE
    );

    fake_app_queue_console_command_during_calibration_save(
        'c'
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        3UL,
        fake_app_scale_factor_set_call_count()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        DEFAULT_CALIBRATION_FACTOR,
        fake_app_last_scale_factor()
    );

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_ERROR,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_return_mode()
    );

    fake_app_complete_operation_pattern();
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_weight_read_call_count()
    );
}


static void test_mass_read_error_enters_recovery(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-1000);

    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_ERROR
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_calibration_save_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "FAULT 04: HX711 conversion read failed."
    );
}


static void test_mass_timeout_is_exact_and_handles_millisecond_overflow(void)
{
    fake_app_set_time_ms(UINT32_MAX - 1000UL);

    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-1000);

    const uint32_t cancels_before =
        fake_app_scale_cancel_call_count();

    fake_app_advance_time_ms(
        SCALE_SAMPLE_COLLECTION_TIMEOUT_MS - 1UL
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        cancels_before,
        fake_app_scale_cancel_call_count()
    );

    fake_app_advance_time_ms(1UL);
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        cancels_before + 1UL,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_RECOVERY,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "FAULT 05: HX711 sample collection timeout."
    );
}


static void test_completed_mass_wins_at_timeout_deadline(void)
{
    load_configuration_with_tare(-172706);
    start_calibration_mass_sampling(-1000);

    const uint32_t cancels_before =
        fake_app_scale_cancel_call_count();

    fake_app_advance_time_ms(
        SCALE_SAMPLE_COLLECTION_TIMEOUT_MS
    );

    fake_app_set_scale_sample_average(true, 59000);
    fake_app_set_scale_collection_status(
        SCALE_SAMPLE_COLLECTION_COMPLETE
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        cancels_before,
        fake_app_scale_cancel_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_calibration_save_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_SUCCESS,
        fake_app_operation_indicator_mode()
    );
}


static void test_mass_collection_start_failure_enters_internal_fault(void)
{
    load_configuration_with_tare(-172706);
    reach_calibration_mass_wait(-1000);

    fake_app_set_scale_collection_start_result(false);
    fake_app_queue_console_command('c');
    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        2UL,
        fake_app_scale_collection_start_call_count()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_FAULT,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "FAULT 06: Invalid sample collection state."
    );
}


int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_configures_scale_without_polling_or_loading_storage);
    RUN_TEST(test_immediate_scale_initialization_failure_enters_fault);
    RUN_TEST(test_startup_wait_returns_without_loading_configuration);
    RUN_TEST(test_startup_timeout_enters_recovery_at_exact_deadline);
    RUN_TEST(test_ready_scale_wins_when_detected_at_timeout_deadline);
    RUN_TEST(test_startup_timeout_handles_millisecond_overflow);
    RUN_TEST(test_valid_stored_configuration_is_loaded_once);
    RUN_TEST(test_invalid_calibration_uses_default_with_corruption_diagnostic);
    RUN_TEST(test_calibration_storage_access_error_enters_terminal_fault);
    RUN_TEST(test_runtime_read_error_enters_recovery_with_stable_code);
    RUN_TEST(test_runtime_no_data_is_tolerated_before_timeout);
    RUN_TEST(test_runtime_no_data_enters_recovery_at_exact_timeout);
    RUN_TEST(test_runtime_value_wins_at_timeout_and_renews_deadline);
    RUN_TEST(test_runtime_no_data_timeout_handles_millisecond_overflow);
    RUN_TEST(test_runtime_supervision_restarts_after_recovery);
    RUN_TEST(test_runtime_timeout_is_inactive_during_calibration_wait);
    RUN_TEST(test_missing_tare_selects_tare_required_and_disables_measurement);
    RUN_TEST(test_invalid_tare_selects_tare_required_with_corruption_diagnostic);
    RUN_TEST(test_tare_storage_access_error_enters_terminal_fault);
    RUN_TEST(test_invalid_active_calibration_factor_enters_fault_before_tare_load);
    RUN_TEST(test_startup_commands_and_presses_are_consumed);
    RUN_TEST(test_input_received_during_configuration_is_not_executed_later);
    RUN_TEST(test_terminal_fault_is_latched_and_rejects_commands);
    RUN_TEST(test_recovery_backoff_is_exact_and_handles_millisecond_overflow);
    RUN_TEST(test_failed_power_cycles_exhaust_exactly_three_attempts);
    RUN_TEST(test_ready_conversion_wins_at_recovery_timeout_deadline);
    RUN_TEST(test_recovery_ready_timeout_schedules_next_attempt_across_overflow);
    RUN_TEST(test_startup_recovery_loads_configuration_on_following_update);
    RUN_TEST(test_runtime_recovery_returns_to_normal_on_following_update);
    RUN_TEST(test_recovery_without_valid_tare_returns_to_tare_required);
    RUN_TEST(test_recovery_discards_input_and_suppresses_button_holds);
    RUN_TEST(test_recovery_cancels_tare_and_preserves_committed_values);
    RUN_TEST(test_recovery_cancels_calibration_and_returns_to_normal);
    RUN_TEST(test_serial_tare_starts_incremental_collection_without_blocking);
    RUN_TEST(test_physical_tare_hold_starts_same_incremental_collection);
    RUN_TEST(test_tare_sampling_updates_once_and_rejects_other_input);
    RUN_TEST(test_serial_q_cancels_before_next_sample_and_restores_normal);
    RUN_TEST(test_button_cancels_before_sample_and_discards_queued_console_input);
    RUN_TEST(test_cancelled_tare_restores_tare_required_state);
    RUN_TEST(test_completed_tare_saves_before_applying_and_discards_save_input);
    RUN_TEST(test_tare_save_failure_preserves_previous_runtime_offset);
    RUN_TEST(test_tare_read_error_enters_recovery_and_preserves_offset);
    RUN_TEST(test_tare_timeout_is_exact_and_handles_millisecond_overflow);
    RUN_TEST(test_completed_tare_wins_at_timeout_deadline);
    RUN_TEST(test_tare_start_failure_enters_internal_fault);
    RUN_TEST(test_result_pattern_rejects_input_and_suppresses_holds);
    RUN_TEST(test_calibration_entry_waits_for_zero_without_starting_collection);
    RUN_TEST(test_zero_confirmation_starts_incremental_collection);
    RUN_TEST(test_zero_sampling_updates_once_and_rejects_other_input);
    RUN_TEST(test_serial_q_cancels_zero_before_next_sample);
    RUN_TEST(test_tare_button_cancels_zero_and_restores_tare_required);
    RUN_TEST(test_completed_zero_saves_before_applying_and_survives_later_cancel);
    RUN_TEST(test_zero_save_failure_preserves_previous_offset_and_allows_retry);
    RUN_TEST(test_zero_read_error_enters_recovery_and_preserves_offset);
    RUN_TEST(test_zero_timeout_is_exact_and_handles_millisecond_overflow);
    RUN_TEST(test_completed_zero_wins_at_timeout_deadline);
    RUN_TEST(test_zero_collection_start_failure_enters_internal_fault);
    RUN_TEST(test_mass_confirmation_starts_incremental_collection);
    RUN_TEST(test_mass_sampling_updates_once_and_rejects_other_input);
    RUN_TEST(test_serial_q_cancels_mass_without_changing_factor_or_new_tare);
    RUN_TEST(test_completed_mass_calculates_saves_and_applies_factor);
    RUN_TEST(test_small_mass_signal_is_rejected_and_allows_retry);
    RUN_TEST(test_result_completion_consumes_boundary_input_before_retry);
    RUN_TEST(test_invalid_calculated_factor_preserves_previous_factor);
    RUN_TEST(test_calibration_save_failure_restores_previous_factor_and_exits);
    RUN_TEST(test_mass_read_error_enters_recovery);
    RUN_TEST(test_mass_timeout_is_exact_and_handles_millisecond_overflow);
    RUN_TEST(test_completed_mass_wins_at_timeout_deadline);
    RUN_TEST(test_mass_collection_start_failure_enters_internal_fault);

    return UNITY_END();
}
