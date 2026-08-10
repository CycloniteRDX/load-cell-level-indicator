#include <math.h>

#include <unity.h>

#include "config.h"
#include "fake_hx711_driver.h"
#include "scale.h"


static const float
    FLOAT_COMPARISON_TOLERANCE = 0.0000001F;

static const float
    VALID_CALIBRATION_FACTOR = 45.5F;

static const int32_t
    ESTABLISHED_TARE_OFFSET = 1234;


void setUp(void)
{
    fake_hx711_driver_reset();
    scale_cancel_sample_collection();
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


static void establish_non_default_scale_state(void)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(
            VALID_CALIBRATION_FACTOR
        )
    );

    scale_set_offset(
        ESTABLISHED_TARE_OFFSET
    );

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
test_scale_init_uses_configured_pins_without_waiting(
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
        0U,
        fake_hx711_driver_get_wait_ready_call_count()
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
test_scale_is_ready_forwards_current_ready_state(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    fake_hx711_driver_reset();
    fake_hx711_driver_set_ready(false);

    TEST_ASSERT_FALSE(scale_is_ready());

    fake_hx711_driver_set_ready(true);

    TEST_ASSERT_TRUE(scale_is_ready());

    TEST_ASSERT_EQUAL_UINT32(
        2U,
        fake_hx711_driver_get_is_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_wait_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_read_raw_call_count()
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
        0U,
        fake_hx711_driver_get_wait_ready_call_count()
    );
}


static void
test_failed_reinitialization_preserves_collector_progress(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(2U)
    );

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            100
        )
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_IN_PROGRESS,
        scale_update_sample_collection()
    );

    fake_hx711_driver_set_init_status(
        HX711_STATUS_INVALID_ARGUMENT
    );

    TEST_ASSERT_FALSE(scale_init());

    TEST_ASSERT_FALSE(
        scale_start_sample_collection(1U)
    );

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            300
        )
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_COMPLETE,
        scale_update_sample_collection()
    );

    int32_t average_raw = 0;

    TEST_ASSERT_TRUE(
        scale_take_sample_average(&average_raw)
    );

    TEST_ASSERT_EQUAL_INT32(
        200,
        average_raw
    );
}


static void
test_successful_reinitialization_discards_collector_progress(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(2U)
    );

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            100
        )
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_IN_PROGRESS,
        scale_update_sample_collection()
    );

    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(1U)
    );

    scale_cancel_sample_collection();
}


static void
test_collector_rejects_zero_samples_and_idle_result_take(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_FALSE(
        scale_start_sample_collection(0U)
    );

    const int32_t sentinel = 123456;
    int32_t average_raw = sentinel;

    TEST_ASSERT_FALSE(
        scale_take_sample_average(&average_raw)
    );

    TEST_ASSERT_EQUAL_INT32(
        sentinel,
        average_raw
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_is_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_read_raw_call_count()
    );
}


static void
test_not_ready_update_keeps_collection_in_progress(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    fake_hx711_driver_set_ready(false);

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(1U)
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_IN_PROGRESS,
        scale_update_sample_collection()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_is_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_read_raw_call_count()
    );

    fake_hx711_driver_set_ready(true);

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            4321
        )
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_COMPLETE,
        scale_update_sample_collection()
    );

    int32_t average_raw = 0;

    TEST_ASSERT_TRUE(
        scale_take_sample_average(&average_raw)
    );

    TEST_ASSERT_EQUAL_INT32(
        4321,
        average_raw
    );
}


static void
test_each_update_reads_at_most_one_sample_and_truncates_average(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

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

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(4U)
    );

    for (uint8_t update_index = 0U;
         update_index < 4U;
         ++update_index)
    {
        const scale_sample_collection_status_t
            expected_status =
                (update_index == 3U)
                    ? SCALE_SAMPLE_COLLECTION_COMPLETE
                    : SCALE_SAMPLE_COLLECTION_IN_PROGRESS;

        TEST_ASSERT_EQUAL(
            expected_status,
            scale_update_sample_collection()
        );

        TEST_ASSERT_EQUAL_UINT32(
            (uint32_t)update_index + 1U,
            fake_hx711_driver_get_read_raw_call_count()
        );
    }

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_COMPLETE,
        scale_update_sample_collection()
    );

    TEST_ASSERT_EQUAL_UINT32(
        4U,
        fake_hx711_driver_get_is_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        4U,
        fake_hx711_driver_get_read_raw_call_count()
    );

    int32_t average_raw = 0;

    TEST_ASSERT_TRUE(
        scale_take_sample_average(&average_raw)
    );

    TEST_ASSERT_EQUAL_INT32(
        -7,
        average_raw
    );
}


