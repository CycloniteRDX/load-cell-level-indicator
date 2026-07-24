#ifndef TEST_HX711_FAKE_PLATFORM_H
#define TEST_HX711_FAKE_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>


typedef enum
{
    FAKE_HX711_PIN_UNCONFIGURED = 0,
    FAKE_HX711_PIN_INPUT,
    FAKE_HX711_PIN_OUTPUT
} fake_hx711_pin_mode_t;


void fake_hx711_platform_reset(void);

void fake_hx711_platform_set_ready(
    bool ready
);

void fake_hx711_platform_load_raw_24(
    uint32_t raw_value
);

void fake_hx711_platform_set_time_ms(
    uint32_t time_ms
);

void fake_hx711_platform_set_millis_step(
    uint32_t step_ms
);

void fake_hx711_platform_set_critical_state(
    uintptr_t critical_state
);

fake_hx711_pin_mode_t
fake_hx711_platform_get_pin_mode(
    uint8_t pin
);

bool fake_hx711_platform_get_pin_output(
    uint8_t pin
);

uint8_t
fake_hx711_platform_get_last_pulse_count(void);

uint32_t
fake_hx711_platform_get_critical_enter_count(void);

uint32_t
fake_hx711_platform_get_critical_exit_count(void);

uintptr_t
fake_hx711_platform_get_restored_critical_state(void);


#endif