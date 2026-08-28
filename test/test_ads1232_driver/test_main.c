#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <unity.h>

#include "ads1232_driver.h"
#include "fake_ads1232_platform.h"

static const uint8_t TEST_DATA_PIN = 2U;
static const uint8_t TEST_CLOCK_PIN = 3U;
static const uint8_t TEST_POWER_DOWN_PIN = 9U;
static const uint8_t TEST_GAIN0_PIN = 14U;
static const uint8_t TEST_GAIN1_PIN = 15U;

void setUp(void)
{
    fake_ads1232_platform_reset();
}

void tearDown(void)
{
}

static ads1232_status_t init_default(
    ads1232_t *device
)
{
    return ads1232_init(
        device,
        TEST_DATA_PIN,
        TEST_CLOCK_PIN,
        TEST_POWER_DOWN_PIN,
        TEST_GAIN0_PIN,
        TEST_GAIN1_PIN,
        ADS1232_GAIN_128
    );
}

static void test_init_configures_pins_gain_and_reset_sequence(
    void
)
{
    ads1232_t device;

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        init_default(&device)
    );

    TEST_ASSERT_TRUE(device.initialized);
    TEST_ASSERT_FALSE(device.powered_down);
    TEST_ASSERT_EQUAL_UINT8(TEST_DATA_PIN, device.data_pin);
    TEST_ASSERT_EQUAL_UINT8(TEST_CLOCK_PIN, device.clock_pin);
    TEST_ASSERT_EQUAL_UINT8(
        TEST_POWER_DOWN_PIN,
        device.power_down_pin
    );
    TEST_ASSERT_EQUAL_UINT8(TEST_GAIN0_PIN, device.gain0_pin);
    TEST_ASSERT_EQUAL_UINT8(TEST_GAIN1_PIN, device.gain1_pin);
    TEST_ASSERT_EQUAL_INT(ADS1232_GAIN_128, device.gain);

    TEST_ASSERT_EQUAL_INT(
        FAKE_ADS1232_PIN_INPUT,
        fake_ads1232_platform_get_pin_mode(TEST_DATA_PIN)
    );
    TEST_ASSERT_EQUAL_INT(
        FAKE_ADS1232_PIN_OUTPUT,
        fake_ads1232_platform_get_pin_mode(TEST_CLOCK_PIN)
    );
    TEST_ASSERT_EQUAL_INT(
        FAKE_ADS1232_PIN_OUTPUT,
        fake_ads1232_platform_get_pin_mode(TEST_POWER_DOWN_PIN)
    );
    TEST_ASSERT_EQUAL_INT(
        FAKE_ADS1232_PIN_OUTPUT,
        fake_ads1232_platform_get_pin_mode(TEST_GAIN0_PIN)
    );
    TEST_ASSERT_EQUAL_INT(
        FAKE_ADS1232_PIN_OUTPUT,
        fake_ads1232_platform_get_pin_mode(TEST_GAIN1_PIN)
    );

    TEST_ASSERT_FALSE(
        fake_ads1232_platform_get_pin_output(TEST_CLOCK_PIN)
    );
    TEST_ASSERT_TRUE(
        fake_ads1232_platform_get_pin_output(TEST_POWER_DOWN_PIN)
    );
    TEST_ASSERT_TRUE(
        fake_ads1232_platform_get_pin_output(TEST_GAIN0_PIN)
    );
    TEST_ASSERT_TRUE(
        fake_ads1232_platform_get_pin_output(TEST_GAIN1_PIN)
    );

    TEST_ASSERT_EQUAL_UINT32(
        4U,
        fake_ads1232_platform_get_pin_write_count(
            TEST_POWER_DOWN_PIN
        )
    );
    TEST_ASSERT_EQUAL_UINT32(
        3U,
        fake_ads1232_platform_get_delay_call_count()
    );
    TEST_ASSERT_EQUAL_UINT16(
        10U,
        fake_ads1232_platform_get_delay_us(0U)
    );
    TEST_ASSERT_EQUAL_UINT16(
        30U,
        fake_ads1232_platform_get_delay_us(1U)
    );
    TEST_ASSERT_EQUAL_UINT16(
        30U,
        fake_ads1232_platform_get_delay_us(2U)
    );
}

