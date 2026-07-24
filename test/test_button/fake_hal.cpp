#include "fake_hal.h"

#include "hal_time.h"


#define FAKE_HAL_PIN_COUNT 32U


static fake_gpio_mode_t pin_modes[
    FAKE_HAL_PIN_COUNT
];

static bool pin_input_levels[
    FAKE_HAL_PIN_COUNT
];

static bool pin_output_levels[
    FAKE_HAL_PIN_COUNT
];

static uint32_t current_time_ms = 0U;


static bool fake_hal_pin_is_valid(
    hal_gpio_pin_t pin
)
{
    return pin < FAKE_HAL_PIN_COUNT;
}


void fake_hal_reset(void)
{
    current_time_ms = 0U;

    for (uint8_t pin = 0U;
         pin < FAKE_HAL_PIN_COUNT;
         ++pin)
    {
        pin_modes[pin] =
            FAKE_GPIO_MODE_UNCONFIGURED;

        /*
         * Default released state for a button using
         * an internal pull-up.
         */
        pin_input_levels[pin] = true;

        pin_output_levels[pin] = false;
    }
}


void fake_hal_set_time_ms(
    uint32_t time_ms
)
{
    current_time_ms = time_ms;
}


void fake_hal_advance_time_ms(
    uint32_t elapsed_ms
)
{
    current_time_ms += elapsed_ms;
}


void fake_hal_set_pin_input(
    hal_gpio_pin_t pin,
    bool level
)
{
    if (!fake_hal_pin_is_valid(pin))
    {
        return;
    }

    pin_input_levels[pin] = level;
}


fake_gpio_mode_t fake_hal_get_pin_mode(
    hal_gpio_pin_t pin
)
{
    if (!fake_hal_pin_is_valid(pin))
    {
        return FAKE_GPIO_MODE_UNCONFIGURED;
    }

    return pin_modes[pin];
}


bool fake_hal_get_pin_output(
    hal_gpio_pin_t pin
)
{
    if (!fake_hal_pin_is_valid(pin))
    {
        return false;
    }

    return pin_output_levels[pin];
}


/*
 * Fake implementation of the production GPIO HAL.
 */

void hal_gpio_configure_input(
    hal_gpio_pin_t pin
)
{
    if (!fake_hal_pin_is_valid(pin))
    {
        return;
    }

    pin_modes[pin] = FAKE_GPIO_MODE_INPUT;
}


void hal_gpio_configure_input_pullup(
    hal_gpio_pin_t pin
)
{
    if (!fake_hal_pin_is_valid(pin))
    {
        return;
    }

    pin_modes[pin] =
        FAKE_GPIO_MODE_INPUT_PULLUP;
}


void hal_gpio_configure_output(
    hal_gpio_pin_t pin
)
{
    if (!fake_hal_pin_is_valid(pin))
    {
        return;
    }

    pin_modes[pin] = FAKE_GPIO_MODE_OUTPUT;
}


bool hal_gpio_read(
    hal_gpio_pin_t pin
)
{
    if (!fake_hal_pin_is_valid(pin))
    {
        return false;
    }

    return pin_input_levels[pin];
}


void hal_gpio_write(
    hal_gpio_pin_t pin,
    bool level
)
{
    if (!fake_hal_pin_is_valid(pin))
    {
        return;
    }

    pin_output_levels[pin] = level;
}


/*
 * Fake implementation of the production time HAL.
 */

uint32_t hal_time_millis(void)
{
    return current_time_ms;
}


void hal_time_delay_us(
    uint16_t microseconds
)
{
    /*
     * The button module does not use microsecond delays.
     * The function exists to complete the HAL interface.
     */ 
    (void)microseconds;
}