#ifndef TEST_SCALE_FAKE_HX711_DRIVER_H
#define TEST_SCALE_FAKE_HX711_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "hx711_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

void fake_hx711_driver_reset(void);

void fake_hx711_driver_set_init_status(
    hx711_status_t status
);

void fake_hx711_driver_set_wait_ready_status(
    hx711_status_t status
);

void fake_hx711_driver_set_ready(
    bool ready
);

bool fake_hx711_driver_push_reading(
    hx711_status_t status,
    int32_t raw_value
);

uint32_t fake_hx711_driver_get_init_call_count(void);
uint32_t fake_hx711_driver_get_wait_ready_call_count(void);
uint32_t fake_hx711_driver_get_is_ready_call_count(void);
uint32_t fake_hx711_driver_get_read_raw_call_count(void);

uint8_t fake_hx711_driver_get_last_data_pin(void);
uint8_t fake_hx711_driver_get_last_clock_pin(void);
uint32_t fake_hx711_driver_get_last_timeout_ms(void);

uint16_t fake_hx711_driver_get_consumed_reading_count(void);
bool fake_hx711_driver_sequence_was_exhausted(void);

#ifdef __cplusplus
}
#endif

#endif
