#ifndef SCALE_ADC_H
#define SCALE_ADC_H

#include <stdbool.h>
#include <stdint.h>

#include "scale_adc_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SCALE_ADC_STATUS_OK = 0,
    SCALE_ADC_STATUS_ERROR
} scale_adc_status_t;

/*
 * Small converter boundary required by scale.cpp.
 * Exactly one implementation is selected at compile time.
 */
scale_adc_status_t scale_adc_init(void);

bool scale_adc_is_ready(void);

scale_adc_status_t scale_adc_read_raw(
    int32_t *raw_value
);

scale_adc_status_t scale_adc_power_down(void);

scale_adc_status_t scale_adc_power_up(void);

#ifdef __cplusplus
}
#endif

#endif
