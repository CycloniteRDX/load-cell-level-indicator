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
        "ERROR: HX711 initialization failed."
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


static void test_startup_timeout_enters_fault_at_exact_deadline(void)
{
    app_init();

    fake_app_set_time_ms(
        SCALE_STARTUP_TIMEOUT_MS
    );

    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_FAULT,
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
        "ERROR: HX711 startup timed out."
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
        OPERATION_INDICATOR_FAULT,
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

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_tare_call_count()
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

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_tare_call_count()
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_weight_read_call_count()
    );

    assert_console_contains(
        "Normal level indication is disabled."
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
        "ERROR: Invalid calibration factor."
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
        0UL,
        fake_app_scale_tare_call_count()
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

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_tare_call_count()
    );

    app_update();

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_tare_call_count()
    );
}


static void test_fault_is_latched_and_rejects_commands(void)
{
    app_init();

    fake_app_set_time_ms(
        SCALE_STARTUP_TIMEOUT_MS
    );

    app_update();

    fake_app_set_scale_ready(true);
    fake_app_queue_console_command('t');

    app_update();

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_FAULT,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1UL,
        fake_app_scale_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_calibration_load_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0UL,
        fake_app_scale_tare_call_count()
    );

    TEST_ASSERT_FALSE(
        fake_app_console_input_is_pending()
    );

    assert_console_contains(
        "FAULT: Reset required."
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
        fake_app_scale_tare_call_count()
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


static void test_tare_read_error_preserves_previous_runtime_offset(void)
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
        OPERATION_INDICATOR_ERROR,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "ERROR: Tare samples could not be read."
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
        OPERATION_INDICATOR_ERROR,
        fake_app_operation_indicator_mode()
    );

    assert_console_contains(
        "ERROR: Tare sample collection timed out."
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


static void test_tare_start_failure_restores_previous_idle_state(void)
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
        OPERATION_INDICATOR_ERROR,
        fake_app_operation_indicator_mode()
    );

    TEST_ASSERT_EQUAL_INT(
        OPERATION_INDICATOR_NONE,
        fake_app_operation_indicator_return_mode()
    );

    assert_console_contains(
        "ERROR: Tare sample collection could not be started."
    );
}


int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_configures_scale_without_polling_or_loading_storage);
    RUN_TEST(test_immediate_scale_initialization_failure_enters_fault);
    RUN_TEST(test_startup_wait_returns_without_loading_configuration);
    RUN_TEST(test_startup_timeout_enters_fault_at_exact_deadline);
    RUN_TEST(test_ready_scale_wins_when_detected_at_timeout_deadline);
    RUN_TEST(test_startup_timeout_handles_millisecond_overflow);
    RUN_TEST(test_valid_stored_configuration_is_loaded_once);
    RUN_TEST(test_missing_tare_selects_tare_required_and_disables_measurement);
    RUN_TEST(test_invalid_active_calibration_factor_enters_fault_before_tare_load);
    RUN_TEST(test_startup_commands_and_presses_are_consumed);
    RUN_TEST(test_input_received_during_configuration_is_not_executed_later);
    RUN_TEST(test_fault_is_latched_and_rejects_commands);
    RUN_TEST(test_serial_tare_starts_incremental_collection_without_blocking);
    RUN_TEST(test_physical_tare_hold_starts_same_incremental_collection);
    RUN_TEST(test_tare_sampling_updates_once_and_rejects_other_input);
    RUN_TEST(test_serial_q_cancels_before_next_sample_and_restores_normal);
    RUN_TEST(test_button_cancels_before_sample_and_discards_queued_console_input);
    RUN_TEST(test_cancelled_tare_restores_tare_required_state);
    RUN_TEST(test_completed_tare_saves_before_applying_and_discards_save_input);
    RUN_TEST(test_tare_save_failure_preserves_previous_runtime_offset);
    RUN_TEST(test_tare_read_error_preserves_previous_runtime_offset);
    RUN_TEST(test_tare_timeout_is_exact_and_handles_millisecond_overflow);
    RUN_TEST(test_completed_tare_wins_at_timeout_deadline);
    RUN_TEST(test_tare_start_failure_restores_previous_idle_state);

    return UNITY_END();
}
