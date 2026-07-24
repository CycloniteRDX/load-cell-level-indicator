#include <unity.h>
#include <stdint.h>

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

static void
test_debounce_works_across_millisecond_overflow(
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
     * Start the transition 20 ms before UINT32_MAX.
     */
    fake_hal_set_time_ms(
        UINT32_MAX - 20U
    );

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        false
    );

    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );

    /*
     * Advancing 39 ms wraps the counter.
     */
    fake_hal_advance_time_ms(39U);

    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );

    /*
     * The total elapsed time is now exactly 40 ms,
     * even though the counter has overflowed.
     */
    fake_hal_advance_time_ms(1U);

    TEST_ASSERT_TRUE(
        button_was_pressed(&button)
    );
}

static void
test_null_button_pointer_is_handled_safely(
    void
)
{
    button_init(
        nullptr,
        TEST_BUTTON_PIN,
        TEST_DEBOUNCE_MS
    );

    TEST_ASSERT_FALSE(
        button_was_pressed(nullptr)
    );

    TEST_ASSERT_FALSE(
        button_was_held(
            nullptr,
            1000U
        )
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


static void
test_contact_bounce_produces_only_one_press_event(
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
     * First falling edge.
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
     * Contact bounces back to HIGH.
     */
    fake_hal_set_time_ms(105U);

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        true
    );

    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );

    /*
     * Contact returns to LOW and remains there.
     * The debounce interval restarts here.
     */
    fake_hal_set_time_ms(110U);

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        false
    );

    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );

    fake_hal_set_time_ms(149U);

    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );

    /*
     * LOW has now remained stable for 40 ms.
     */
    fake_hal_set_time_ms(150U);

    TEST_ASSERT_TRUE(
        button_was_pressed(&button)
    );

    /*
     * The same press must not be reported again.
     */
    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );
}

static void
test_release_allows_a_new_press_event(
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
     * First press.
     */
    fake_hal_set_time_ms(100U);

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        false
    );

    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );

    fake_hal_set_time_ms(140U);

    TEST_ASSERT_TRUE(
        button_was_pressed(&button)
    );

    /*
     * Release starts at 200 ms.
     */
    fake_hal_set_time_ms(200U);

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        true
    );

    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );

    /*
     * Release becomes debounced.
     * Releasing never produces a press event.
     */
    fake_hal_set_time_ms(240U);

    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );

    TEST_ASSERT_TRUE(
        button.stable_state
    );

    /*
     * Second press.
     */
    fake_hal_set_time_ms(300U);

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        false
    );

    TEST_ASSERT_FALSE(
        button_was_pressed(&button)
    );

    fake_hal_set_time_ms(340U);

    TEST_ASSERT_TRUE(
        button_was_pressed(&button)
    );
}

static void
test_hold_is_reported_after_required_time(
    void
)
{
    static const uint32_t HOLD_MS = 1000U;

    button_t button = {};

    button_init(
        &button,
        TEST_BUTTON_PIN,
        TEST_DEBOUNCE_MS
    );

    /*
     * Physical press begins.
     */
    fake_hal_set_time_ms(100U);

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        false
    );

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    /*
     * Press becomes debounced at 140 ms.
     * The hold interval begins from this point.
     */
    fake_hal_set_time_ms(140U);

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    fake_hal_set_time_ms(1139U);

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    /*
     * Exactly 1000 ms since the debounced press.
     */
    fake_hal_set_time_ms(1140U);

    TEST_ASSERT_TRUE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );
}

static void
test_hold_is_reported_only_once_while_button_remains_pressed(
    void
)
{
    static const uint32_t HOLD_MS = 1000U;

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

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    fake_hal_set_time_ms(140U);

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    fake_hal_set_time_ms(1140U);

    TEST_ASSERT_TRUE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    /*
     * It remains physically pressed, but the event
     * has already been reported.
     */
    fake_hal_set_time_ms(1200U);

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    fake_hal_set_time_ms(5000U);

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );
}

static void
test_release_allows_a_new_hold_event(
    void
)
{
    static const uint32_t HOLD_MS = 1000U;

    button_t button = {};

    button_init(
        &button,
        TEST_BUTTON_PIN,
        TEST_DEBOUNCE_MS
    );

    /*
     * First long press.
     */
    fake_hal_set_time_ms(100U);

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        false
    );

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    fake_hal_set_time_ms(140U);

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    fake_hal_set_time_ms(1140U);

    TEST_ASSERT_TRUE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    /*
     * Release and debounce it.
     */
    fake_hal_set_time_ms(1200U);

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        true
    );

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    fake_hal_set_time_ms(1240U);

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    /*
     * Second press.
     */
    fake_hal_set_time_ms(1300U);

    fake_hal_set_pin_input(
        TEST_BUTTON_PIN,
        false
    );

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    fake_hal_set_time_ms(1340U);

    TEST_ASSERT_FALSE(
        button_was_held(
            &button,
            HOLD_MS
        )
    );

    fake_hal_set_time_ms(2340U);

    TEST_ASSERT_TRUE(
        button_was_held(
            &button,
            HOLD_MS
        )
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

    RUN_TEST(
        test_contact_bounce_produces_only_one_press_event
    );

    RUN_TEST(
        test_release_allows_a_new_press_event
    );

    RUN_TEST(
        test_hold_is_reported_after_required_time
    );

    RUN_TEST(
        test_hold_is_reported_only_once_while_button_remains_pressed
    );

    RUN_TEST(
        test_release_allows_a_new_hold_event
    );

    RUN_TEST(
        test_debounce_works_across_millisecond_overflow
    );

    RUN_TEST(
        test_null_button_pointer_is_handled_safely
    );

    return UNITY_END();
}