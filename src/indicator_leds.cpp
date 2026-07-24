#include "hal_gpio.h"
#include "config.h"
#include "indicator_leds.h"


static void write_led(
    uint8_t pin,
    bool led_on
)
{
    hal_gpio_write(pin, led_on);
}


void indicator_leds_init(void)
{
    hal_gpio_configure_output(LOW_LEVEL_LED_PIN);
    hal_gpio_configure_output(MEDIUM_LEVEL_LED_PIN);
    hal_gpio_configure_output(HIGH_LEVEL_LED_PIN);

    indicator_leds_off();
}


void indicator_leds_set(
    bool low_led_on,
    bool medium_led_on,
    bool high_led_on
)
{
    write_led(
        LOW_LEVEL_LED_PIN,
        low_led_on
    );

    write_led(
        MEDIUM_LEVEL_LED_PIN,
        medium_led_on
    );

    write_led(
        HIGH_LEVEL_LED_PIN,
        high_led_on
    );
}


void indicator_leds_off(void)
{
    indicator_leds_set(
        false,
        false,
        false
    );
}