static void test_init_applies_all_gain_bit_patterns(void)
{
    const ads1232_gain_t gains[] = {
        ADS1232_GAIN_1,
        ADS1232_GAIN_2,
        ADS1232_GAIN_64,
        ADS1232_GAIN_128
    };

    const bool expected_gain0[] = {
        false,
        true,
        false,
        true
    };

    const bool expected_gain1[] = {
        false,
        false,
        true,
        true
    };

    for (uint8_t index = 0U;
         index < 4U;
         ++index)
    {
        ads1232_t device;
        fake_ads1232_platform_reset();

        TEST_ASSERT_EQUAL_INT(
            ADS1232_STATUS_OK,
            ads1232_init(
                &device,
                TEST_DATA_PIN,
                TEST_CLOCK_PIN,
                TEST_POWER_DOWN_PIN,
                TEST_GAIN0_PIN,
                TEST_GAIN1_PIN,
                gains[index]
            )
        );

        TEST_ASSERT_EQUAL(
            expected_gain0[index],
            fake_ads1232_platform_get_pin_output(
                TEST_GAIN0_PIN
            )
        );
        TEST_ASSERT_EQUAL(
            expected_gain1[index],
            fake_ads1232_platform_get_pin_output(
                TEST_GAIN1_PIN
            )
        );
    }
}

static void test_init_rejects_invalid_arguments(void)
{
    ads1232_t device = {0};

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_INVALID_ARGUMENT,
        ads1232_init(
            NULL,
            TEST_DATA_PIN,
            TEST_CLOCK_PIN,
            TEST_POWER_DOWN_PIN,
            TEST_GAIN0_PIN,
            TEST_GAIN1_PIN,
            ADS1232_GAIN_128
        )
    );

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_INVALID_ARGUMENT,
        ads1232_init(
            &device,
            TEST_DATA_PIN,
            TEST_CLOCK_PIN,
            TEST_POWER_DOWN_PIN,
            TEST_GAIN0_PIN,
            TEST_GAIN1_PIN,
            (ads1232_gain_t)99
        )
    );

    TEST_ASSERT_FALSE(device.initialized);
}

static void test_is_ready_reflects_dout_and_device_state(void)
{
    ads1232_t device = {0};

    TEST_ASSERT_FALSE(ads1232_is_ready(NULL));
    TEST_ASSERT_FALSE(ads1232_is_ready(&device));
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));

    fake_ads1232_platform_set_ready(false);
    TEST_ASSERT_FALSE(ads1232_is_ready(&device));

    fake_ads1232_platform_set_ready(true);
    TEST_ASSERT_TRUE(ads1232_is_ready(&device));

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_power_down(&device)
    );
    TEST_ASSERT_FALSE(ads1232_is_ready(&device));
}

static void test_wait_ready_returns_timeout(void)
{
    ads1232_t device;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));

    fake_ads1232_platform_set_ready(false);
    fake_ads1232_platform_set_time_ms(0U);
    fake_ads1232_platform_set_millis_step(1U);

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_TIMEOUT,
        ads1232_wait_ready(&device, 5U)
    );
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_ads1232_platform_get_critical_enter_count()
    );
}

static void test_wait_ready_handles_millisecond_wraparound(void)
{
    ads1232_t device;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));

    fake_ads1232_platform_set_ready(false);
    fake_ads1232_platform_set_time_ms(UINT32_MAX - 2U);
    fake_ads1232_platform_set_millis_step(1U);

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_TIMEOUT,
        ads1232_wait_ready(&device, 5U)
    );
}

static void test_wait_ready_rejects_invalid_device_states(void)
{
    ads1232_t device = {0};

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_INVALID_ARGUMENT,
        ads1232_wait_ready(NULL, 100U)
    );
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_NOT_INITIALIZED,
        ads1232_wait_ready(&device, 100U)
    );

    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_power_down(&device)
    );
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_POWERED_DOWN,
        ads1232_wait_ready(&device, 100U)
    );
}

static void test_read_raw_reconstructs_positive_value(void)
{
    ads1232_t device;
    int32_t raw_value = 0;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));

    fake_ads1232_platform_set_ready(true);
    fake_ads1232_platform_load_raw_24(0x00123456UL);

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_read_raw(&device, &raw_value)
    );
    TEST_ASSERT_EQUAL_INT32((int32_t)0x00123456L, raw_value);
}

static void test_read_raw_sign_extends_negative_one(void)
{
    ads1232_t device;
    int32_t raw_value = 0;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));

    fake_ads1232_platform_set_ready(true);
    fake_ads1232_platform_load_raw_24(0x00FFFFFFUL);

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_read_raw(&device, &raw_value)
    );
    TEST_ASSERT_EQUAL_INT32(-1, raw_value);
}

