#include "fake_ads1232_platform.h"

#include <limits.h>

#include "ads1232_platform.h"

#define FAKE_ADS1232_PIN_COUNT       32U
#define FAKE_ADS1232_DATA_BITS       24U
#define FAKE_ADS1232_DELAY_CAPACITY  64U

static fake_ads1232_pin_mode_t pin_modes[
    FAKE_ADS1232_PIN_COUNT
];

static bool pin_output_levels[
    FAKE_ADS1232_PIN_COUNT
];

static uint32_t pin_write_counts[
    FAKE_ADS1232_PIN_COUNT
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

static uint16_t requested_delays[
    FAKE_ADS1232_DELAY_CAPACITY
];

static uint32_t delay_call_count = 0U;

static bool fake_ads1232_pin_is_valid(
    uint8_t pin
)
{
    return pin < FAKE_ADS1232_PIN_COUNT;
}

void fake_ads1232_platform_reset(void)
{
    for (uint8_t pin = 0U;
         pin < FAKE_ADS1232_PIN_COUNT;
         ++pin)
    {
        pin_modes[pin] =
            FAKE_ADS1232_PIN_UNCONFIGURED;

        pin_output_levels[pin] = false;
        pin_write_counts[pin] = 0U;
    }

    for (uint8_t index = 0U;
         index < FAKE_ADS1232_DELAY_CAPACITY;
         ++index)
    {
        requested_delays[index] = 0U;
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

    delay_call_count = 0U;
}

void fake_ads1232_platform_set_ready(
    bool ready
)
{
    conversion_ready = ready;
}

void fake_ads1232_platform_load_raw_24(
    uint32_t raw_value
)
{
    simulated_raw_24 =
        raw_value & 0x00FFFFFFUL;
}

void fake_ads1232_platform_set_time_ms(
    uint32_t time_ms
)
{
    current_time_ms = time_ms;
}

void fake_ads1232_platform_set_millis_step(
    uint32_t step_ms
)
{
    millis_step_ms = step_ms;
}

void fake_ads1232_platform_set_critical_state(
    uintptr_t critical_state
)
{
    simulated_critical_state = critical_state;
}

fake_ads1232_pin_mode_t
fake_ads1232_platform_get_pin_mode(
    uint8_t pin
)
{
    if (!fake_ads1232_pin_is_valid(pin))
    {
        return FAKE_ADS1232_PIN_UNCONFIGURED;
    }

    return pin_modes[pin];
}

bool fake_ads1232_platform_get_pin_output(
    uint8_t pin
)
{
    if (!fake_ads1232_pin_is_valid(pin))
    {
        return false;
    }

    return pin_output_levels[pin];
}

uint32_t fake_ads1232_platform_get_pin_write_count(
    uint8_t pin
)
{
    if (!fake_ads1232_pin_is_valid(pin))
    {
        return 0U;
    }

    return pin_write_counts[pin];
}

uint8_t
fake_ads1232_platform_get_last_pulse_count(void)
{
    return last_pulse_count;
}

uint32_t
fake_ads1232_platform_get_critical_enter_count(void)
{
    return critical_enter_count;
}

uint32_t
fake_ads1232_platform_get_critical_exit_count(void)
{
    return critical_exit_count;
}

uintptr_t
fake_ads1232_platform_get_restored_critical_state(void)
{
    return restored_critical_state;
}

uint32_t
fake_ads1232_platform_get_delay_call_count(void)
{
    return delay_call_count;
}

uint16_t fake_ads1232_platform_get_delay_us(
    uint8_t index
)
{
    if (index >= FAKE_ADS1232_DELAY_CAPACITY)
    {
        return 0U;
    }

    return requested_delays[index];
}

void ads1232_platform_configure_input(
    uint8_t pin
)
{
    if (!fake_ads1232_pin_is_valid(pin))
    {
        return;
    }

    pin_modes[pin] = FAKE_ADS1232_PIN_INPUT;
    configured_data_pin = pin;
}

void ads1232_platform_configure_output(
    uint8_t pin
)
{
    if (!fake_ads1232_pin_is_valid(pin))
    {
        return;
    }

    pin_modes[pin] = FAKE_ADS1232_PIN_OUTPUT;

    if (configured_clock_pin == UINT8_MAX)
    {
        configured_clock_pin = pin;
    }
}

bool ads1232_platform_read_pin(
    uint8_t pin
)
{
    if (pin != configured_data_pin)
    {
        return false;
    }

    if (inside_critical_section &&
        clock_level &&
        (data_bit_index < FAKE_ADS1232_DATA_BITS))
    {
        const uint8_t bit_position =
            (uint8_t)(
                (FAKE_ADS1232_DATA_BITS - 1U) -
                data_bit_index
            );

        const bool bit_value =
            ((simulated_raw_24 >> bit_position) &
             1UL) != 0U;

        ++data_bit_index;

        return bit_value;
    }

    return !conversion_ready;
}

void ads1232_platform_write_pin(
    uint8_t pin,
    bool level
)
{
    if (!fake_ads1232_pin_is_valid(pin))
    {
        return;
    }

    pin_output_levels[pin] = level;
    ++pin_write_counts[pin];

    if (pin != configured_clock_pin)
    {
        return;
    }

    if (inside_critical_section &&
        !clock_level &&
        level)
    {
        ++current_pulse_count;
    }

    clock_level = level;
}

uint32_t ads1232_platform_millis(void)
{
    const uint32_t returned_time =
        current_time_ms;

    current_time_ms += millis_step_ms;

    return returned_time;
}

void ads1232_platform_delay_us(
    uint16_t microseconds
)
{
    if (delay_call_count <
        FAKE_ADS1232_DELAY_CAPACITY)
    {
        requested_delays[delay_call_count] =
            microseconds;
    }

    ++delay_call_count;
}

ads1232_platform_critical_state_t
ads1232_platform_enter_critical(void)
{
    inside_critical_section = true;
    data_bit_index = 0U;
    current_pulse_count = 0U;
    ++critical_enter_count;

    return
        (ads1232_platform_critical_state_t)
        simulated_critical_state;
}

void ads1232_platform_exit_critical(
    ads1232_platform_critical_state_t previous_state
)
{
    inside_critical_section = false;
    last_pulse_count = current_pulse_count;
    restored_critical_state =
        (uintptr_t)previous_state;
    conversion_ready = false;
    ++critical_exit_count;
}