static void
test_new_collection_requires_idle_collector(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(1U)
    );

    TEST_ASSERT_FALSE(
        scale_start_sample_collection(1U)
    );

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            250
        )
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_COMPLETE,
        scale_update_sample_collection()
    );

    TEST_ASSERT_FALSE(
        scale_start_sample_collection(1U)
    );

    int32_t average_raw = 0;

    TEST_ASSERT_TRUE(
        scale_take_sample_average(&average_raw)
    );

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(1U)
    );

    scale_cancel_sample_collection();
}


static void
test_completed_result_rejects_null_output_and_remains_available(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            -765
        )
    );

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(1U)
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_COMPLETE,
        scale_update_sample_collection()
    );

    TEST_ASSERT_FALSE(
        scale_take_sample_average(NULL)
    );

    TEST_ASSERT_FALSE(
        scale_start_sample_collection(1U)
    );

    int32_t average_raw = 0;

    TEST_ASSERT_TRUE(
        scale_take_sample_average(&average_raw)
    );

    TEST_ASSERT_EQUAL_INT32(
        -765,
        average_raw
    );
}


static void
test_read_errors_are_sticky_at_first_middle_and_final_sample(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(
            VALID_CALIBRATION_FACTOR
        )
    );

    scale_set_offset(ESTABLISHED_TARE_OFFSET);

    const uint8_t successful_before_error[] = {
        0U,
        2U,
        4U
    };

    for (uint8_t scenario = 0U;
         scenario < 3U;
         ++scenario)
    {
        fake_hx711_driver_reset();
        scale_cancel_sample_collection();

        push_successful_readings_before_error(
            successful_before_error[scenario],
            5000,
            HX711_STATUS_TIMEOUT
        );

        TEST_ASSERT_TRUE(
            scale_start_sample_collection(5U)
        );

        for (uint8_t successful = 0U;
             successful <
                successful_before_error[scenario];
             ++successful)
        {
            TEST_ASSERT_EQUAL(
                SCALE_SAMPLE_COLLECTION_IN_PROGRESS,
                scale_update_sample_collection()
            );
        }

        TEST_ASSERT_EQUAL(
            SCALE_SAMPLE_COLLECTION_ERROR,
            scale_update_sample_collection()
        );

        const uint32_t read_count_after_error =
            (uint32_t)
                successful_before_error[scenario] +
            1U;

        TEST_ASSERT_EQUAL_UINT32(
            read_count_after_error,
            fake_hx711_driver_get_read_raw_call_count()
        );

        TEST_ASSERT_EQUAL(
            SCALE_SAMPLE_COLLECTION_ERROR,
            scale_update_sample_collection()
        );

        TEST_ASSERT_EQUAL_UINT32(
            read_count_after_error,
            fake_hx711_driver_get_read_raw_call_count()
        );

        TEST_ASSERT_FALSE(
            scale_start_sample_collection(1U)
        );

        TEST_ASSERT_EQUAL_INT32(
            ESTABLISHED_TARE_OFFSET,
            scale_get_offset()
        );

        assert_calibration_factor_is(
            VALID_CALIBRATION_FACTOR
        );

        scale_cancel_sample_collection();
    }
}


static void
test_cancellation_returns_partial_complete_and_error_to_idle(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(2U)
    );

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            100
        )
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_IN_PROGRESS,
        scale_update_sample_collection()
    );

    scale_cancel_sample_collection();

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(1U)
    );

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            200
        )
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_COMPLETE,
        scale_update_sample_collection()
    );

    scale_cancel_sample_collection();

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(1U)
    );

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_TIMEOUT,
            0
        )
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_ERROR,
        scale_update_sample_collection()
    );

    scale_cancel_sample_collection();

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(1U)
    );

    scale_cancel_sample_collection();
}


