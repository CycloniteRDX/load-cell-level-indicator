#ifndef ADS1232_PLATFORM_H
#define ADS1232_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t ads1232_platform_critical_state_t;

void ads1232_platform_configure_input(uint8_t pin);

void ads1232_platform_configure_output(uint8_t pin);

bool ads1232_platform_read_pin(uint8_t pin);

void ads1232_platform_write_pin(
    uint8_t pin,
    bool level
);

uint32_t ads1232_platform_millis(void);

void ads1232_platform_delay_us(
    uint16_t microseconds
);

ads1232_platform_critical_state_t
ads1232_platform_enter_critical(void);

void ads1232_platform_exit_critical(
    ads1232_platform_critical_state_t previous_state
);

#ifdef __cplusplus
}
#endif

#endif
