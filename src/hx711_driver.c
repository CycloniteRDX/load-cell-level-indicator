#include "hx711_driver.h"

#include <stddef.h>

#include "hx711_platform.h"

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