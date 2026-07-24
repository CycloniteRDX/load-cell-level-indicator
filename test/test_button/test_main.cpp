#include <unity.h>

#include "button.h"
#include "fake_hal.h"


static const uint8_t TEST_BUTTON_PIN = 7U;

static const uint32_t TEST_DEBOUNCE_MS =
    40U;


void setUp(void)
{
    fake_hal_reset();

    /*
     * INPUT_PULLUP:
     *
     * true  = HIGH = released
     * false = LOW  = pressed
     */
    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        true
    );
}


void tearDown(void)
{
}


static void
test_button_init_configures_pullup_and_released_state(
    void
)
{
    button_t button = {};

    button_init(
        &button,
        TEST_BUTTON_PIN,
        TEST_DEBOUNCE_MS
    );

    TEST_ASSERT_EQUAL_UINT8(
        FAKE_GPIO_MODE_INPUT_PULLUP,
        fake_hal_get_pin_mode(
            TEST_BUTTON_PIN
        )
    );

    TEST_ASSERT_EQUAL_UINT8(
        TEST_BUTTON_PIN,
        button.pin
    );

    TEST_ASSERT_TRUE(
        button.last_raw_state
    );

    TEST_ASSERT_TRUE(
        button.stable_state
    );

    TEST_ASSERT_EQUAL_UINT32(
        TEST_DEBOUNCE_MS,
        button.debounce_ms
    );

    TEST_ASSERT_FALSE(
        button.hold_event_reported
    );
}


static void
test_press_is_not_reported_before_debounce_finishes(
    void
)
{
    button_t button = {};

    button_init(
        &button,
        TEST_BUTTON_PIN,
        TEST_DEBOUNCE_MS
    );

    /*
     * The physical contact changes to LOW at 100 ms.
     */
    fake_hal_set_time_ms(100U);

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        false
    );

    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );

    /*
     * Only 39 ms have elapsed since the transition.
     */
    fake_hal_set_time_ms(139U);

    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );
}


static void
test_press_is_reported_once_after_debounce_finishes(
    void
)
{
    button_t button = {};

    button_init(
        &button,
        TEST_BUTTON_PIN,
        TEST_DEBOUNCE_MS
    );

    fake_hal_set_time_ms(100U);

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        false
    );

    /*
     * Records the raw transition and starts
     * the debounce interval.
     */
    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );

    /*
     * Exactly 40 ms have now elapsed.
     */
    fake_hal_set_time_ms(140U);

    TEST_ASSERT_TRUE(
        button_was_pressed(&button)
    );

    /*
     * The same physical press must not be
     * reported repeatedly.
     */
    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );
}


int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(
        test_button_init_configures_pullup_and_released_state
    );

    RUN_TEST(
        test_press_is_not_reported_before_debounce_finishes
    );

    RUN_TEST(
        test_press_is_reported_once_after_debounce_finishes
    );

    return UNITY_END();
}