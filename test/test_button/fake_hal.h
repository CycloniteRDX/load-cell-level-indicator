#ifndef TEST_BUTTON_FAKE_HAL_H
#define TEST_BUTTON_FAKE_HAL_H

#include <stdbool.h>
#include <stdint.h>

#include "hal_gpio.h"


typedef enum
{
    FAKE_GPIO_MODE_UNCONFIGURED = 0,
    FAKE_GPIO_MODE_INPUT,
    FAKE_GPIO_MODE_INPUT_PULLUP,
    FAKE_GPIO_MODE_OUTPUT
} fake_gpio_mode_t;


void fake_hal_reset(void);

void fake_hal_set_time_ms(
    uint32_t time_ms
);

void fake_hal_advance_time_ms(
    uint32_t elapsed_ms
);

void fake_hal_set_pin_input(
    hal_gpio_pin_t pin,
    bool level
);

fake_gpio_mode_t fake_hal_get_pin_mode(
    hal_gpio_pin_t pin
);

bool fake_hal_get_pin_output(
    hal_gpio_pin_t pin
);


#endif