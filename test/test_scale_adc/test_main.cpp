#include <stdint.h>

#include <unity.h>

#include "config.h"
#include "fake_converter_drivers.h"
#include "scale_adc.h"
#include "storage_layout.h"

void setUp(void)
{
    fake_converter_drivers_reset();
}

void tearDown(void)
{
}

static void test_backend_identity_and_default_factor(void)
{
#if SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_HX711

    TEST_ASSERT_EQUAL_STRING(
        "HX711",
        SCALE_ADC_NAME_LITERAL
    );
    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        45.589332F,
        DEFAULT_CALIBRATION_FACTOR
    );

#elif SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_ADS1232

    TEST_ASSERT_EQUAL_STRING(
        "ADS1232",
        SCALE_ADC_NAME_LITERAL
    );
    TEST_ASSERT_FLOAT_WITHIN(
        0.000001F,
        1.0F,
        DEFAULT_CALIBRATION_FACTOR
    );

#endif
}

static void test_backend_uses_distinct_persistent_slots(void)
{
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        (uint32_t)
        HX711_CALIBRATION_STORAGE_ADDRESS
    );
    TEST_ASSERT_EQUAL_UINT32(
        12U,
        (uint32_t)HX711_TARE_STORAGE_ADDRESS
    );
    TEST_ASSERT_EQUAL_UINT32(
        24U,
        (uint32_t)
        ADS1232_CALIBRATION_STORAGE_ADDRESS
    );
    TEST_ASSERT_EQUAL_UINT32(
        36U,
        (uint32_t)ADS1232_TARE_STORAGE_ADDRESS
    );
    TEST_ASSERT_EQUAL_UINT32(
        48U,
        (uint32_t)STORAGE_LAYOUT_TOTAL_CAPACITY
    );

#if SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_HX711

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        (uint32_t)CALIBRATION_STORAGE_ADDRESS
    );
    TEST_ASSERT_EQUAL_UINT32(
        12U,
        (uint32_t)TARE_STORAGE_ADDRESS
    );
    TEST_ASSERT_EQUAL_UINT32(
        24U,
        (uint32_t)STORAGE_LAYOUT_REQUIRED_CAPACITY
    );

#elif SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_ADS1232

    TEST_ASSERT_EQUAL_UINT32(
        24U,
        (uint32_t)CALIBRATION_STORAGE_ADDRESS
    );
    TEST_ASSERT_EQUAL_UINT32(
        36U,
        (uint32_t)TARE_STORAGE_ADDRESS
    );
    TEST_ASSERT_EQUAL_UINT32(
        48U,
        (uint32_t)STORAGE_LAYOUT_REQUIRED_CAPACITY
    );

#endif
}

static void test_init_forwards_selected_backend_configuration(void)
{
    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_OK,
        scale_adc_init()
    );
    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_converter_get_init_call_count()
    );

#if SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_HX711

    TEST_ASSERT_EQUAL_UINT8(
        HX711_DOUT_PIN,
        fake_converter_get_data_pin()
    );
    TEST_ASSERT_EQUAL_UINT8(
        HX711_SCK_PIN,
        fake_converter_get_clock_pin()
    );

#elif SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_ADS1232

    TEST_ASSERT_EQUAL_UINT8(
        ADS1232_DOUT_PIN,
        fake_converter_get_data_pin()
    );
    TEST_ASSERT_EQUAL_UINT8(
        ADS1232_SCLK_PIN,
        fake_converter_get_clock_pin()
    );
    TEST_ASSERT_EQUAL_UINT8(
        ADS1232_PDWN_PIN,
        fake_converter_get_power_down_pin()
    );
    TEST_ASSERT_EQUAL_UINT8(
        ADS1232_GAIN0_PIN,
        fake_converter_get_gain0_pin()
    );
    TEST_ASSERT_EQUAL_UINT8(
        ADS1232_GAIN1_PIN,
        fake_converter_get_gain1_pin()
    );
    TEST_ASSERT_EQUAL_INT(
        ADS1232_GAIN_128,
        fake_converter_get_ads1232_gain()
    );

#endif
}

static void test_init_maps_driver_failure(void)
{
    fake_converter_set_init_success(false);

    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_ERROR,
        scale_adc_init()
    );
    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_converter_get_init_call_count()
    );
}

static void test_ready_state_is_forwarded(void)
{
    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_OK,
        scale_adc_init()
    );

    fake_converter_set_ready(false);
    TEST_ASSERT_FALSE(scale_adc_is_ready());

    fake_converter_set_ready(true);
    TEST_ASSERT_TRUE(scale_adc_is_ready());

    TEST_ASSERT_EQUAL_UINT32(
        2U,
        fake_converter_get_ready_call_count()
    );
}

static void test_read_forwards_raw_value(void)
{
    int32_t raw_value = 0;

    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_OK,
        scale_adc_init()
    );

    fake_converter_set_read_result(
        true,
        -765432
    );

    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_OK,
        scale_adc_read_raw(&raw_value)
    );
    TEST_ASSERT_EQUAL_INT32(-765432, raw_value);
    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_converter_get_read_call_count()
    );
}

static void test_read_maps_failure_and_preserves_output(void)
{
    int32_t raw_value = 123456;

    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_OK,
        scale_adc_init()
    );

    fake_converter_set_read_result(false, 0);

    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_ERROR,
        scale_adc_read_raw(&raw_value)
    );
    TEST_ASSERT_EQUAL_INT32(123456, raw_value);

    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_ERROR,
        scale_adc_read_raw(nullptr)
    );
}

static void test_power_operations_map_success_and_failure(void)
{
    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_OK,
        scale_adc_init()
    );

    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_OK,
        scale_adc_power_down()
    );
    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_OK,
        scale_adc_power_up()
    );

    fake_converter_set_power_down_success(false);
    fake_converter_set_power_up_success(false);

    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_ERROR,
        scale_adc_power_down()
    );
    TEST_ASSERT_EQUAL_INT(
        SCALE_ADC_STATUS_ERROR,
        scale_adc_power_up()
    );

    TEST_ASSERT_EQUAL_UINT32(
        2U,
        fake_converter_get_power_down_call_count()
    );
    TEST_ASSERT_EQUAL_UINT32(
        2U,
        fake_converter_get_power_up_call_count()
    );
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_backend_identity_and_default_factor);
    RUN_TEST(test_backend_uses_distinct_persistent_slots);
    RUN_TEST(test_init_forwards_selected_backend_configuration);
    RUN_TEST(test_init_maps_driver_failure);
    RUN_TEST(test_ready_state_is_forwarded);
    RUN_TEST(test_read_forwards_raw_value);
    RUN_TEST(test_read_maps_failure_and_preserves_output);
    RUN_TEST(test_power_operations_map_success_and_failure);

    return UNITY_END();
}
