#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "hx711_driver.h"
#include "scale.h"

#define SCALE_INITIAL_READY_TIMEOUT_MS 2000U

static hx711_t hx711_device;

static int32_t tare_offset = 0;

static float current_calibration_factor = 1.0F;

static bool scale_read_average_raw(
    int32_t *average_raw,
    uint8_t samples
)
{
    if ((average_raw == NULL) || (samples == 0U))
    {
        return false;
    }

    int32_t raw_sum = 0;

    for (uint8_t sample_index = 0U;
         sample_index < samples;
         ++sample_index)
    {
        int32_t raw_value = 0;

        const hx711_status_t status =
            hx711_read_raw(&hx711_device, &raw_value);

        if (status != HX711_STATUS_OK)
        {
            return false;
        }

        raw_sum += raw_value;
    }

    *average_raw = raw_sum / (int32_t)samples;

    return true;
}

bool scale_init(void)
{
    const hx711_status_t init_status = hx711_init(
        &hx711_device,
        LOADCELL_DOUT_PIN,
        LOADCELL_SCK_PIN
    );

    if (init_status != HX711_STATUS_OK)
    {
        return false;
    }

    const hx711_status_t ready_status = hx711_wait_ready(
        &hx711_device,
        SCALE_INITIAL_READY_TIMEOUT_MS
    );

    if (ready_status != HX711_STATUS_OK)
    {
        return false;
    }

    tare_offset = 0;
    current_calibration_factor = 1.0F;

    return true;
}

bool scale_set_calibration_factor(
    float calibration_factor
)
{
    /*
     * A negative calibration factor is valid.
     * Its sign depends on the load-cell wiring direction.
     *
     * Zero, NaN and infinity are not valid.
     */
    if (isnan(calibration_factor) ||
        isinf(calibration_factor) ||
        (fabsf(calibration_factor) < 0.000001F))
    {
        return false;
    }

    current_calibration_factor = calibration_factor;

    return true;
}

void scale_tare(void)
{
    int32_t new_tare_offset = 0;

    if (scale_read_average_raw(
            &new_tare_offset,
            TARE_SAMPLES))
    {
        tare_offset = new_tare_offset;
    }
}

bool scale_read_weight(float *weight_grams)
{
    if (weight_grams == NULL)
    {
        return false;
    }

    /*
     * Preserve the previous non-blocking behaviour:
     * do not start a measurement unless a conversion
     * is already available.
     */
    if (!hx711_is_ready(&hx711_device))
    {
        return false;
    }

    int32_t average_raw = 0;

    if (!scale_read_average_raw(
            &average_raw,
            WEIGHT_SAMPLES))
    {
        return false;
    }

    const int32_t net_counts =
        average_raw - tare_offset;

    *weight_grams =
        (float)net_counts /
        current_calibration_factor;

    return true;
}

bool scale_read_net_counts(
    float *net_counts,
    uint8_t samples
)
{
    if ((net_counts == NULL) || (samples == 0U))
    {
        return false;
    }

    int32_t average_raw = 0;

    if (!scale_read_average_raw(
            &average_raw,
            samples))
    {
        return false;
    }

    *net_counts =
        (float)(average_raw - tare_offset);

    return true;
}

long scale_get_offset(void)
{
    return (long)tare_offset;
}

float scale_get_calibration_factor(void)
{
    return current_calibration_factor;
}