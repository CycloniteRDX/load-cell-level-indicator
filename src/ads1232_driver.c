#include "ads1232_driver.h"

#include <stddef.h>

#include "ads1232_platform.h"

#define ADS1232_READ_TIMEOUT_MS       1000U
#define ADS1232_CLOCK_DELAY_US        1U
#define ADS1232_DATA_BITS             24U
#define ADS1232_READ_PULSES           25U
#define ADS1232_CALIBRATION_PULSES    26U
#define ADS1232_INITIAL_LOW_US        10U
#define ADS1232_RESET_PHASE_US        30U
#define ADS1232_POWER_DOWN_RESET_US   30U

static bool ads1232_gain_is_valid(
    ads1232_gain_t gain
)
{
    return (gain == ADS1232_GAIN_1)   ||
           (gain == ADS1232_GAIN_2)   ||
           (gain == ADS1232_GAIN_64)  ||
           (gain == ADS1232_GAIN_128);
}

static void ads1232_apply_gain(
    const ads1232_t *device
)
{
    const uint8_t encoded_gain =
        (uint8_t)device->gain;

    ads1232_platform_write_pin(
        device->gain0_pin,
        (encoded_gain & 0x01U) != 0U
    );

    ads1232_platform_write_pin(
        device->gain1_pin,
        (encoded_gain & 0x02U) != 0U
    );
}

static ads1232_status_t ads1232_validate_active(
    const ads1232_t *device
)
{
    if (device == NULL)
    {
        return ADS1232_STATUS_INVALID_ARGUMENT;
    }

    if (!device->initialized)
    {
        return ADS1232_STATUS_NOT_INITIALIZED;
    }

    if (device->powered_down)
    {
        return ADS1232_STATUS_POWERED_DOWN;
    }

    if (!ads1232_gain_is_valid(device->gain))
    {
        return ADS1232_STATUS_INVALID_ARGUMENT;
    }

    return ADS1232_STATUS_OK;
}

static ads1232_status_t ads1232_transfer(
    ads1232_t *device,
    uint8_t total_pulses,
    int32_t *raw_value
)
{
    const ads1232_status_t active_status =
        ads1232_validate_active(device);

    if (active_status != ADS1232_STATUS_OK)
    {
        return active_status;
    }

    const ads1232_status_t ready_status =
        ads1232_wait_ready(
            device,
            ADS1232_READ_TIMEOUT_MS
        );

    if (ready_status != ADS1232_STATUS_OK)
    {
        return ready_status;
    }

    uint32_t raw_data = 0U;

    const ads1232_platform_critical_state_t
        previous_state =
            ads1232_platform_enter_critical();

    for (uint8_t pulse_index = 0U;
         pulse_index < total_pulses;
         ++pulse_index)
    {
        ads1232_platform_write_pin(
            device->clock_pin,
            true
        );

        ads1232_platform_delay_us(
            ADS1232_CLOCK_DELAY_US
        );

        if (pulse_index < ADS1232_DATA_BITS)
        {
            raw_data <<= 1U;

            if (ads1232_platform_read_pin(
                    device->data_pin
                ))
            {
                raw_data |= 1U;
            }
        }

        ads1232_platform_write_pin(
            device->clock_pin,
            false
        );

        ads1232_platform_delay_us(
            ADS1232_CLOCK_DELAY_US
        );
    }

    ads1232_platform_exit_critical(
        previous_state
    );

    if (raw_value != NULL)
    {
        if ((raw_data & 0x00800000UL) != 0U)
        {
            raw_data |= 0xFF000000UL;
        }

        *raw_value = (int32_t)raw_data;
    }

    return ADS1232_STATUS_OK;
}

