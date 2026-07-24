#include <unity.h>
#include <stdint.h>
#include <stddef.h>

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

static void
test_invalid_arguments_are_rejected(void)
{
    hx711_t device = {0};
    int32_t raw_value = 0;

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_INVALID_ARGUMENT,
        hx711_init(
            NULL,
            TEST_DATA_PIN,
            TEST_CLOCK_PIN
        )
    );

    TEST_ASSERT_FALSE(
        hx711_is_ready(NULL)
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_INVALID_ARGUMENT,
        hx711_wait_ready(
            NULL,
            100U
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_INVALID_ARGUMENT,
        hx711_read_raw(
            NULL,
            &raw_value
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_INVALID_ARGUMENT,
        hx711_read_raw(
            &device,
            NULL
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_INVALID_ARGUMENT,
        hx711_set_gain(
            NULL,
            HX711_GAIN_A_128
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_INVALID_ARGUMENT,
        hx711_power_down(NULL)
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_INVALID_ARGUMENT,
        hx711_power_up(NULL)
    );
}

static void
test_uninitialized_device_is_rejected(void)
{
    hx711_t device = {0};
    int32_t raw_value = 0;

    TEST_ASSERT_FALSE(
        hx711_is_ready(&device)
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_NOT_INITIALIZED,
        hx711_wait_ready(
            &device,
            100U
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_NOT_INITIALIZED,
        hx711_read_raw(
            &device,
            &raw_value
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_NOT_INITIALIZED,
        hx711_set_gain(
            &device,
            HX711_GAIN_B_32
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_NOT_INITIALIZED,
        hx711_power_down(&device)
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_NOT_INITIALIZED,
        hx711_power_up(&device)
    );
}

static void
test_read_raw_sign_extends_minimum_negative_value(
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

    /*
     * 0x800000 represents -8388608 in signed
     * 24-bit two's-complement representation.
     */
    fake_hx711_platform_load_raw_24(
        0x00800000UL
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_read_raw(
            &device,
            &raw_value
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        -8388608L,
        raw_value
    );
}

static void
test_read_raw_timeout_does_not_modify_output_or_enter_critical(
    void
)
{
    hx711_t device;
    int32_t raw_value = 123456;

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_init(
            &device,
            TEST_DATA_PIN,
            TEST_CLOCK_PIN
        )
    );

    fake_hx711_platform_set_ready(false);
    fake_hx711_platform_set_time_ms(0U);
    fake_hx711_platform_set_millis_step(1U);

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_TIMEOUT,
        hx711_read_raw(
            &device,
            &raw_value
        )
    );

    TEST_ASSERT_EQUAL_INT32(
        123456,
        raw_value
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_platform_get_critical_enter_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_platform_get_critical_exit_count()
    );
}

static void
test_setting_current_gain_does_not_discard_a_reading(
    void
)
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

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_set_gain(
            &device,
            HX711_GAIN_A_128
        )
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_platform_get_critical_enter_count()
    );

    TEST_ASSERT_EQUAL_UINT8(
        0U,
        fake_hx711_platform_get_last_pulse_count()
    );
}

static void
test_invalid_gain_is_rejected(void)
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

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_INVALID_ARGUMENT,
        hx711_set_gain(
            &device,
            (hx711_gain_t)99
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_GAIN_A_128,
        device.gain
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_platform_get_critical_enter_count()
    );
}

static void
test_set_gain_b_32_generates_26_pulses(void)
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

    fake_hx711_platform_set_ready(true);
    fake_hx711_platform_load_raw_24(0U);

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_set_gain(
            &device,
            HX711_GAIN_B_32
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_GAIN_B_32,
        device.gain
    );

    TEST_ASSERT_EQUAL_UINT8(
        26U,
        fake_hx711_platform_get_last_pulse_count()
    );
}

static void
test_set_gain_a_64_generates_27_pulses(void)
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

    fake_hx711_platform_set_ready(true);
    fake_hx711_platform_load_raw_24(0U);

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_set_gain(
            &device,
            HX711_GAIN_A_64
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_GAIN_A_64,
        device.gain
    );

    TEST_ASSERT_EQUAL_UINT8(
        27U,
        fake_hx711_platform_get_last_pulse_count()
    );
}

