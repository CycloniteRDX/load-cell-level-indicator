#include <stdbool.h>

#include <unity.h>

#include "watchdog_validation.h"


static watchdog_validation_t validation;


void setUp(void)
{
    watchdog_validation_init(&validation);
}


void tearDown(void)
{
}


static bool update_trigger(
    bool tare_button_released,
    bool calibration_button_released
)
{
    return watchdog_validation_should_stall(
        &validation,
        tare_button_released,
        calibration_button_released
    );
}


static void test_null_state_fails_safely(void)
{
    watchdog_validation_init(NULL);

    TEST_ASSERT_FALSE(
        watchdog_validation_should_stall(
            NULL,
            false,
            false
        )
    );
}


static void test_buttons_held_at_startup_do_not_trigger(void)
{
    TEST_ASSERT_FALSE(update_trigger(false, false));
    TEST_ASSERT_FALSE(update_trigger(false, false));
}


static void test_one_released_button_does_not_arm_trigger(void)
{
    TEST_ASSERT_FALSE(update_trigger(true, false));
    TEST_ASSERT_FALSE(update_trigger(false, true));
    TEST_ASSERT_FALSE(update_trigger(false, false));
}


static void test_both_released_buttons_arm_without_triggering(void)
{
    TEST_ASSERT_FALSE(update_trigger(true, true));
}


static void test_armed_trigger_requires_both_buttons_pressed(void)
{
    TEST_ASSERT_FALSE(update_trigger(true, true));

    TEST_ASSERT_FALSE(update_trigger(false, true));
    TEST_ASSERT_FALSE(update_trigger(true, false));

    TEST_ASSERT_TRUE(update_trigger(false, false));
}


static void test_reinitialization_disarms_trigger(void)
{
    TEST_ASSERT_FALSE(update_trigger(true, true));
    TEST_ASSERT_TRUE(update_trigger(false, false));

    watchdog_validation_init(&validation);

    TEST_ASSERT_FALSE(update_trigger(false, false));
}


int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_null_state_fails_safely);
    RUN_TEST(test_buttons_held_at_startup_do_not_trigger);
    RUN_TEST(test_one_released_button_does_not_arm_trigger);
    RUN_TEST(test_both_released_buttons_arm_without_triggering);
    RUN_TEST(test_armed_trigger_requires_both_buttons_pressed);
    RUN_TEST(test_reinitialization_disarms_trigger);

    return UNITY_END();
}
