#include <Arduino.h>
#include <HX711.h>
#include <math.h>

#include "config.h"
#include "scale.h"

static HX711 hx711;

static float current_calibration_factor = 1.0F;

/*
 * Private HX711 object.
 *
 * static makes it accessible only inside this source file.
 */


bool scale_init(void)
{
    hx711.begin(
        LOADCELL_DOUT_PIN,
        LOADCELL_SCK_PIN
    );

    if (!hx711.wait_ready_timeout(2000))
    {
        return false;
    }

    /*
     * Start with a neutral factor.
     *
     * The application will provide the actual
     * factor after initialization.
     */
    hx711.set_scale(1.0F);
    current_calibration_factor = 1.0F;

    return true;
}


bool scale_set_calibration_factor(
    float calibration_factor
)
{
    /*
     * A negative calibration factor is valid.
     * Its sign depends on the polarity of the
     * load-cell signal.
     *
     * Zero, NaN and infinity are not valid.
     */
    if (isnan(calibration_factor) ||
        isinf(calibration_factor) ||
        (fabsf(calibration_factor) < 0.000001F))
    {
        return false;
    }

    hx711.set_scale(calibration_factor);

    current_calibration_factor =
        calibration_factor;

    return true;
}


void scale_tare(void)
{
    hx711.tare(TARE_SAMPLES);
}


bool scale_read_weight(float *weight_grams)
{
    if (weight_grams == nullptr)
    {
        return false;
    }

    /*
     * Do not block waiting for a conversion.
     */
    if (!hx711.is_ready())
    {
        return false;
    }

    *weight_grams =
        hx711.get_units(WEIGHT_SAMPLES);

    return true;
}


long scale_get_offset(void)
{
    return hx711.get_offset();
}


float scale_get_calibration_factor(void)
{
    return current_calibration_factor;
}