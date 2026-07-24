#include "hx711_driver.h"

#include <stddef.h>

#include "hx711_platform.h"

#define HX711_READ_TIMEOUT_MS 1000U
#define HX711_CLOCK_DELAY_US  1U
#define HX711_DATA_BITS       24U

static bool hx711_gain_is_valid(hx711_gain_t gain)
{
    return (gain == HX711_GAIN_A_128) ||
           (gain == HX711_GAIN_B_32)  ||
           (gain == HX711_GAIN_A_64);
}

hx711_status_t hx711_init(
    hx711_t *device,
    uint8_t data_pin,
    uint8_t clock_pin
)
{
    if (device == NULL)
    {
        return HX711_STATUS_INVALID_ARGUMENT;
    }

    /*
     * Mark the device as uninitialized until GPIO configuration
     * has been completed successfully.
     */
    device->initialized = false;

    device->data_pin = data_pin;
    device->clock_pin = clock_pin;
    device->gain = HX711_GAIN_A_128;

    hx711_platform_configure_input(device->data_pin);
    hx711_platform_configure_output(device->clock_pin);

    /*
     * PD_SCK must normally remain LOW.
     * Leaving it HIGH for too long would power down the HX711.
     */
    hx711_platform_write_pin(device->clock_pin, false);

    device->initialized = true;

    return HX711_STATUS_OK;
}

bool hx711_is_ready(const hx711_t *device)
{
    if (device == NULL)
    {
        return false;
    }

    if (!device->initialized)
    {
        return false;
    }

    /*
     * The HX711 indicates that a conversion is ready by pulling
     * DOUT LOW.
     */
    return !hx711_platform_read_pin(device->data_pin);
}

hx711_status_t hx711_wait_ready(
    const hx711_t *device,
    uint32_t timeout_ms
)
{
    if (device == NULL)
    {
        return HX711_STATUS_INVALID_ARGUMENT;
    }

    if (!device->initialized)
    {
        return HX711_STATUS_NOT_INITIALIZED;
    }

    const uint32_t start_time_ms = hx711_platform_millis();

    while (!hx711_is_ready(device))
    {
        const uint32_t elapsed_time_ms =
            hx711_platform_millis() - start_time_ms;

        if (elapsed_time_ms >= timeout_ms)
        {
            return HX711_STATUS_TIMEOUT;
        }
    }

    return HX711_STATUS_OK;
}

hx711_status_t hx711_read_raw(
    hx711_t *device,
    int32_t *raw_value
)
{
    if ((device == NULL) || (raw_value == NULL))
    {
        return HX711_STATUS_INVALID_ARGUMENT;
    }

    if (!device->initialized)
    {
        return HX711_STATUS_NOT_INITIALIZED;
    }

    if (!hx711_gain_is_valid(device->gain))
    {
        return HX711_STATUS_INVALID_ARGUMENT;
    }

    const hx711_status_t ready_status =
        hx711_wait_ready(device, HX711_READ_TIMEOUT_MS);

    if (ready_status != HX711_STATUS_OK)
    {
        return ready_status;
    }

    uint32_t raw_data = 0U;

    const hx711_platform_critical_state_t previous_state =
        hx711_platform_enter_critical();

    for (uint8_t bit_index = 0U;
         bit_index < HX711_DATA_BITS;
         ++bit_index)
    {
        hx711_platform_write_pin(device->clock_pin, true);
        hx711_platform_delay_us(HX711_CLOCK_DELAY_US);

        raw_data <<= 1U;

        if (hx711_platform_read_pin(device->data_pin))
        {
            raw_data |= 1U;
        }

        hx711_platform_write_pin(device->clock_pin, false);
        hx711_platform_delay_us(HX711_CLOCK_DELAY_US);
    }

    for (uint8_t pulse_index = 0U;
         pulse_index < (uint8_t)device->gain;
         ++pulse_index)
    {
        hx711_platform_write_pin(device->clock_pin, true);
        hx711_platform_delay_us(HX711_CLOCK_DELAY_US);

        hx711_platform_write_pin(device->clock_pin, false);
        hx711_platform_delay_us(HX711_CLOCK_DELAY_US);
    }

    hx711_platform_exit_critical(previous_state);

    if ((raw_data & 0x00800000UL) != 0U)
    {
        raw_data |= 0xFF000000UL;
    }

    *raw_value = (int32_t)raw_data;

    return HX711_STATUS_OK;
}

hx711_status_t hx711_set_gain(
    hx711_t *device,
    hx711_gain_t gain
)
{
    if (device == NULL)
    {
        return HX711_STATUS_INVALID_ARGUMENT;
    }

    if (!device->initialized)
    {
        return HX711_STATUS_NOT_INITIALIZED;
    }

    if (!hx711_gain_is_valid(gain))
    {
        return HX711_STATUS_INVALID_ARGUMENT;
    }

    if (device->gain == gain)
    {
        return HX711_STATUS_OK;
    }

    const hx711_gain_t previous_gain = device->gain;

    device->gain = gain;

    /*
     * The additional clock pulses at the end of a reading select
     * the channel and gain for the following conversion.
     *
     * Discard one reading so that the next reading uses the newly
     * selected configuration.
     */
    int32_t discarded_reading = 0;

    const hx711_status_t status =
        hx711_read_raw(device, &discarded_reading);

    if (status != HX711_STATUS_OK)
    {
        device->gain = previous_gain;
        return status;
    }

    return HX711_STATUS_OK;
}

hx711_status_t hx711_power_down(hx711_t *device)
{
    if (device == NULL)
    {
        return HX711_STATUS_INVALID_ARGUMENT;
    }

    if (!device->initialized)
    {
        return HX711_STATUS_NOT_INITIALIZED;
    }

    /*
     * PD_SCK must remain HIGH for at least 60 microseconds
     * to put the HX711 into power-down mode.
     */
    hx711_platform_write_pin(device->clock_pin, false);
    hx711_platform_write_pin(device->clock_pin, true);

    hx711_platform_delay_us(70U);

    return HX711_STATUS_OK;
}

hx711_status_t hx711_power_up(hx711_t *device)
{
    if (device == NULL)
    {
        return HX711_STATUS_INVALID_ARGUMENT;
    }

    if (!device->initialized)
    {
        return HX711_STATUS_NOT_INITIALIZED;
    }

    /*
     * Pulling PD_SCK LOW wakes the HX711.
     */
    hx711_platform_write_pin(device->clock_pin, false);

    /*
     * After power-up, the HX711 returns to channel A with gain 128.
     *
     * If another configuration was selected, discard one conversion
     * so that the additional clock pulses configure the following
     * conversion with the desired gain.
     */
    if (device->gain != HX711_GAIN_A_128)
    {
        int32_t discarded_reading = 0;

        return hx711_read_raw(device, &discarded_reading);
    }

    return HX711_STATUS_OK;
}