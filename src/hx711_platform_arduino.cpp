#include <Arduino.h>
#include <avr/interrupt.h>

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

extern "C" void hx711_platform_delay_us(uint16_t microseconds)
{
    delayMicroseconds(microseconds);
}

extern "C" hx711_platform_critical_state_t
hx711_platform_enter_critical(void)
{
    const uint8_t previous_state = SREG;

    cli();

    return (hx711_platform_critical_state_t)previous_state;
}

extern "C" void hx711_platform_exit_critical(
    hx711_platform_critical_state_t previous_state
)
{
    SREG = (uint8_t)previous_state;
}