static void
test_collector_returns_raw_average_without_applying_tare(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    scale_set_offset(1000);

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            1200
        )
    );

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            1300
        )
    );

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(2U)
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_IN_PROGRESS,
        scale_update_sample_collection()
    );

    TEST_ASSERT_EQUAL(
        SCALE_SAMPLE_COLLECTION_COMPLETE,
        scale_update_sample_collection()
    );

    int32_t average_raw = 0;

    TEST_ASSERT_TRUE(
        scale_take_sample_average(&average_raw)
    );

    TEST_ASSERT_EQUAL_INT32(
        1250,
        average_raw
    );

    TEST_ASSERT_EQUAL_INT32(
        1000,
        scale_get_offset()
    );
}


static void
test_positive_hx711_limit_collector_does_not_overflow(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    const uint8_t sample_count = UINT8_MAX;
    const int32_t maximum_hx711_value = 8388607;

    push_constant_readings(
        sample_count,
        maximum_hx711_value
    );

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(sample_count)
    );

    for (uint16_t sample = 0U;
         sample < sample_count;
         ++sample)
    {
        scale_update_sample_collection();
    }

    int32_t average_raw = 0;

    TEST_ASSERT_TRUE(
        scale_take_sample_average(&average_raw)
    );

    TEST_ASSERT_EQUAL_INT32(
        maximum_hx711_value,
        average_raw
    );
}


static void
test_negative_hx711_limit_collector_does_not_overflow(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    const uint8_t sample_count = UINT8_MAX;
    const int32_t minimum_hx711_value = -8388608;

    push_constant_readings(
        sample_count,
        minimum_hx711_value
    );

    TEST_ASSERT_TRUE(
        scale_start_sample_collection(sample_count)
    );

    for (uint16_t sample = 0U;
         sample < sample_count;
         ++sample)
    {
        scale_update_sample_collection();
    }

    int32_t average_raw = 0;

    TEST_ASSERT_TRUE(
        scale_take_sample_average(&average_raw)
    );

    TEST_ASSERT_EQUAL_INT32(
        minimum_hx711_value,
        average_raw
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

static void
test_scale_set_offset_accepts_zero_positive_and_negative_values(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    fake_hx711_driver_reset();

    scale_set_offset(0);

    TEST_ASSERT_EQUAL_INT32(
        0,
        scale_get_offset()
    );

    scale_set_offset(123456);

    TEST_ASSERT_EQUAL_INT32(
        123456,
        scale_get_offset()
    );

    scale_set_offset(-654321);

    TEST_ASSERT_EQUAL_INT32(
        -654321,
        scale_get_offset()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_read_raw_call_count()
    );
}


static void
test_scale_set_offset_accepts_int32_boundaries(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    scale_set_offset(INT32_MIN);

    TEST_ASSERT_EQUAL_INT32(
        INT32_MIN,
        scale_get_offset()
    );

    scale_set_offset(INT32_MAX);

    TEST_ASSERT_EQUAL_INT32(
        INT32_MAX,
        scale_get_offset()
    );
}


static void
test_restored_offset_is_used_by_weight_reading(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(50.0F)
    );

    scale_set_offset(-1000);

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            0
        )
    );

    float weight_grams = 0.0F;

    TEST_ASSERT_TRUE(
        scale_try_read_weight(
            &weight_grams
        )
    );

    TEST_ASSERT_FLOAT_WITHIN(
        FLOAT_COMPARISON_TOLERANCE,
        20.0F,
        weight_grams
    );
}


static void
assert_weight_is(
    float expected_weight,
    float tolerance
)
{
    float weight_grams = 0.0F;

    TEST_ASSERT_TRUE(
        scale_try_read_weight(&weight_grams)
    );

    TEST_ASSERT_FLOAT_WITHIN(
        tolerance,
        expected_weight,
        weight_grams
    );
}


static void
test_weight_rejects_null_output_without_checking_ready(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_FALSE(
        scale_try_read_weight(NULL)
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_is_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_read_raw_call_count()
    );
}


static void
test_weight_not_ready_preserves_output_without_reading(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    fake_hx711_driver_reset();
    fake_hx711_driver_set_ready(false);

    const float sentinel = 321.75F;
    float weight_grams = sentinel;

    TEST_ASSERT_FALSE(
        scale_try_read_weight(&weight_grams)
    );

    TEST_ASSERT_FLOAT_WITHIN(
        FLOAT_COMPARISON_TOLERANCE,
        sentinel,
        weight_grams
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_is_ready_call_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_driver_get_read_raw_call_count()
    );
}


