#include <avr/io.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hal_critical.h"
#include "hal_gpio.h"


#define AVR_GPIO_PORT_D_LAST_PIN 7U

#define AVR_GPIO_PORT_B_FIRST_PIN 8U
#define AVR_GPIO_PORT_B_LAST_PIN  13U

#define AVR_GPIO_PORT_C_FIRST_PIN 14U
#define AVR_GPIO_PORT_C_LAST_PIN  19U


typedef struct
{
    volatile uint8_t *direction_register;
    volatile uint8_t *output_register;
    volatile uint8_t *input_register;
    uint8_t bit_mask;
    bool valid;
} avr_gpio_pin_mapping_t;


static inline avr_gpio_pin_mapping_t
avr_gpio_make_invalid_mapping(void)
{
    const avr_gpio_pin_mapping_t mapping =
    {
        .direction_register = NULL,
        .output_register = NULL,
        .input_register = NULL,
        .bit_mask = 0U,
        .valid = false
    };

    return mapping;
}


static inline avr_gpio_pin_mapping_t
avr_gpio_make_mapping(
    volatile uint8_t *direction_register,
    volatile uint8_t *output_register,
    volatile uint8_t *input_register,
    uint8_t bit_position
)
{
    const avr_gpio_pin_mapping_t mapping =
    {
        .direction_register = direction_register,
        .output_register = output_register,
        .input_register = input_register,
        .bit_mask = (uint8_t)(1U << bit_position),
        .valid = true
    };

    return mapping;
}


static inline avr_gpio_pin_mapping_t
avr_gpio_get_pin_mapping(
    hal_gpio_pin_t pin
)
{
    if (pin <= AVR_GPIO_PORT_D_LAST_PIN)
    {
        return avr_gpio_make_mapping(
            &DDRD,
            &PORTD,
            &PIND,
            pin
        );
    }

    if ((pin >= AVR_GPIO_PORT_B_FIRST_PIN) &&
        (pin <= AVR_GPIO_PORT_B_LAST_PIN))
    {
        return avr_gpio_make_mapping(
            &DDRB,
            &PORTB,
            &PINB,
            (uint8_t)(
                pin - AVR_GPIO_PORT_B_FIRST_PIN
            )
        );
    }

    if ((pin >= AVR_GPIO_PORT_C_FIRST_PIN) &&
        (pin <= AVR_GPIO_PORT_C_LAST_PIN))
    {
        return avr_gpio_make_mapping(
            &DDRC,
            &PORTC,
            &PINC,
            (uint8_t)(
                pin - AVR_GPIO_PORT_C_FIRST_PIN
            )
        );
    }

    return avr_gpio_make_invalid_mapping();
}


void hal_gpio_configure_input(
    hal_gpio_pin_t pin
)
{
    const avr_gpio_pin_mapping_t mapping =
        avr_gpio_get_pin_mapping(pin);

    if (!mapping.valid)
    {
        return;
    }

    const hal_critical_state_t previous_state =
        hal_critical_enter();

    *mapping.direction_register &=
        (uint8_t)~mapping.bit_mask;

    *mapping.output_register &=
        (uint8_t)~mapping.bit_mask;

    hal_critical_exit(previous_state);
}


void hal_gpio_configure_input_pullup(
    hal_gpio_pin_t pin
)
{
    const avr_gpio_pin_mapping_t mapping =
        avr_gpio_get_pin_mapping(pin);

    if (!mapping.valid)
    {
        return;
    }

    const hal_critical_state_t previous_state =
        hal_critical_enter();

    *mapping.direction_register &=
        (uint8_t)~mapping.bit_mask;

    *mapping.output_register |=
        mapping.bit_mask;

    hal_critical_exit(previous_state);
}


void hal_gpio_configure_output(
    hal_gpio_pin_t pin
)
{
    const avr_gpio_pin_mapping_t mapping =
        avr_gpio_get_pin_mapping(pin);

    if (!mapping.valid)
    {
        return;
    }

    const hal_critical_state_t previous_state =
        hal_critical_enter();

    *mapping.direction_register |=
        mapping.bit_mask;

    hal_critical_exit(previous_state);
}
