#include <Arduino.h>

#include "hal_gpio.h"

extern "C" void hal_gpio_configure_input(
    hal_gpio_pin_t pin
)
{
    pinMode(pin, INPUT);
}

extern "C" void hal_gpio_configure_input_pullup(
    hal_gpio_pin_t pin
)
{
    pinMode(pin, INPUT_PULLUP);
}

extern "C" void hal_gpio_configure_output(
    hal_gpio_pin_t pin
)
{
    pinMode(pin, OUTPUT);
}

extern "C" bool hal_gpio_read(
    hal_gpio_pin_t pin
)
{
    return digitalRead(pin) == HIGH;
}

extern "C" void hal_gpio_write(
    hal_gpio_pin_t pin,
    bool level
)
{
    digitalWrite(pin, level ? HIGH : LOW);
}