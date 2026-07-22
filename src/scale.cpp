#include <Arduino.h>
#include <HX711.h>

#include "config.h"
#include "scale.h"


/*
 * Private HX711 object.
 *
 * static makes it accessible only inside this source file.
 */
static HX711 hx711;


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

    hx711.set_scale(CALIBRATION_FACTOR);

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
    return CALIBRATION_FACTOR;
}