#include "fake_operation_indicator_support.h"

#include "hal_time.h"
#include "indicator_leds.h"

static uint32_t current_time_ms = 0U;

static bool low_led_on = false;
static bool medium_led_on = false;
static bool high_led_on = false;

void fake_operation_indicator_reset(void)
{
    current_time_ms = 0U;

    low_led_on = false;
    medium_led_on = false;
    high_led_on = false;
}

void fake_operation_indicator_set_time_ms(
    uint32_t time_ms
)
{
    current_time_ms = time_ms;
}

void fake_operation_indicator_advance_time_ms(
    uint32_t elapsed_ms
)
{
    current_time_ms += elapsed_ms;
}

bool fake_operation_indicator_low_led_is_on(void)
{
    return low_led_on;
}

bool fake_operation_indicator_medium_led_is_on(void)
{
    return medium_led_on;
}

bool fake_operation_indicator_high_led_is_on(void)
{
    return high_led_on;
}

uint32_t hal_time_millis(void)
{
    return current_time_ms;
}

void hal_time_delay_us(
    uint16_t microseconds
)
{
    (void)microseconds;
}

void indicator_leds_init(void)
{
    indicator_leds_off();
}

void indicator_leds_set(
    bool new_low_led_on,
    bool new_medium_led_on,
    bool new_high_led_on
)
{
    low_led_on = new_low_led_on;
    medium_led_on = new_medium_led_on;
    high_led_on = new_high_led_on;
}

void indicator_leds_off(void)
{
    indicator_leds_set(
        false,
        false,
        false
    );
}
