#include "hx711_platform.h"

#include "hal_critical.h"
#include "hal_gpio.h"
#include "hal_time.h"

void hx711_platform_configure_input(uint8_t pin)
{
    hal_gpio_configure_input((hal_gpio_pin_t)pin);
}

void hx711_platform_configure_output(uint8_t pin)
{
    hal_gpio_configure_output((hal_gpio_pin_t)pin);
}

bool hx711_platform_read_pin(uint8_t pin)
{
    return hal_gpio_read((hal_gpio_pin_t)pin);
}

void hx711_platform_write_pin(
    uint8_t pin,
    bool level
)
{
    hal_gpio_write(
        (hal_gpio_pin_t)pin,
        level
    );
}

uint32_t hx711_platform_millis(void)
{
    return hal_time_millis();
}

void hx711_platform_delay_us(
    uint16_t microseconds
)
{
    hal_time_delay_us(microseconds);
}

hx711_platform_critical_state_t
hx711_platform_enter_critical(void)
{
    const hal_critical_state_t previous_state =
        hal_critical_enter();

    return
        (hx711_platform_critical_state_t)previous_state;
}

void hx711_platform_exit_critical(
    hx711_platform_critical_state_t previous_state
)
{
    hal_critical_exit(
        (hal_critical_state_t)previous_state
    );
}