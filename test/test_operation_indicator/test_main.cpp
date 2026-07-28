#include <stdint.h>
#include <unity.h>

#include "config.h"
#include "fake_operation_indicator_support.h"
#include "operation_indicator.h"

void setUp(void)
{
    fake_operation_indicator_reset();
    operation_indicator_init();
}

void tearDown(void)
{
}

static void assert_leds(
    bool expected_low,
    bool expected_medium,
    bool expected_high
)
{
    TEST_ASSERT_EQUAL_INT(
        expected_low,
        fake_operation_indicator_low_led_is_on()
    );

    TEST_ASSERT_EQUAL_INT(
        expected_medium,
        fake_operation_indicator_medium_led_is_on()
    );

    TEST_ASSERT_EQUAL_INT(
        expected_high,
        fake_operation_indicator_high_led_is_on()
    );
}

static void advance_and_update(
    uint32_t elapsed_ms
)
{
    fake_operation_indicator_advance_time_ms(
        elapsed_ms
    );

    operation_indicator_update();
}

static void test_init_turns_all_leds_off(void)
{
    assert_leds(false, false, false);

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );
}

static void test_tare_turns_all_leds_on_persistently(void)
{
    operation_indicator_set_mode(
        OPERATION_INDICATOR_TARE
    );

    assert_leds(true, true, true);

    advance_and_update(
        OPERATION_INDICATOR_BLINK_PERIOD_MS * 2U
    );

    assert_leds(true, true, true);

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );
}

static void test_tare_required_blinks_all_leds_slowly(void)
{
    operation_indicator_set_mode(
        OPERATION_INDICATOR_TARE_REQUIRED
    );

    assert_leds(true, true, true);

    advance_and_update(
        TARE_REQUIRED_BLINK_PERIOD_MS - 1U
    );

    assert_leds(true, true, true);

    advance_and_update(1U);

    assert_leds(false, false, false);

    advance_and_update(
        TARE_REQUIRED_BLINK_PERIOD_MS
    );

    assert_leds(true, true, true);

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );
}


static void test_error_can_return_to_tare_required_mode(void)
{
    operation_indicator_show_error(
        OPERATION_INDICATOR_TARE_REQUIRED
    );

    for (uint8_t transition = 0U;
         transition < 5U;
         ++transition)
    {
        advance_and_update(
            OPERATION_RESULT_BLINK_PERIOD_MS
        );
    }

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );

    assert_leds(true, true, true);

    advance_and_update(
        TARE_REQUIRED_BLINK_PERIOD_MS - 1U
    );

    assert_leds(true, true, true);

    advance_and_update(1U);

    assert_leds(false, false, false);
}


static void test_calibration_zero_blinks_only_low_led(void)
{
    operation_indicator_set_mode(
        OPERATION_INDICATOR_CALIBRATION_ZERO
    );

    assert_leds(true, false, false);

    advance_and_update(
        OPERATION_INDICATOR_BLINK_PERIOD_MS - 1U
    );

    assert_leds(true, false, false);

    advance_and_update(1U);

    assert_leds(false, false, false);

    advance_and_update(
        OPERATION_INDICATOR_BLINK_PERIOD_MS
    );

    assert_leds(true, false, false);
}

static void test_calibration_mass_blinks_only_medium_led(void)
{
    operation_indicator_set_mode(
        OPERATION_INDICATOR_CALIBRATION_MASS
    );

    assert_leds(false, true, false);

    advance_and_update(
        OPERATION_INDICATOR_BLINK_PERIOD_MS
    );

    assert_leds(false, false, false);

    advance_and_update(
        OPERATION_INDICATOR_BLINK_PERIOD_MS
    );

    assert_leds(false, true, false);
}

static void test_none_mode_turns_all_leds_off(void)
{
    operation_indicator_set_mode(
        OPERATION_INDICATOR_TARE
    );

    assert_leds(true, true, true);

    operation_indicator_set_mode(
        OPERATION_INDICATOR_NONE
    );

    assert_leds(false, false, false);
}

static void test_success_flashes_all_leds_twice_then_releases_them(void)
{
    operation_indicator_show_success();

    assert_leds(true, true, true);

    TEST_ASSERT_TRUE(
        operation_indicator_is_temporary_active()
    );

    advance_and_update(
        OPERATION_RESULT_BLINK_PERIOD_MS - 1U
    );

    assert_leds(true, true, true);

    advance_and_update(1U);

    assert_leds(false, false, false);

    TEST_ASSERT_TRUE(
        operation_indicator_is_temporary_active()
    );

    advance_and_update(
        OPERATION_RESULT_BLINK_PERIOD_MS
    );

    assert_leds(true, true, true);

    advance_and_update(
        OPERATION_RESULT_BLINK_PERIOD_MS
    );

    assert_leds(false, false, false);

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );
}

