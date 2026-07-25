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

static void establish_tare_offset(
    int32_t tare_value
)
{
    TEST_ASSERT_TRUE(scale_init());

    push_constant_readings(
        TARE_SAMPLES,
        tare_value
    );

    scale_tare();

    TEST_ASSERT_EQUAL_INT32(
        tare_value,
        scale_get_offset()
    );
}


static void
push_successful_readings_before_error(
    uint8_t successful_reading_count,
    int32_t raw_value,
    hx711_status_t error_status
)
{
    push_constant_readings(
        successful_reading_count,
        raw_value
    );

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            error_status,
            0
        )
    );
}


static void
assert_net_count_output_unchanged_after_failure(
    float expected_sentinel,
    uint8_t samples
)
{
    float net_counts = expected_sentinel;

    TEST_ASSERT_FALSE(
        scale_read_net_counts(
            &net_counts,
            samples
        )
    );

    TEST_ASSERT_FLOAT_WITHIN(
        FLOAT_COMPARISON_TOLERANCE,
        expected_sentinel,
        net_counts
    );
}


static void
test_successful_tare_averages_all_samples(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    for (uint8_t sample = 0U;
         sample < (TARE_SAMPLES / 2U);
         ++sample)
    {
        TEST_ASSERT_TRUE(
            fake_hx711_driver_push_reading(
                HX711_STATUS_OK,
                1200
            )
        );

        TEST_ASSERT_TRUE(
            fake_hx711_driver_push_reading(
                HX711_STATUS_OK,
                -800
            )
        );
    }

    scale_tare();

    TEST_ASSERT_EQUAL_INT32(
        200,
        scale_get_offset()
    );

    TEST_ASSERT_EQUAL_UINT32(
        TARE_SAMPLES,
        fake_hx711_driver_get_read_raw_call_count()
    );

    TEST_ASSERT_EQUAL_UINT16(
        TARE_SAMPLES,
        fake_hx711_driver_get_consumed_reading_count()
    );

    TEST_ASSERT_FALSE(
        fake_hx711_driver_sequence_was_exhausted()
    );
}


static void
test_tare_failure_on_first_read_preserves_offset(
    void
)
{
    establish_tare_offset(
        ESTABLISHED_TARE_OFFSET
    );

    fake_hx711_driver_reset();

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_TIMEOUT,
            0
        )
    );

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            9999
        )
    );

    scale_tare();

    TEST_ASSERT_EQUAL_INT32(
        ESTABLISHED_TARE_OFFSET,
        scale_get_offset()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_read_raw_call_count()
    );

    TEST_ASSERT_EQUAL_UINT16(
        1U,
        fake_hx711_driver_get_consumed_reading_count()
    );
}


static void
test_tare_failure_in_middle_preserves_offset(
    void
)
{
    establish_tare_offset(
        ESTABLISHED_TARE_OFFSET
    );

    fake_hx711_driver_reset();

    push_successful_readings_before_error(
        7U,
        5000,
        HX711_STATUS_TIMEOUT
    );

    push_constant_readings(
        3U,
        6000
    );

    scale_tare();

    TEST_ASSERT_EQUAL_INT32(
        ESTABLISHED_TARE_OFFSET,
        scale_get_offset()
    );

    TEST_ASSERT_EQUAL_UINT32(
        8U,
        fake_hx711_driver_get_read_raw_call_count()
    );

    TEST_ASSERT_EQUAL_UINT16(
        8U,
        fake_hx711_driver_get_consumed_reading_count()
    );
}


static void
test_tare_failure_on_final_read_preserves_offset(
    void
)
{
    establish_tare_offset(
        ESTABLISHED_TARE_OFFSET
    );

    fake_hx711_driver_reset();

    push_successful_readings_before_error(
        (uint8_t)(TARE_SAMPLES - 1U),
        7000,
        HX711_STATUS_TIMEOUT
    );

    scale_tare();

    TEST_ASSERT_EQUAL_INT32(
        ESTABLISHED_TARE_OFFSET,
        scale_get_offset()
    );

    TEST_ASSERT_EQUAL_UINT32(
        TARE_SAMPLES,
        fake_hx711_driver_get_read_raw_call_count()
    );

    TEST_ASSERT_EQUAL_UINT16(
        TARE_SAMPLES,
        fake_hx711_driver_get_consumed_reading_count()
    );
}


static void
test_repeated_successful_tare_replaces_offset(
    void
)
{
    establish_tare_offset(1000);

    fake_hx711_driver_reset();

    push_constant_readings(
        TARE_SAMPLES,
        -250
    );

    scale_tare();

    TEST_ASSERT_EQUAL_INT32(
        -250,
        scale_get_offset()
    );

    TEST_ASSERT_EQUAL_UINT32(
        TARE_SAMPLES,
        fake_hx711_driver_get_read_raw_call_count()
    );
}


static void
test_net_counts_rejects_null_output_without_reading(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_FALSE(
        scale_read_net_counts(
            NULL,
            1U
        )
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_read_raw_call_count()
    );
}


static void
test_net_counts_rejects_zero_samples_without_reading(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    const float sentinel = 123.25F;
    float net_counts = sentinel;

    TEST_ASSERT_FALSE(
        scale_read_net_counts(
            &net_counts,
            0U
        )
    );

    TEST_ASSERT_FLOAT_WITHIN(
        FLOAT_COMPARISON_TOLERANCE,
        sentinel,
        net_counts
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_read_raw_call_count()
    );
}