ads1232_status_t ads1232_init(
    ads1232_t *device,
    uint8_t data_pin,
    uint8_t clock_pin,
    uint8_t power_down_pin,
    uint8_t gain0_pin,
    uint8_t gain1_pin,
    ads1232_gain_t gain
)
{
    if ((device == NULL) ||
        !ads1232_gain_is_valid(gain))
    {
        return ADS1232_STATUS_INVALID_ARGUMENT;
    }

    device->initialized = false;
    device->powered_down = true;

    device->data_pin = data_pin;
    device->clock_pin = clock_pin;
    device->power_down_pin = power_down_pin;
    device->gain0_pin = gain0_pin;
    device->gain1_pin = gain1_pin;
    device->gain = gain;

    ads1232_platform_configure_input(
        device->data_pin
    );

    ads1232_platform_configure_output(
        device->clock_pin
    );

    ads1232_platform_configure_output(
        device->power_down_pin
    );

    ads1232_platform_configure_output(
        device->gain0_pin
    );

    ads1232_platform_configure_output(
        device->gain1_pin
    );

    ads1232_platform_write_pin(
        device->clock_pin,
        false
    );

    ads1232_platform_write_pin(
        device->power_down_pin,
        false
    );

    ads1232_apply_gain(device);

    /*
     * ADS1232 power-up reset sequence after the supplies are stable:
     * LOW >= 10 us, HIGH >= 26 us, LOW >= 26 us, then HIGH.
     */
    ads1232_platform_delay_us(
        ADS1232_INITIAL_LOW_US
    );

    ads1232_platform_write_pin(
        device->power_down_pin,
        true
    );

    ads1232_platform_delay_us(
        ADS1232_RESET_PHASE_US
    );

    ads1232_platform_write_pin(
        device->power_down_pin,
        false
    );

    ads1232_platform_delay_us(
        ADS1232_RESET_PHASE_US
    );

    ads1232_platform_write_pin(
        device->power_down_pin,
        true
    );

    device->powered_down = false;
    device->initialized = true;

    return ADS1232_STATUS_OK;
}

bool ads1232_is_ready(const ads1232_t *device)
{
    if ((device == NULL) ||
        !device->initialized ||
        device->powered_down)
    {
        return false;
    }

    return !ads1232_platform_read_pin(
        device->data_pin
    );
}

ads1232_status_t ads1232_wait_ready(
    const ads1232_t *device,
    uint32_t timeout_ms
)
{
    const ads1232_status_t active_status =
        ads1232_validate_active(device);

    if (active_status != ADS1232_STATUS_OK)
    {
        return active_status;
    }

    const uint32_t start_time_ms =
        ads1232_platform_millis();

    while (!ads1232_is_ready(device))
    {
        const uint32_t elapsed_time_ms =
            ads1232_platform_millis() -
            start_time_ms;

        if (elapsed_time_ms >= timeout_ms)
        {
            return ADS1232_STATUS_TIMEOUT;
        }
    }

    return ADS1232_STATUS_OK;
}

ads1232_status_t ads1232_read_raw(
    ads1232_t *device,
    int32_t *raw_value
)
{
    if (raw_value == NULL)
    {
        return ADS1232_STATUS_INVALID_ARGUMENT;
    }

    return ads1232_transfer(
        device,
        ADS1232_READ_PULSES,
        raw_value
    );
}

ads1232_status_t ads1232_start_offset_calibration(
    ads1232_t *device
)
{
    return ads1232_transfer(
        device,
        ADS1232_CALIBRATION_PULSES,
        NULL
    );
}

ads1232_status_t ads1232_power_down(
    ads1232_t *device
)
{
    if (device == NULL)
    {
        return ADS1232_STATUS_INVALID_ARGUMENT;
    }

    if (!device->initialized)
    {
        return ADS1232_STATUS_NOT_INITIALIZED;
    }

    if (device->powered_down)
    {
        return ADS1232_STATUS_OK;
    }

    ads1232_platform_write_pin(
        device->clock_pin,
        false
    );

    ads1232_platform_write_pin(
        device->power_down_pin,
        false
    );

    ads1232_platform_delay_us(
        ADS1232_POWER_DOWN_RESET_US
    );

    device->powered_down = true;

    return ADS1232_STATUS_OK;
}

ads1232_status_t ads1232_power_up(
    ads1232_t *device
)
{
    if (device == NULL)
    {
        return ADS1232_STATUS_INVALID_ARGUMENT;
    }

    if (!device->initialized)
    {
        return ADS1232_STATUS_NOT_INITIALIZED;
    }

    if (!device->powered_down)
    {
        return ADS1232_STATUS_OK;
    }

    if (!ads1232_gain_is_valid(device->gain))
    {
        return ADS1232_STATUS_INVALID_ARGUMENT;
    }

    ads1232_platform_write_pin(
        device->clock_pin,
        false
    );

    ads1232_apply_gain(device);

    ads1232_platform_write_pin(
        device->power_down_pin,
        true
    );

    device->powered_down = false;

    return ADS1232_STATUS_OK;
}