static void
test_set_gain_restores_previous_gain_after_timeout(
    void
)
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
    fake_hx711_platform_set_time_ms(0U);
    fake_hx711_platform_set_millis_step(1U);

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_TIMEOUT,
        hx711_set_gain(
            &device,
            HX711_GAIN_B_32
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_GAIN_A_128,
        device.gain
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_platform_get_critical_enter_count()
    );
}

static void
test_power_down_leaves_clock_high_after_70_us_delay(
    void
)
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

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_power_down(&device)
    );

    TEST_ASSERT_TRUE(
        fake_hx711_platform_get_pin_output(
            TEST_CLOCK_PIN
        )
    );

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_hx711_platform_get_delay_call_count()
    );

    TEST_ASSERT_EQUAL_UINT16(
        70U,
        fake_hx711_platform_get_last_delay_us()
    );
}

static void
test_power_up_with_default_gain_pulls_clock_low_without_reading(
    void
)
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

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_power_down(&device)
    );

    TEST_ASSERT_TRUE(
        fake_hx711_platform_get_pin_output(
            TEST_CLOCK_PIN
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_power_up(&device)
    );

    TEST_ASSERT_FALSE(
        fake_hx711_platform_get_pin_output(
            TEST_CLOCK_PIN
        )
    );

    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_hx711_platform_get_critical_enter_count()
    );
}

static void
test_power_up_reapplies_non_default_gain(
    void
)
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

    fake_hx711_platform_set_ready(true);
    fake_hx711_platform_load_raw_24(0U);

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_set_gain(
            &device,
            HX711_GAIN_B_32
        )
    );

    const uint32_t critical_count_before_power_up =
        fake_hx711_platform_get_critical_enter_count();

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_power_down(&device)
    );

    TEST_ASSERT_TRUE(
        fake_hx711_platform_get_pin_output(
            TEST_CLOCK_PIN
        )
    );

    fake_hx711_platform_load_raw_24(0U);

    TEST_ASSERT_EQUAL_INT(
        HX711_STATUS_OK,
        hx711_power_up(&device)
    );

    TEST_ASSERT_FALSE(
        fake_hx711_platform_get_pin_output(
            TEST_CLOCK_PIN
        )
    );

    TEST_ASSERT_EQUAL_INT(
        HX711_GAIN_B_32,
        device.gain
    );

    TEST_ASSERT_EQUAL_UINT8(
        26U,
        fake_hx711_platform_get_last_pulse_count()
    );

    TEST_ASSERT_EQUAL_UINT32(
        critical_count_before_power_up + 1U,
        fake_hx711_platform_get_critical_enter_count()
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

    RUN_TEST(
        test_invalid_arguments_are_rejected
    );

    RUN_TEST(
        test_uninitialized_device_is_rejected
    );

    RUN_TEST(
        test_read_raw_sign_extends_minimum_negative_value
    );

    RUN_TEST(
        test_read_raw_timeout_does_not_modify_output_or_enter_critical
    );

    RUN_TEST(
        test_setting_current_gain_does_not_discard_a_reading
    );

    RUN_TEST(
        test_invalid_gain_is_rejected
    );

    RUN_TEST(
        test_set_gain_b_32_generates_26_pulses
    );

    RUN_TEST(
        test_set_gain_a_64_generates_27_pulses
    );

    RUN_TEST(
        test_set_gain_restores_previous_gain_after_timeout
    );

    RUN_TEST(
        test_power_down_leaves_clock_high_after_70_us_delay
    );

    RUN_TEST(
        test_power_up_with_default_gain_pulls_clock_low_without_reading
    );

    RUN_TEST(
        test_power_up_reapplies_non_default_gain
    );

    return UNITY_END();
}