static void
test_single_sample_net_counts_subtracts_tare(
    void
)
{
    establish_tare_offset(1000);

    fake_hx711_driver_reset();

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            1250
        )
    );

    float net_counts = 0.0F;

    TEST_ASSERT_TRUE(
        scale_read_net_counts(
            &net_counts,
            1U
        )
    );

    TEST_ASSERT_FLOAT_WITHIN(
        FLOAT_COMPARISON_TOLERANCE,
        250.0F,
        net_counts
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_read_raw_call_count()
    );
}


static void
test_multiple_net_count_samples_use_truncated_average(
    void
)
{
    establish_tare_offset(-100);

    fake_hx711_driver_reset();

    const int32_t readings[] = {
        -9,
        -8,
        -7,
        -6
    };

    for (uint8_t index = 0U;
         index < 4U;
         ++index)
    {
        TEST_ASSERT_TRUE(
            fake_hx711_driver_push_reading(
                HX711_STATUS_OK,
                readings[index]
            )
        );
    }

    float net_counts = 0.0F;

    TEST_ASSERT_TRUE(
        scale_read_net_counts(
            &net_counts,
            4U
        )
    );

    /*
     * (-9 - 8 - 7 - 6) / 4 = -30 / 4.
     * Integer division truncates toward zero: -7.
     * Net counts: -7 - (-100) = 93.
     */
    TEST_ASSERT_FLOAT_WITHIN(
        FLOAT_COMPARISON_TOLERANCE,
        93.0F,
        net_counts
    );

    TEST_ASSERT_EQUAL_UINT32(
        4U,
        fake_hx711_driver_get_read_raw_call_count()
    );
}


static void
test_net_count_failure_on_first_read_preserves_output(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_TIMEOUT,
            0
        )
    );

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            500
        )
    );

    assert_net_count_output_unchanged_after_failure(
        456.75F,
        2U
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_read_raw_call_count()
    );

    TEST_ASSERT_EQUAL_UINT16(
        1U,
        fake_hx711_driver_get_consumed_reading_count()
    );
}


static void
test_net_count_failure_in_middle_preserves_output(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    push_successful_readings_before_error(
        2U,
        500,
        HX711_STATUS_TIMEOUT
    );

    push_constant_readings(
        2U,
        700
    );

    assert_net_count_output_unchanged_after_failure(
        -321.5F,
        5U
    );

    TEST_ASSERT_EQUAL_UINT32(
        3U,
        fake_hx711_driver_get_read_raw_call_count()
    );

    TEST_ASSERT_EQUAL_UINT16(
        3U,
        fake_hx711_driver_get_consumed_reading_count()
    );
}


static void
test_net_count_failure_on_final_read_preserves_output(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    push_successful_readings_before_error(
        3U,
        -500,
        HX711_STATUS_TIMEOUT
    );

    assert_net_count_output_unchanged_after_failure(
        88.0F,
        4U
    );

    TEST_ASSERT_EQUAL_UINT32(
        4U,
        fake_hx711_driver_get_read_raw_call_count()
    );

    TEST_ASSERT_EQUAL_UINT16(
        4U,
        fake_hx711_driver_get_consumed_reading_count()
    );
}


static void
test_positive_hx711_limit_averages_without_overflow(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    const uint8_t maximum_sample_count = UINT8_MAX;
    const int32_t maximum_hx711_value = 8388607;

    push_constant_readings(
        maximum_sample_count,
        maximum_hx711_value
    );

    float net_counts = 0.0F;

    TEST_ASSERT_TRUE(
        scale_read_net_counts(
            &net_counts,
            maximum_sample_count
        )
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.5F,
        (float)maximum_hx711_value,
        net_counts
    );

    TEST_ASSERT_EQUAL_UINT32(
        maximum_sample_count,
        fake_hx711_driver_get_read_raw_call_count()
    );
}


static void
test_negative_hx711_limit_averages_without_overflow(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    const uint8_t maximum_sample_count = UINT8_MAX;
    const int32_t minimum_hx711_value = -8388608;

    push_constant_readings(
        maximum_sample_count,
        minimum_hx711_value
    );

    float net_counts = 0.0F;

    TEST_ASSERT_TRUE(
        scale_read_net_counts(
            &net_counts,
            maximum_sample_count
        )
    );

    TEST_ASSERT_FLOAT_WITHIN(
        0.5F,
        (float)minimum_hx711_value,
        net_counts
    );

    TEST_ASSERT_EQUAL_UINT32(
        maximum_sample_count,
        fake_hx711_driver_get_read_raw_call_count()
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


    RUN_TEST(
        test_successful_tare_averages_all_samples
    );

    RUN_TEST(
        test_tare_failure_on_first_read_preserves_offset
    );

    RUN_TEST(
        test_tare_failure_in_middle_preserves_offset
    );

    RUN_TEST(
        test_tare_failure_on_final_read_preserves_offset
    );

    RUN_TEST(
        test_repeated_successful_tare_replaces_offset
    );

    RUN_TEST(
        test_net_counts_rejects_null_output_without_reading
    );

    RUN_TEST(
        test_net_counts_rejects_zero_samples_without_reading
    );

    RUN_TEST(
        test_single_sample_net_counts_subtracts_tare
    );

    RUN_TEST(
        test_multiple_net_count_samples_use_truncated_average
    );

    RUN_TEST(
        test_net_count_failure_on_first_read_preserves_output
    );

    RUN_TEST(
        test_net_count_failure_in_middle_preserves_output
    );

    RUN_TEST(
        test_net_count_failure_on_final_read_preserves_output
    );

    RUN_TEST(
        test_positive_hx711_limit_averages_without_overflow
    );

    RUN_TEST(
        test_negative_hx711_limit_averages_without_overflow
    );

    return UNITY_END();
}