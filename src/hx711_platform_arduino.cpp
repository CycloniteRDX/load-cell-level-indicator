#include <Arduino.h>

#include "hx711_platform.h"

extern "C" void hx711_platform_configure_input(uint8_t pin)
{
    pinMode(pin, INPUT);
}

extern "C" void hx711_platform_configure_output(uint8_t pin)
{
    pinMode(pin, OUTPUT);
}

extern "C" bool hx711_platform_read_pin(uint8_t pin)
{
    return digitalRead(pin) == HIGH;
}

extern "C" void hx711_platform_write_pin(uint8_t pin, bool level)
{
    digitalWrite(pin, level ? HIGH : LOW);
}

extern "C" uint32_t hx711_platform_millis(void)
{
    return millis();
}