static void test_read_raw_sign_extends_minimum_value(void)
{
    ads1232_t device;
    int32_t raw_value = 0;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));

    fake_ads1232_platform_set_ready(true);
    fake_ads1232_platform_load_raw_24(0x00800000UL);

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_read_raw(&device, &raw_value)
    );
    TEST_ASSERT_EQUAL_INT32(-8388608L, raw_value);
}

static void test_read_uses_25_pulses_and_restores_critical_state(void)
{
    ads1232_t device;
    int32_t raw_value = 0;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));

    fake_ads1232_platform_set_ready(true);
    fake_ads1232_platform_load_raw_24(0x00010203UL);
    fake_ads1232_platform_set_critical_state((uintptr_t)0xA5U);

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_read_raw(&device, &raw_value)
    );
    TEST_ASSERT_EQUAL_UINT8(
        25U,
        fake_ads1232_platform_get_last_pulse_count()
    );
    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_ads1232_platform_get_critical_enter_count()
    );
    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_ads1232_platform_get_critical_exit_count()
    );
    TEST_ASSERT_EQUAL_UINT32(
        0xA5U,
        (uint32_t)
        fake_ads1232_platform_get_restored_critical_state()
    );
    TEST_ASSERT_FALSE(
        fake_ads1232_platform_get_pin_output(TEST_CLOCK_PIN)
    );
}

static void test_read_timeout_preserves_output_and_skips_transfer(void)
{
    ads1232_t device;
    int32_t raw_value = 123456;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));

    fake_ads1232_platform_set_ready(false);
    fake_ads1232_platform_set_time_ms(0U);
    fake_ads1232_platform_set_millis_step(1U);

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_TIMEOUT,
        ads1232_read_raw(&device, &raw_value)
    );
    TEST_ASSERT_EQUAL_INT32(123456, raw_value);
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_ads1232_platform_get_critical_enter_count()
    );
}

static void test_read_rejects_invalid_arguments_and_gain(void)
{
    ads1232_t device = {0};
    int32_t raw_value = 0;

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_INVALID_ARGUMENT,
        ads1232_read_raw(NULL, &raw_value)
    );
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_INVALID_ARGUMENT,
        ads1232_read_raw(&device, NULL)
    );
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_NOT_INITIALIZED,
        ads1232_read_raw(&device, &raw_value)
    );

    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));
    device.gain = (ads1232_gain_t)99;

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_INVALID_ARGUMENT,
        ads1232_read_raw(&device, &raw_value)
    );
}

static void test_offset_calibration_uses_26_pulses(void)
{
    ads1232_t device;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));

    fake_ads1232_platform_set_ready(true);
    fake_ads1232_platform_load_raw_24(0x00010203UL);

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_start_offset_calibration(&device)
    );
    TEST_ASSERT_EQUAL_UINT8(
        26U,
        fake_ads1232_platform_get_last_pulse_count()
    );
    TEST_ASSERT_EQUAL_UINT32(
        1U,
        fake_ads1232_platform_get_critical_enter_count()
    );
    TEST_ASSERT_FALSE(ads1232_is_ready(&device));
}

static void test_offset_calibration_timeout_skips_transfer(void)
{
    ads1232_t device;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));

    fake_ads1232_platform_set_ready(false);
    fake_ads1232_platform_set_time_ms(0U);
    fake_ads1232_platform_set_millis_step(1U);

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_TIMEOUT,
        ads1232_start_offset_calibration(&device)
    );
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_ads1232_platform_get_critical_enter_count()
    );
}

static void test_offset_calibration_rejects_invalid_states(void)
{
    ads1232_t device = {0};

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_INVALID_ARGUMENT,
        ads1232_start_offset_calibration(NULL)
    );
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_NOT_INITIALIZED,
        ads1232_start_offset_calibration(&device)
    );

    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_power_down(&device)
    );
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_POWERED_DOWN,
        ads1232_start_offset_calibration(&device)
    );
}

