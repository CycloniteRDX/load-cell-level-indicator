#include <unity.h>

#include "fake_hx711_platform.h"
#include "hx711_driver.h"


static const uint8_t TEST_DATA_PIN = 2U;
static const uint8_t TEST_CLOCK_PIN = 3U;


void setUp(void)
{
    fake_hx711_platform_reset();
}


void tearDown(void)
{
}


static void
test_init_configures_pins_and_default_gain(void)
{
    hx711_t device;

    const hx711_status_t status =
        hx711_init(
            &device,
            TEST_DATA_PIN,
            TEST_CLOCK_PIN
        );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        status
    );

    TEST_ASSERT_TRUE(
        device.initialized
    );

    TEST_ASSERT_EQUAL_UINT8(
        TEST_DATA_PIN,
        device.data_pin
    );

    TEST_ASSERT_EQUAL_UINT8(
        TEST_CLOCK_PIN,
        device.clock_pin
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_GAIN_A_128,
        device.gain
    );

    TEST_ASSERT_EQUAL_INT(
        FAKE_HX711_PIN_INPUT,
        fake_hx711_platform_get_pin_mode(
            TEST_DATA_PIN
        )
    );

    TEST_ASSERT_EQUAL_INT(
        FAKE_HX711_PIN_OUTPUT,
        fake_hx711_platform_get_pin_mode(
            TEST_CLOCK_PIN
        )
    );

    TEST_ASSERT_FALSE(
        fake_hx711_platform_get_pin_output(
            TEST_CLOCK_PIN
        )
    );
}


static void
test_is_ready_reflects_dout_state(void)
{
    hx711_t device;

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_init(
            &device,
            TEST_DATA_PIN,
            TEST_CLOCK_PIN
        )
    );

    fake_hx711_platform_set_ready(false);

    TEST_ASSERT_FALSE(
        hx711_is_ready(&device)
    );

    fake_hx711_platform_set_ready(true);

    TEST_ASSERT_TRUE(
        hx711_is_ready(&device)
    );
}


static void
test_wait_ready_returns_timeout(void)
{
    hx711_t device;

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_init(
            &device,
            TEST_DATA_PIN,
            TEST_CLOCK_PIN
        )
    );

    fake_hx711_platform_set_ready(false);

    /*
     * Advance the simulated clock by 1 ms every
     * time the driver requests the current time.
     */
    fake_hx711_platform_set_time_ms(0U);
    fake_hx711_platform_set_millis_step(1U);

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_TIMEOUT,
        hx711_wait_ready(
            &device,
            5U
        )
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_platform_get_critical_enter_count()
    );
}


static void
test_read_raw_reconstructs_positive_value(void)
{
    hx711_t device;
    int32_t raw_value = 0;

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_init(
            &device,
            TEST_DATA_PIN,
            TEST_CLOCK_PIN
        )
    );

    fake_hx711_platform_set_ready(true);

    fake_hx711_platform_load_raw_24(
        0x00123456UL
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_read_raw(
            &device,
            &raw_value
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        (int32_t)0x00123456L,
        raw_value
    );
}


static void
test_read_raw_sign_extends_negative_value(void)
{
    hx711_t device;
    int32_t raw_value = 0;

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_init(
            &device,
            TEST_DATA_PIN,
            TEST_CLOCK_PIN
        )
    );

    fake_hx711_platform_set_ready(true);

    /*
     * 0xFFFFFF is -1 in signed 24-bit
     * two's-complement representation.
     */
    fake_hx711_platform_load_raw_24(
        0x00FFFFFFUL
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_read_raw(
            &device,
            &raw_value
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        -1,
        raw_value
    );
}


static void
test_read_uses_25_pulses_and_restores_critical_state(
    void
)
{
    hx711_t device;
    int32_t raw_value = 0;

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_init(
            &device,
            TEST_DATA_PIN,
            TEST_CLOCK_PIN
        )
    );

    fake_hx711_platform_set_ready(true);

    fake_hx711_platform_load_raw_24(
        0x00010203UL
    );

    fake_hx711_platform_set_critical_state(
        (uintptr_t)0xA5U
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_read_raw(
            &device,
            &raw_value
        )
    );

    /*
     * Channel A, gain 128:
     *
     * 24 data pulses + 1 gain pulse.
     */
    TEST_ASSERT_EQUAL_UINT8(
        25U,
        fake_hx711_platform_get_last_pulse_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_platform_get_critical_enter_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_platform_get_critical_exit_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0xA5U,
        (uint32_t)
        fake_hx711_platform_get_restored_critical_state()
    );
}


int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(
        test_init_configures_pins_and_default_gain
    );

    RUN_TEST(
        test_is_ready_reflects_dout_state
    );

    RUN_TEST(
        test_wait_ready_returns_timeout
    );

    RUN_TEST(
        test_read_raw_reconstructs_positive_value
    );

    RUN_TEST(
        test_read_raw_sign_extends_negative_value
    );

    RUN_TEST(
        test_read_uses_25_pulses_and_restores_critical_state
    );

    return UNITY_END();
}