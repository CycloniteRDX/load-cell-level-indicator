#include <unity.h>

#include "config.h"
#include "fake_hx711_driver.h"
#include "scale.h"


static const uint32_t
    EXPECTED_INITIAL_READY_TIMEOUT_MS = 2000U;


void setUp(void)
{
    fake_hx711_driver_reset();
}


void tearDown(void)
{
}


static void
test_scale_init_uses_configured_pins_and_timeout(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_init_call_count()
    );

    TEST_ASSERT_EQUAL_UINT8(
        LOADCELL_DOUT_PIN,
        fake_hx711_driver_get_last_data_pin()
    );

    TEST_ASSERT_EQUAL_UINT8(
        LOADCELL_SCK_PIN,
        fake_hx711_driver_get_last_clock_pin()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_wait_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        EXPECTED_INITIAL_READY_TIMEOUT_MS,
        fake_hx711_driver_get_last_timeout_ms()
    );
}


static void
test_scale_init_failure_skips_wait_ready(
    void
)
{
    fake_hx711_driver_set_init_status(
        HX711_STATUS_INVALID_ARGUMENT
    );

    TEST_ASSERT_FALSE(scale_init());

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_init_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_wait_ready_call_count()
    );
}


static void
test_scale_init_returns_false_after_ready_timeout(
    void
)
{
    fake_hx711_driver_set_wait_ready_status(
        HX711_STATUS_TIMEOUT
    );

    TEST_ASSERT_FALSE(scale_init());

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_init_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_wait_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        EXPECTED_INITIAL_READY_TIMEOUT_MS,
        fake_hx711_driver_get_last_timeout_ms()
    );
}


static void
test_successful_reinitialization_resets_scale_state(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(45.5F)
    );

    for (uint8_t sample = 0U;
         sample < TARE_SAMPLES;
         ++sample)
    {
        TEST_ASSERT_TRUE(
            fake_hx711_driver_push_reading(
                HX711_STATUS_OK,
                1234
            )
        );
    }

    scale_tare();

    TEST_ASSERT_EQUAL_INT32(
        1234,
        scale_get_offset()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        45.5F,
        scale_get_calibration_factor()
    );

    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_EQUAL_INT32(
        0,
        scale_get_offset()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        1.0F,
        scale_get_calibration_factor()
    );
}


int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(
        test_scale_init_uses_configured_pins_and_timeout
    );

    RUN_TEST(
        test_scale_init_failure_skips_wait_ready
    );

    RUN_TEST(
        test_scale_init_returns_false_after_ready_timeout
    );

    RUN_TEST(
        test_successful_reinitialization_resets_scale_state
    );

    return UNITY_END();
}
