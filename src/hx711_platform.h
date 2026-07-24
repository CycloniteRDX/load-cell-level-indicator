#ifndef HX711_PLATFORM_H
#define HX711_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t hx711_platform_critical_state_t;

void hx711_platform_configure_input(uint8_t pin);

void hx711_platform_configure_output(uint8_t pin);

bool hx711_platform_read_pin(uint8_t pin);

void hx711_platform_write_pin(uint8_t pin, bool level);

uint32_t hx711_platform_millis(void);

void hx711_platform_delay_us(uint16_t microseconds);

hx711_platform_critical_state_t
hx711_platform_enter_critical(void);

void hx711_platform_exit_critical(
    hx711_platform_critical_state_t previous_state
);

#ifdef __cplusplus
}
#endif

#endif