static void
test_weight_converts_positive_net_counts_to_grams(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());
    scale_set_offset(-170000);

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(46.5F)
    );

    fake_hx711_driver_reset();

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            -100000
        )
    );

    const float expected_weight =
        70000.0F / 46.5F;

    assert_weight_is(
        expected_weight,
        0.001F
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_is_ready_call_count()
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
test_negative_calibration_factor_reverses_weight_sign(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(-45.5F)
    );

    fake_hx711_driver_reset();

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            455
        )
    );

    assert_weight_is(
        -10.0F,
        FLOAT_COMPARISON_TOLERANCE
    );
}


static void
test_negative_net_counts_produce_negative_weight(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());
    scale_set_offset(1000);

    TEST_ASSERT_TRUE(
        scale_set_calibration_factor(45.5F)
    );

    fake_hx711_driver_reset();

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_OK,
            545
        )
    );

    assert_weight_is(
        -10.0F,
        FLOAT_COMPARISON_TOLERANCE
    );
}


static void
test_weight_read_error_after_ready_preserves_output(
    void
)
{
    TEST_ASSERT_TRUE(scale_init());

    fake_hx711_driver_reset();

    TEST_ASSERT_TRUE(
        fake_hx711_driver_push_reading(
            HX711_STATUS_TIMEOUT,
            0
        )
    );

    const float sentinel = -987.25F;
    float weight_grams = sentinel;

    TEST_ASSERT_FALSE(
        scale_try_read_weight(&weight_grams)
    );

    TEST_ASSERT_FLOAT_WITHIN(
        FLOAT_COMPARISON_TOLERANCE,
        sentinel,
        weight_grams
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_driver_get_is_ready_call_count()
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


int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(
        test_scale_init_uses_configured_pins_without_waiting
    );

    RUN_TEST(
        test_scale_init_failure_skips_wait_ready
    );

    RUN_TEST(
        test_scale_is_ready_forwards_current_ready_state
    );

    RUN_TEST(
        test_successful_reinitialization_resets_scale_state
    );

    RUN_TEST(
        test_hx711_init_failure_preserves_scale_state
    );

    RUN_TEST(
        test_failed_reinitialization_preserves_collector_progress
    );

    RUN_TEST(
        test_successful_reinitialization_discards_collector_progress
    );

    RUN_TEST(
        test_collector_rejects_zero_samples_and_idle_result_take
    );

    RUN_TEST(
        test_not_ready_update_keeps_collection_in_progress
    );

    RUN_TEST(
        test_each_update_reads_at_most_one_sample_and_truncates_average
    );

    RUN_TEST(
        test_new_collection_requires_idle_collector
    );

    RUN_TEST(
        test_completed_result_rejects_null_output_and_remains_available
    );

    RUN_TEST(
        test_read_errors_are_sticky_at_first_middle_and_final_sample
    );

    RUN_TEST(
        test_cancellation_returns_partial_complete_and_error_to_idle
    );

    RUN_TEST(
        test_collector_returns_raw_average_without_applying_tare
    );

    RUN_TEST(
        test_positive_hx711_limit_collector_does_not_overflow
    );

    RUN_TEST(
        test_negative_hx711_limit_collector_does_not_overflow
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
        test_scale_set_offset_accepts_zero_positive_and_negative_values
    );

    RUN_TEST(
        test_scale_set_offset_accepts_int32_boundaries
    );

    RUN_TEST(
        test_restored_offset_is_used_by_weight_reading
    );



    RUN_TEST(
        test_weight_rejects_null_output_without_checking_ready
    );

    RUN_TEST(
        test_weight_not_ready_preserves_output_without_reading
    );

    RUN_TEST(
        test_weight_converts_positive_net_counts_to_grams
    );

    RUN_TEST(
        test_negative_calibration_factor_reverses_weight_sign
    );

    RUN_TEST(
        test_negative_net_counts_produce_negative_weight
    );

    RUN_TEST(
        test_weight_read_error_after_ready_preserves_output
    );

    return UNITY_END();
}
