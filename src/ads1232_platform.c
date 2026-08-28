#include "ads1232_platform.h"

#include "hal_critical.h"
#include "hal_gpio.h"
#include "hal_time.h"

void ads1232_platform_configure_input(uint8_t pin)
{
    hal_gpio_configure_input((hal_gpio_pin_t)pin);
}

void ads1232_platform_configure_output(uint8_t pin)
{
    hal_gpio_configure_output((hal_gpio_pin_t)pin);
}

bool ads1232_platform_read_pin(uint8_t pin)
{
    return hal_gpio_read((hal_gpio_pin_t)pin);
}

void ads1232_platform_write_pin(
    uint8_t pin,
    bool level
)
{
    hal_gpio_write(
        (hal_gpio_pin_t)pin,
        level
    );
}

uint32_t ads1232_platform_millis(void)
{
    return hal_time_millis();
}

void ads1232_platform_delay_us(
    uint16_t microseconds
)
{
    hal_time_delay_us(microseconds);
}

ads1232_platform_critical_state_t
ads1232_platform_enter_critical(void)
{
    const hal_critical_state_t previous_state =
        hal_critical_enter();

    return
        (ads1232_platform_critical_state_t)
        previous_state;
}

void ads1232_platform_exit_critical(
    ads1232_platform_critical_state_t previous_state
)
{
    hal_critical_exit(
        (hal_critical_state_t)previous_state
    );
}
