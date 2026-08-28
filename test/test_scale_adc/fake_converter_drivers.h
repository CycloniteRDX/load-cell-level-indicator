#ifndef TEST_SCALE_ADC_FAKE_CONVERTER_DRIVERS_H
#define TEST_SCALE_ADC_FAKE_CONVERTER_DRIVERS_H

#include <stdbool.h>
#include <stdint.h>

#include "ads1232_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

void fake_converter_drivers_reset(void);

void fake_converter_set_init_success(
    bool success
);

void fake_converter_set_ready(
    bool ready
);

void fake_converter_set_read_result(
    bool success,
    int32_t raw_value
);

void fake_converter_set_power_down_success(
    bool success
);

void fake_converter_set_power_up_success(
    bool success
);

uint32_t fake_converter_get_init_call_count(void);
uint32_t fake_converter_get_ready_call_count(void);
uint32_t fake_converter_get_read_call_count(void);
uint32_t fake_converter_get_power_down_call_count(void);
uint32_t fake_converter_get_power_up_call_count(void);

uint8_t fake_converter_get_data_pin(void);
uint8_t fake_converter_get_clock_pin(void);
uint8_t fake_converter_get_power_down_pin(void);
uint8_t fake_converter_get_gain0_pin(void);
uint8_t fake_converter_get_gain1_pin(void);
ads1232_gain_t fake_converter_get_ads1232_gain(void);

#ifdef __cplusplus
}
#endif

#endif
