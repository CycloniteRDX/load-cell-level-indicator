#include "fake_hx711_platform.h"

#include "hx711_platform.h"


#define FAKE_HX711_PIN_COUNT 32U
#define FAKE_HX711_DATA_BITS 24U


static fake_hx711_pin_mode_t pin_modes[
    FAKE_HX711_PIN_COUNT
];

static bool pin_output_levels[
    FAKE_HX711_PIN_COUNT
];

static uint8_t configured_data_pin = UINT8_MAX;
static uint8_t configured_clock_pin = UINT8_MAX;

static bool conversion_ready = true;

static uint32_t simulated_raw_24 = 0U;
static uint8_t data_bit_index = 0U;

static bool clock_level = false;
static bool inside_critical_section = false;

static uint8_t current_pulse_count = 0U;
static uint8_t last_pulse_count = 0U;

static uint32_t current_time_ms = 0U;
static uint32_t millis_step_ms = 0U;

static uintptr_t simulated_critical_state = 0U;
static uintptr_t restored_critical_state = 0U;

static uint32_t critical_enter_count = 0U;
static uint32_t critical_exit_count = 0U;


static bool fake_hx711_pin_is_valid(
    uint8_t pin
)
{
    return pin < FAKE_HX711_PIN_COUNT;
}


void fake_hx711_platform_reset(void)
{
    for (uint8_t pin = 0U;
         pin < FAKE_HX711_PIN_COUNT;
         ++pin)
    {
        pin_modes[pin] =
            FAKE_HX711_PIN_UNCONFIGURED;

        pin_output_levels[pin] = false;
    }

    configured_data_pin = UINT8_MAX;
    configured_clock_pin = UINT8_MAX;

    conversion_ready = true;

    simulated_raw_24 = 0U;
    data_bit_index = 0U;

    clock_level = false;
    inside_critical_section = false;

    current_pulse_count = 0U;
    last_pulse_count = 0U;

    current_time_ms = 0U;
    millis_step_ms = 0U;

    simulated_critical_state = 0U;
    restored_critical_state = 0U;

    critical_enter_count = 0U;
    critical_exit_count = 0U;
}


void fake_hx711_platform_set_ready(
    bool ready
)
{
    conversion_ready = ready;
}


void fake_hx711_platform_load_raw_24(
    uint32_t raw_value
)
{
    simulated_raw_24 =
        raw_value & 0x00FFFFFFUL;
}


void fake_hx711_platform_set_time_ms(
    uint32_t time_ms
)
{
    current_time_ms = time_ms;
}


void fake_hx711_platform_set_millis_step(
    uint32_t step_ms
)
{
    millis_step_ms = step_ms;
}


void fake_hx711_platform_set_critical_state(
    uintptr_t critical_state
)
{
    simulated_critical_state =
        critical_state;
}


fake_hx711_pin_mode_t
fake_hx711_platform_get_pin_mode(
    uint8_t pin
)
{
    if (!fake_hx711_pin_is_valid(pin))
    {
        return FAKE_HX711_PIN_UNCONFIGURED;
    }

    return pin_modes[pin];
}


bool fake_hx711_platform_get_pin_output(
    uint8_t pin
)
{
    if (!fake_hx711_pin_is_valid(pin))
    {
        return false;
    }

    return pin_output_levels[pin];
}


uint8_t
fake_hx711_platform_get_last_pulse_count(void)
{
    return last_pulse_count;
}


uint32_t
fake_hx711_platform_get_critical_enter_count(void)
{
    return critical_enter_count;
}


uint32_t
fake_hx711_platform_get_critical_exit_count(void)
{
    return critical_exit_count;
}


uintptr_t
fake_hx711_platform_get_restored_critical_state(void)
{
    return restored_critical_state;
}


/*
 * Fake implementation of hx711_platform.h
 */

void hx711_platform_configure_input(
    uint8_t pin
)
{
    if (!fake_hx711_pin_is_valid(pin))
    {
        return;
    }

    pin_modes[pin] =
        FAKE_HX711_PIN_INPUT;

    configured_data_pin = pin;
}


void hx711_platform_configure_output(
    uint8_t pin
)
{
    if (!fake_hx711_pin_is_valid(pin))
    {
        return;
    }

    pin_modes[pin] =
        FAKE_HX711_PIN_OUTPUT;

    configured_clock_pin = pin;
}


bool hx711_platform_read_pin(
    uint8_t pin
)
{
    if (pin != configured_data_pin)
    {
        return false;
    }

    /*
     * During the 24-bit transfer, return the next
     * simulated data bit while PD_SCK is HIGH.
     */
    if (inside_critical_section &&
        clock_level &&
        (data_bit_index < FAKE_HX711_DATA_BITS))
    {
        const uint8_t bit_position =
            (uint8_t)(
                (FAKE_HX711_DATA_BITS - 1U) -
                data_bit_index
            );

        const bool bit_value =
            ((simulated_raw_24 >> bit_position) &
             1UL) != 0U;

        ++data_bit_index;

        return bit_value;
    }

    /*
     * Outside the data transfer:
     *
     * LOW  = conversion ready
     * HIGH = conversion not ready
     */
    return !conversion_ready;
}


void hx711_platform_write_pin(
    uint8_t pin,
    bool level
)
{
    if (!fake_hx711_pin_is_valid(pin))
    {
        return;
    }

    pin_output_levels[pin] = level;

    if (pin != configured_clock_pin)
    {
        return;
    }

    /*
     * Count each LOW-to-HIGH transition generated
     * during the protected reading sequence.
     */
    if (inside_critical_section &&
        !clock_level &&
        level)
    {
        ++current_pulse_count;
    }

    clock_level = level;
}


uint32_t hx711_platform_millis(void)
{
    const uint32_t returned_time =
        current_time_ms;

    current_time_ms += millis_step_ms;

    return returned_time;
}


void hx711_platform_delay_us(
    uint16_t microseconds
)
{
    /*
     * Native tests do not wait in real time.
     */
    (void)microseconds;
}


hx711_platform_critical_state_t
hx711_platform_enter_critical(void)
{
    inside_critical_section = true;

    data_bit_index = 0U;
    current_pulse_count = 0U;

    ++critical_enter_count;

    return
        (hx711_platform_critical_state_t)
        simulated_critical_state;
}


void hx711_platform_exit_critical(
    hx711_platform_critical_state_t previous_state
)
{
    inside_critical_section = false;

    last_pulse_count =
        current_pulse_count;

    restored_critical_state =
        (uintptr_t)previous_state;

    ++critical_exit_count;
}