static void test_power_down_holds_pdwn_low_and_is_idempotent(void)
{
    ads1232_t device;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));

    const uint32_t delays_before =
        fake_ads1232_platform_get_delay_call_count();

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_power_down(&device)
    );
    TEST_ASSERT_TRUE(device.powered_down);
    TEST_ASSERT_FALSE(
        fake_ads1232_platform_get_pin_output(TEST_CLOCK_PIN)
    );
    TEST_ASSERT_FALSE(
        fake_ads1232_platform_get_pin_output(TEST_POWER_DOWN_PIN)
    );
    TEST_ASSERT_EQUAL_UINT32(
        delays_before + 1U,
        fake_ads1232_platform_get_delay_call_count()
    );
    TEST_ASSERT_EQUAL_UINT16(
        30U,
        fake_ads1232_platform_get_delay_us(
            (uint8_t)delays_before
        )
    );

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_power_down(&device)
    );
    TEST_ASSERT_EQUAL_UINT32(
        delays_before + 1U,
        fake_ads1232_platform_get_delay_call_count()
    );
}

static void test_power_up_reapplies_gain_and_releases_pdwn(void)
{
    ads1232_t device;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_power_down(&device)
    );

    const uint32_t gain0_writes_before =
        fake_ads1232_platform_get_pin_write_count(
            TEST_GAIN0_PIN
        );
    const uint32_t gain1_writes_before =
        fake_ads1232_platform_get_pin_write_count(
            TEST_GAIN1_PIN
        );

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_power_up(&device)
    );
    TEST_ASSERT_FALSE(device.powered_down);
    TEST_ASSERT_TRUE(
        fake_ads1232_platform_get_pin_output(TEST_POWER_DOWN_PIN)
    );
    TEST_ASSERT_TRUE(
        fake_ads1232_platform_get_pin_output(TEST_GAIN0_PIN)
    );
    TEST_ASSERT_TRUE(
        fake_ads1232_platform_get_pin_output(TEST_GAIN1_PIN)
    );
    TEST_ASSERT_EQUAL_UINT32(
        gain0_writes_before + 1U,
        fake_ads1232_platform_get_pin_write_count(
            TEST_GAIN0_PIN
        )
    );
    TEST_ASSERT_EQUAL_UINT32(
        gain1_writes_before + 1U,
        fake_ads1232_platform_get_pin_write_count(
            TEST_GAIN1_PIN
        )
    );
}

static void test_power_functions_reject_invalid_devices(void)
{
    ads1232_t device = {0};

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_INVALID_ARGUMENT,
        ads1232_power_down(NULL)
    );
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_INVALID_ARGUMENT,
        ads1232_power_up(NULL)
    );
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_NOT_INITIALIZED,
        ads1232_power_down(&device)
    );
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_NOT_INITIALIZED,
        ads1232_power_up(&device)
    );
}

static void test_read_while_powered_down_is_rejected_immediately(void)
{
    ads1232_t device;
    int32_t raw_value = 123456;
    TEST_ASSERT_EQUAL_INT(ADS1232_STATUS_OK, init_default(&device));
    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_OK,
        ads1232_power_down(&device)
    );

    TEST_ASSERT_EQUAL_INT(
        ADS1232_STATUS_POWERED_DOWN,
        ads1232_read_raw(&device, &raw_value)
    );
    TEST_ASSERT_EQUAL_INT32(123456, raw_value);
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        fake_ads1232_platform_get_critical_enter_count()
    );
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_init_configures_pins_gain_and_reset_sequence);
    RUN_TEST(test_init_applies_all_gain_bit_patterns);
    RUN_TEST(test_init_rejects_invalid_arguments);
    RUN_TEST(test_is_ready_reflects_dout_and_device_state);
    RUN_TEST(test_wait_ready_returns_timeout);
    RUN_TEST(test_wait_ready_handles_millisecond_wraparound);
    RUN_TEST(test_wait_ready_rejects_invalid_device_states);
    RUN_TEST(test_read_raw_reconstructs_positive_value);
    RUN_TEST(test_read_raw_sign_extends_negative_one);
    RUN_TEST(test_read_raw_sign_extends_minimum_value);
    RUN_TEST(test_read_uses_25_pulses_and_restores_critical_state);
    RUN_TEST(test_read_timeout_preserves_output_and_skips_transfer);
    RUN_TEST(test_read_rejects_invalid_arguments_and_gain);
    RUN_TEST(test_offset_calibration_uses_26_pulses);
    RUN_TEST(test_offset_calibration_timeout_skips_transfer);
    RUN_TEST(test_offset_calibration_rejects_invalid_states);
    RUN_TEST(test_power_down_holds_pdwn_low_and_is_idempotent);
    RUN_TEST(test_power_up_reapplies_gain_and_releases_pdwn);
    RUN_TEST(test_power_functions_reject_invalid_devices);
    RUN_TEST(test_read_while_powered_down_is_rejected_immediately);

    return UNITY_END();
}
