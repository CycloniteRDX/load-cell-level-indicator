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

    return UNITY_END();
}
