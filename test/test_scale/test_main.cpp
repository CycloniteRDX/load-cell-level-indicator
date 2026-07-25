#include <math.h>

#include <unity.h>

#include "config.h"
#include "fake_hx711_driver.h"
#include "scale.h"


static const uint32_t
    EXPECTED_INITIAL_READY_TIMEOUT_MS = 2000U;

static const float
    FLOAT_COMPARISON_TOLERANCE = 0.0000001F;

static const float
    VALID_CALIBRATION_FACTOR = 45.5F;

static const int32_t
    ESTABLISHED_TARE_OFFSET = 1234;


void setUp(void)
{
    fake_hx711_driver_reset();
}


void tearDown(void)
{
}


static void push_constant_readings(
    uint8_t sample_count,
    int32_t raw_value
)
{
    for (uint8_t sample = 0U;
         sample < sample_count;
         ++sample)
    {
        TEST_ASSERT_TRUE(
            fake_hx711_driver_push_reading(
                HX711_STATUS_OK,
                raw_value
            )
        );
    }
}


static void establish_non_default_scale_state(void)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(
            VALID_CALIBRATION_FACTOR
        )
    );

    push_constant_readings(
        TARE_SAMPLES,
        ESTABLISHED_TARE_OFFSET
    );

    scale_tare();

    TEST_ASSERT_EQUAL_INT32(
        ESTABLISHED_TARE_OFFSET,
        scale_get_offset()
    );

    TEST_ASSERT_FLOAT_WITHIN(
        FLOAT_COMPARISON_TOLERANCE,
        VALID_CALIBRATION_FACTOR,
        scale_get_calibration_factor()
    );
}


static void assert_calibration_factor_is(
    float expected_factor
)
{
    TEST_ASSERT_FLOAT_WITHIN(
        FLOAT_COMPARISON_TOLERANCE,
        expected_factor,
        scale_get_calibration_factor()
    );
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
    establish_non_default_scale_state();

    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_EQUAL_INT32(
        0,
        scale_get_offset()
    );

    assert_calibration_factor_is(1.0F);
}


static void
test_hx711_init_failure_preserves_scale_state(
    void
)
{
    establish_non_default_scale_state();

    fake_hx711_driver_set_init_status(
        HX711_STATUS_INVALID_ARGUMENT
    );

    TEST_ASSERT_FALSE(scale_init());

    TEST_ASSERT_EQUAL_INT32(
        ESTABLISHED_TARE_OFFSET,
        scale_get_offset()
    );

    assert_calibration_factor_is(
        VALID_CALIBRATION_FACTOR
    );

    TEST_ASSERT_EQUAL_UINT32(
        2U,
        fake_hx711_driver_get_init_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_wait_ready_call_count()
    );
}


static void
test_ready_timeout_preserves_scale_state(
    void
)
{
    establish_non_default_scale_state();

    fake_hx711_driver_set_wait_ready_status(
        HX711_STATUS_TIMEOUT
    );

    TEST_ASSERT_FALSE(scale_init());

    TEST_ASSERT_EQUAL_INT32(
        ESTABLISHED_TARE_OFFSET,
        scale_get_offset()
    );

    assert_calibration_factor_is(
        VALID_CALIBRATION_FACTOR
    );

    TEST_ASSERT_EQUAL_UINT32(
        2U,
        fake_hx711_driver_get_init_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        2U,
        fake_hx711_driver_get_wait_ready_call_count()
    );
}


static void
test_positive_calibration_factors_are_accepted(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(1.0F)
    );
    assert_calibration_factor_is(1.0F);

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(45.5F)
    );
    assert_calibration_factor_is(45.5F);
}


static void
test_negative_calibration_factors_are_accepted(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(-1.0F)
    );
    assert_calibration_factor_is(-1.0F);

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(-45.5F)
    );
    assert_calibration_factor_is(-45.5F);
}


static void
test_positive_and_negative_zero_are_rejected(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(
            VALID_CALIBRATION_FACTOR
        )
    );

    TEST_ASSERT_FALSE(
        scale_set_calibration_factor(0.0F)
    );
    assert_calibration_factor_is(
        VALID_CALIBRATION_FACTOR
    );

    TEST_ASSERT_FALSE(
        scale_set_calibration_factor(-0.0F)
    );
    assert_calibration_factor_is(
        VALID_CALIBRATION_FACTOR
    );
}


static void
test_non_finite_calibration_factors_are_rejected(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(
            VALID_CALIBRATION_FACTOR
        )
    );

    TEST_ASSERT_FALSE(
        scale_set_calibration_factor(NAN)
    );
    assert_calibration_factor_is(
        VALID_CALIBRATION_FACTOR
    );

    TEST_ASSERT_FALSE(
        scale_set_calibration_factor(INFINITY)
    );
    assert_calibration_factor_is(
        VALID_CALIBRATION_FACTOR
    );

    TEST_ASSERT_FALSE(
        scale_set_calibration_factor(-INFINITY)
    );
    assert_calibration_factor_is(
        VALID_CALIBRATION_FACTOR
    );
}


static void
test_factors_below_minimum_magnitude_are_rejected(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(
            VALID_CALIBRATION_FACTOR
        )
    );

    TEST_ASSERT_FALSE(
        scale_set_calibration_factor(0.0000005F)
    );
    assert_calibration_factor_is(
        VALID_CALIBRATION_FACTOR
    );

    TEST_ASSERT_FALSE(
        scale_set_calibration_factor(-0.0000005F)
    );
    assert_calibration_factor_is(
        VALID_CALIBRATION_FACTOR
    );
}


static void
test_minimum_magnitude_boundary_is_accepted(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(0.000001F)
    );
    assert_calibration_factor_is(0.000001F);

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(-0.000001F)
    );
    assert_calibration_factor_is(-0.000001F);
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

    RUN_TEST(
        test_hx711_init_failure_preserves_scale_state
    );

    RUN_TEST(
        test_ready_timeout_preserves_scale_state
    );

    RUN_TEST(
        test_positive_calibration_factors_are_accepted
    );

    RUN_TEST(
        test_negative_calibration_factors_are_accepted
    );

    RUN_TEST(
        test_positive_and_negative_zero_are_rejected
    );

    RUN_TEST(
        test_non_finite_calibration_factors_are_rejected
    );

    RUN_TEST(
        test_factors_below_minimum_magnitude_are_rejected
    );

    RUN_TEST(
        test_minimum_magnitude_boundary_is_accepted
    );

    return UNITY_END();
}