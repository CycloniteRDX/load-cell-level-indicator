#include "scale_adc.h"

#include "config.h"

#if SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_HX711

#include "hx711_driver.h"

static hx711_t converter;

scale_adc_status_t scale_adc_init(void)
{
    const hx711_status_t status =
        hx711_init(
            &converter,
            HX711_DOUT_PIN,
            HX711_SCK_PIN
        );

    return (status == HX711_STATUS_OK) ?
        SCALE_ADC_STATUS_OK :
        SCALE_ADC_STATUS_ERROR;
}

bool scale_adc_is_ready(void)
{
    return hx711_is_ready(&converter);
}

scale_adc_status_t scale_adc_read_raw(
    int32_t *raw_value
)
{
    const hx711_status_t status =
        hx711_read_raw(
            &converter,
            raw_value
        );

    return (status == HX711_STATUS_OK) ?
        SCALE_ADC_STATUS_OK :
        SCALE_ADC_STATUS_ERROR;
}

scale_adc_status_t scale_adc_power_down(void)
{
    const hx711_status_t status =
        hx711_power_down(&converter);

    return (status == HX711_STATUS_OK) ?
        SCALE_ADC_STATUS_OK :
        SCALE_ADC_STATUS_ERROR;
}

scale_adc_status_t scale_adc_power_up(void)
{
    const hx711_status_t status =
        hx711_power_up(&converter);

    return (status == HX711_STATUS_OK) ?
        SCALE_ADC_STATUS_OK :
        SCALE_ADC_STATUS_ERROR;
}

#elif SCALE_ADC_BACKEND == SCALE_ADC_BACKEND_ADS1232

#include "ads1232_driver.h"

static ads1232_t converter;

scale_adc_status_t scale_adc_init(void)
{
    const ads1232_status_t status =
        ads1232_init(
            &converter,
            ADS1232_DOUT_PIN,
            ADS1232_SCLK_PIN,
            ADS1232_PDWN_PIN,
            ADS1232_GAIN0_PIN,
            ADS1232_GAIN1_PIN,
            ADS1232_GAIN_128
        );

    return (status == ADS1232_STATUS_OK) ?
        SCALE_ADC_STATUS_OK :
        SCALE_ADC_STATUS_ERROR;
}

bool scale_adc_is_ready(void)
{
    return ads1232_is_ready(&converter);
}

scale_adc_status_t scale_adc_read_raw(
    int32_t *raw_value
)
{
    const ads1232_status_t status =
        ads1232_read_raw(
            &converter,
            raw_value
        );

    return (status == ADS1232_STATUS_OK) ?
        SCALE_ADC_STATUS_OK :
        SCALE_ADC_STATUS_ERROR;
}

scale_adc_status_t scale_adc_power_down(void)
{
    const ads1232_status_t status =
        ads1232_power_down(&converter);

    return (status == ADS1232_STATUS_OK) ?
        SCALE_ADC_STATUS_OK :
        SCALE_ADC_STATUS_ERROR;
}

scale_adc_status_t scale_adc_power_up(void)
{
    const ads1232_status_t status =
        ads1232_power_up(&converter);

    return (status == ADS1232_STATUS_OK) ?
        SCALE_ADC_STATUS_OK :
        SCALE_ADC_STATUS_ERROR;
}

#endif
