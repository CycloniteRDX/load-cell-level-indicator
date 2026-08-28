#ifndef TEST_SCALE_FAKE_SCALE_ADC_H
#define TEST_SCALE_FAKE_SCALE_ADC_H

#include <stdbool.h>
#include <stdint.h>

#include "scale_adc.h"

#ifdef __cplusplus
extern "C" {
#endif

void fake_scale_adc_reset(void);

void fake_scale_adc_set_init_status(
    scale_adc_status_t status
);

void fake_scale_adc_set_ready(
    bool ready
);

void fake_scale_adc_set_power_down_status(
    scale_adc_status_t status
);

void fake_scale_adc_set_power_up_status(
    scale_adc_status_t status
);

bool fake_scale_adc_push_reading(
    scale_adc_status_t status,
    int32_t raw_value
);

uint32_t fake_scale_adc_get_init_call_count(void);
uint32_t fake_scale_adc_get_is_ready_call_count(void);
uint32_t fake_scale_adc_get_read_raw_call_count(void);
uint32_t fake_scale_adc_get_power_down_call_count(void);
uint32_t fake_scale_adc_get_power_up_call_count(void);

bool fake_scale_adc_power_down_preceded_power_up(
    void
);

uint16_t fake_scale_adc_get_consumed_reading_count(void);
bool fake_scale_adc_sequence_was_exhausted(void);

#ifdef __cplusplus
}
#endif

#endif
