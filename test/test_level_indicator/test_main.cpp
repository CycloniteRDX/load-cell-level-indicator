#include <stdint.h>
#include <string.h>
#include <unity.h>

#include "fake_level_indicator_support.h"
#include "level_indicator.h"

void setUp(void)
{
    fake_level_indicator_support_reset();
    level_indicator_reset();
}

void tearDown(void)
{
}

static void assert_leds(
    bool low_led_on,
    bool medium_led_on,
    bool high_led_on
)
{
    TEST_ASSERT_EQUAL_INT(
        low_led_on,
        fake_level_indicator_low_led_is_on()
    );

    TEST_ASSERT_EQUAL_INT(
        medium_led_on,
        fake_level_indicator_medium_led_is_on()
    );

    TEST_ASSERT_EQUAL_INT(
        high_led_on,
        fake_level_indicator_high_led_is_on()
    );
}

static void assert_state_name(
    const char *expected_name
)
{
    TEST_ASSERT_EQUAL_STRING(
        expected_name,
        level_indicator_get_state_name()
    );
}

static void test_init_leaves_state_unknown_and_all_leds_off(void)
{
    level_indicator_update(750.0F);

    level_indicator_init();

    assert_state_name("UNKNOWN");
    assert_leds(false, false, false);
}

static void test_initial_weight_below_100_selects_very_low(void)
{
    level_indicator_update(99.0F);

    assert_state_name("VERY_LOW");
    assert_leds(true, false, false);
}

static void test_initial_threshold_boundaries_select_higher_level(void)
{
    level_indicator_update(100.0F);
    assert_state_name("LOW");
    assert_leds(true, false, false);

    level_indicator_reset();
    level_indicator_update(500.0F);
    assert_state_name("MEDIUM");
    assert_leds(false, true, false);

    level_indicator_reset();
    level_indicator_update(1000.0F);
    assert_state_name("HIGH");
    assert_leds(false, false, true);
}

static void test_very_low_requires_120_grams_to_enter_low(void)
{
    level_indicator_update(0.0F);

    level_indicator_update(119.0F);
    assert_state_name("VERY_LOW");

    level_indicator_update(120.0F);
    assert_state_name("LOW");
    assert_leds(true, false, false);
}

static void test_low_uses_hysteresis_in_both_directions(void)
{
    level_indicator_update(100.0F);

    level_indicator_update(81.0F);
    assert_state_name("LOW");

    level_indicator_update(80.0F);
    assert_state_name("VERY_LOW");

    level_indicator_reset();
    level_indicator_update(100.0F);

    level_indicator_update(519.0F);
    assert_state_name("LOW");

    level_indicator_update(520.0F);
    assert_state_name("MEDIUM");
}

static void test_medium_uses_hysteresis_in_both_directions(void)
{
    level_indicator_update(500.0F);

    level_indicator_update(481.0F);
    assert_state_name("MEDIUM");

    level_indicator_update(480.0F);
    assert_state_name("LOW");

    level_indicator_reset();
    level_indicator_update(500.0F);

    level_indicator_update(1019.0F);
    assert_state_name("MEDIUM");

    level_indicator_update(1020.0F);
    assert_state_name("HIGH");
}

static void test_high_requires_980_grams_to_return_to_medium(void)
{
    level_indicator_update(1000.0F);

    level_indicator_update(981.0F);
    assert_state_name("HIGH");

    level_indicator_update(980.0F);
    assert_state_name("MEDIUM");
    assert_leds(false, true, false);
}

static void test_direct_jumps_between_distant_levels_are_allowed(void)
{
    level_indicator_update(0.0F);
    level_indicator_update(1020.0F);

    assert_state_name("HIGH");
    assert_leds(false, false, true);

    level_indicator_update(80.0F);

    assert_state_name("VERY_LOW");
    assert_leds(true, false, false);
}

static void test_very_low_does_not_blink_before_250_ms(void)
{
    fake_level_indicator_set_time_ms(100U);
    level_indicator_update(0.0F);

    fake_level_indicator_advance_time_ms(249U);
    level_indicator_update_visual();

    assert_leds(true, false, false);
}

static void test_very_low_toggles_every_250_ms(void)
{
    fake_level_indicator_set_time_ms(100U);
    level_indicator_update(0.0F);

    fake_level_indicator_advance_time_ms(250U);
    level_indicator_update_visual();
    assert_leds(false, false, false);

    fake_level_indicator_advance_time_ms(250U);
    level_indicator_update_visual();
    assert_leds(true, false, false);
}

static void test_leaving_very_low_stops_blinking(void)
{
    level_indicator_update(0.0F);

    fake_level_indicator_advance_time_ms(250U);
    level_indicator_update_visual();
    assert_leds(false, false, false);

    level_indicator_update(120.0F);
    assert_state_name("LOW");
    assert_leds(true, false, false);

    fake_level_indicator_advance_time_ms(1000U);
    level_indicator_update_visual();
    assert_leds(true, false, false);
}

static void test_visual_update_does_nothing_for_non_blinking_levels(void)
{
    level_indicator_update(750.0F);

    fake_level_indicator_advance_time_ms(10000U);
    level_indicator_update_visual();

    assert_state_name("MEDIUM");
    assert_leds(false, true, false);
}

static void test_reset_clears_active_level_and_turns_all_leds_off(void)
{
    level_indicator_update(1200.0F);

    level_indicator_reset();

    assert_state_name("UNKNOWN");
    assert_leds(false, false, false);
}

static void test_very_low_blinking_works_across_millisecond_overflow(void)
{
    fake_level_indicator_set_time_ms(
        UINT32_MAX - 100U
    );

    level_indicator_update(0.0F);

    fake_level_indicator_advance_time_ms(249U);
    level_indicator_update_visual();
    assert_leds(true, false, false);

    fake_level_indicator_advance_time_ms(1U);
    level_indicator_update_visual();
    assert_leds(false, false, false);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(
        test_init_leaves_state_unknown_and_all_leds_off
    );

    RUN_TEST(
        test_initial_weight_below_100_selects_very_low
    );

    RUN_TEST(
        test_initial_threshold_boundaries_select_higher_level
    );

    RUN_TEST(
        test_very_low_requires_120_grams_to_enter_low
    );

    RUN_TEST(
        test_low_uses_hysteresis_in_both_directions
    );

    RUN_TEST(
        test_medium_uses_hysteresis_in_both_directions
    );

    RUN_TEST(
        test_high_requires_980_grams_to_return_to_medium
    );

    RUN_TEST(
        test_direct_jumps_between_distant_levels_are_allowed
    );

    RUN_TEST(
        test_very_low_does_not_blink_before_250_ms
    );

    RUN_TEST(
        test_very_low_toggles_every_250_ms
    );

    RUN_TEST(
        test_leaving_very_low_stops_blinking
    );

    RUN_TEST(
        test_visual_update_does_nothing_for_non_blinking_levels
    );

    RUN_TEST(
        test_reset_clears_active_level_and_turns_all_leds_off
    );

    RUN_TEST(
        test_very_low_blinking_works_across_millisecond_overflow
    );

    return UNITY_END();
}
