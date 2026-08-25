#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "hx711_driver.h"
#include "scale.h"

static hx711_t hx711_device;

static int32_t tare_offset = 0;

static float current_calibration_factor = 1.0F;

static scale_sample_collection_status_t
    sample_collection_status =
        SCALE_SAMPLE_COLLECTION_IDLE;

static uint8_t requested_sample_count = 0U;
static uint8_t collected_sample_count = 0U;
static int32_t sample_sum = 0;
static int32_t completed_sample_average = 0;


static void scale_reset_sample_collection(void)
{
    sample_collection_status =
        SCALE_SAMPLE_COLLECTION_IDLE;

    requested_sample_count = 0U;
    collected_sample_count = 0U;
    sample_sum = 0;
    completed_sample_average = 0;
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

    tare_offset = 0;
    current_calibration_factor = 1.0F;
    scale_reset_sample_collection();

    return true;
}


bool scale_is_ready(void)
{
    return hx711_is_ready(&hx711_device);
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

void scale_set_offset(
    int32_t new_tare_offset
)
{
    tare_offset = new_tare_offset;
}


bool scale_start_sample_collection(
    uint8_t sample_count
)
{
    if ((sample_count == 0U) ||
        (sample_collection_status !=
            SCALE_SAMPLE_COLLECTION_IDLE))
    {
        return false;
    }

    requested_sample_count = sample_count;
    collected_sample_count = 0U;
    sample_sum = 0;
    completed_sample_average = 0;

    sample_collection_status =
        SCALE_SAMPLE_COLLECTION_IN_PROGRESS;

    return true;
}


scale_sample_collection_status_t
scale_update_sample_collection(void)
{
    if (sample_collection_status !=
        SCALE_SAMPLE_COLLECTION_IN_PROGRESS)
    {
        return sample_collection_status;
    }

    if (!hx711_is_ready(&hx711_device))
    {
        return sample_collection_status;
    }

    int32_t raw_value = 0;

    const hx711_status_t read_status =
        hx711_read_raw(
            &hx711_device,
            &raw_value
        );

    if (read_status != HX711_STATUS_OK)
    {
        sample_collection_status =
            SCALE_SAMPLE_COLLECTION_ERROR;

        return sample_collection_status;
    }

    sample_sum += raw_value;
    ++collected_sample_count;

    if (collected_sample_count ==
        requested_sample_count)
    {
        completed_sample_average =
            sample_sum /
            (int32_t)requested_sample_count;

        sample_collection_status =
            SCALE_SAMPLE_COLLECTION_COMPLETE;
    }

    return sample_collection_status;
}


bool scale_take_sample_average(
    int32_t *average_raw
)
{
    if ((average_raw == NULL) ||
        (sample_collection_status !=
            SCALE_SAMPLE_COLLECTION_COMPLETE))
    {
        return false;
    }

    *average_raw = completed_sample_average;
    scale_reset_sample_collection();

    return true;
}


void scale_cancel_sample_collection(void)
{
    scale_reset_sample_collection();
}


bool scale_recover(void)
{
    scale_cancel_sample_collection();

    const hx711_status_t power_down_status =
        hx711_power_down(&hx711_device);

    if (power_down_status != HX711_STATUS_OK)
    {
        return false;
    }

    /*
     * Scale currently keeps the HX711 at its initialized
     * channel-A, gain-128 setting. At that setting the
     * driver power-up path does not wait for a conversion.
     */
    const hx711_status_t power_up_status =
        hx711_power_up(&hx711_device);

    return power_up_status == HX711_STATUS_OK;
}


scale_read_status_t scale_try_read_measurement(
    scale_measurement_t *measurement
)
{
    if (measurement == NULL)
    {
        return SCALE_READ_ERROR;
    }

    /*
     * Preserve the previous non-blocking behaviour:
     * do not start a measurement unless a conversion
     * is already available.
     */
    if (!hx711_is_ready(&hx711_device))
    {
        return SCALE_READ_NO_DATA;
    }

    int32_t raw_value = 0;

    const hx711_status_t read_status =
        hx711_read_raw(
            &hx711_device,
            &raw_value
        );

    if (read_status != HX711_STATUS_OK)
    {
        return SCALE_READ_ERROR;
    }

    const int32_t net_counts =
        raw_value - tare_offset;

    scale_measurement_t candidate_measurement;

    candidate_measurement.raw_counts = raw_value;
    candidate_measurement.net_counts = net_counts;
    candidate_measurement.weight_grams =
        (float)net_counts /
        current_calibration_factor;

    *measurement = candidate_measurement;

    return SCALE_READ_VALUE;
}


int32_t scale_get_offset(void)
{
    return tare_offset;
}

float scale_get_calibration_factor(void)
{
    return current_calibration_factor;
}
