#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t hal_gpio_pin_t;

void hal_gpio_configure_input(
    hal_gpio_pin_t pin
);

void hal_gpio_configure_input_pullup(
    hal_gpio_pin_t pin
);

void hal_gpio_configure_output(
    hal_gpio_pin_t pin
);

bool hal_gpio_read(
    hal_gpio_pin_t pin
);

void hal_gpio_write(
    hal_gpio_pin_t pin,
    bool level
);

#ifdef __cplusplus
}
#endif

#endif