static void test_error_flashes_high_led_three_times_then_returns_to_zero_mode(void)
{
    operation_indicator_show_error(
        OPERATION_INDICATOR_CALIBRATION_ZERO
    );

    assert_leds(false, false, true);

    TEST_ASSERT_TRUE(
        operation_indicator_is_temporary_active()
    );

    advance_and_update(OPERATION_RESULT_BLINK_PERIOD_MS);
    assert_leds(false, false, false);

    advance_and_update(OPERATION_RESULT_BLINK_PERIOD_MS);
    assert_leds(false, false, true);

    advance_and_update(OPERATION_RESULT_BLINK_PERIOD_MS);
    assert_leds(false, false, false);

    advance_and_update(OPERATION_RESULT_BLINK_PERIOD_MS);
    assert_leds(false, false, true);

    advance_and_update(OPERATION_RESULT_BLINK_PERIOD_MS);

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );

    assert_leds(true, false, false);

    advance_and_update(
        OPERATION_INDICATOR_BLINK_PERIOD_MS
    );

    assert_leds(false, false, false);
}

static void test_error_can_return_to_tare_mode(void)
{
    operation_indicator_show_error(
        OPERATION_INDICATOR_TARE
    );

    for (uint8_t transition = 0U;
         transition < 5U;
         ++transition)
    {
        advance_and_update(
            OPERATION_RESULT_BLINK_PERIOD_MS
        );
    }

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );

    assert_leds(true, true, true);
}

static void test_error_with_none_return_releases_leds(void)
{
    operation_indicator_show_error(
        OPERATION_INDICATOR_NONE
    );

    for (uint8_t transition = 0U;
         transition < 5U;
         ++transition)
    {
        advance_and_update(
            OPERATION_RESULT_BLINK_PERIOD_MS
        );
    }

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );

    assert_leds(false, false, false);
}

static void test_clear_cancels_temporary_pattern(void)
{
    operation_indicator_show_success();

    TEST_ASSERT_TRUE(
        operation_indicator_is_temporary_active()
    );

    operation_indicator_clear();

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );

    assert_leds(false, false, false);

    advance_and_update(
        OPERATION_RESULT_BLINK_PERIOD_MS * 4U
    );

    assert_leds(false, false, false);
}

static void test_set_mode_cancels_temporary_pattern(void)
{
    operation_indicator_show_error(
        OPERATION_INDICATOR_CALIBRATION_ZERO
    );

    TEST_ASSERT_TRUE(
        operation_indicator_is_temporary_active()
    );

    operation_indicator_set_mode(
        OPERATION_INDICATOR_CALIBRATION_MASS
    );

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );

    assert_leds(false, true, false);

    advance_and_update(
        OPERATION_INDICATOR_BLINK_PERIOD_MS
    );

    assert_leds(false, false, false);
}

static void test_invalid_mode_turns_leds_off_and_does_not_blink(void)
{
    operation_indicator_set_mode(
        (operation_indicator_mode_t)99
    );

    assert_leds(false, false, false);

    advance_and_update(
        OPERATION_INDICATOR_BLINK_PERIOD_MS * 3U
    );

    assert_leds(false, false, false);

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );
}

static void test_persistent_blinking_handles_millisecond_overflow(void)
{
    fake_operation_indicator_set_time_ms(
        UINT32_MAX - 200U
    );

    operation_indicator_set_mode(
        OPERATION_INDICATOR_CALIBRATION_ZERO
    );

    assert_leds(true, false, false);

    advance_and_update(
        OPERATION_INDICATOR_BLINK_PERIOD_MS - 1U
    );

    assert_leds(true, false, false);

    advance_and_update(1U);

    assert_leds(false, false, false);
}

static void test_temporary_pattern_handles_millisecond_overflow(void)
{
    fake_operation_indicator_set_time_ms(
        UINT32_MAX - 50U
    );

    operation_indicator_show_success();

    assert_leds(true, true, true);

    advance_and_update(
        OPERATION_RESULT_BLINK_PERIOD_MS
    );

    assert_leds(false, false, false);

    advance_and_update(
        OPERATION_RESULT_BLINK_PERIOD_MS
    );

    assert_leds(true, true, true);

    advance_and_update(
        OPERATION_RESULT_BLINK_PERIOD_MS
    );

    assert_leds(false, false, false);

    TEST_ASSERT_FALSE(
        operation_indicator_is_temporary_active()
    );
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_turns_all_leds_off);
    RUN_TEST(test_tare_turns_all_leds_on_persistently);
    RUN_TEST(test_tare_required_blinks_all_leds_slowly);
    RUN_TEST(test_error_can_return_to_tare_required_mode);
    RUN_TEST(test_calibration_zero_blinks_only_low_led);
    RUN_TEST(test_calibration_mass_blinks_only_medium_led);
    RUN_TEST(test_none_mode_turns_all_leds_off);
    RUN_TEST(test_success_flashes_all_leds_twice_then_releases_them);
    RUN_TEST(test_error_flashes_high_led_three_times_then_returns_to_zero_mode);
    RUN_TEST(test_error_can_return_to_tare_mode);
    RUN_TEST(test_error_with_none_return_releases_leds);
    RUN_TEST(test_clear_cancels_temporary_pattern);
    RUN_TEST(test_set_mode_cancels_temporary_pattern);
    RUN_TEST(test_invalid_mode_turns_leds_off_and_does_not_blink);
    RUN_TEST(test_persistent_blinking_handles_millisecond_overflow);
    RUN_TEST(test_temporary_pattern_handles_millisecond_overflow);

    return UNITY_END();
}
