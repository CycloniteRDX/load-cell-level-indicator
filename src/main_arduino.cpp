#include <Arduino.h>

#include "app.h"
#include "hal_watchdog.h"

#if defined(WATCHDOG_HARDWARE_VALIDATION)
#include "config.h"
#include "console.h"
#include "hal_gpio.h"
#include "watchdog_validation.h"

static watchdog_validation_t watchdog_validation;


static void init_watchdog_hardware_validation(void)
{
    watchdog_validation_init(&watchdog_validation);

    console_newline();
    CONSOLE_PRINTLN(
        "=== WATCHDOG HARDWARE VALIDATION BUILD ==="
    );
    CONSOLE_PRINTLN(
        "Release D4 and D8, then press both together."
    );
    CONSOLE_PRINTLN(
        "The main loop will stall until the watchdog resets the MCU."
    );
}


static void run_watchdog_hardware_validation(void)
{
    const bool tare_button_released =
        hal_gpio_read(TARE_BUTTON_PIN);

    const bool calibration_button_released =
        hal_gpio_read(CALIBRATION_BUTTON_PIN);

    if (!watchdog_validation_should_stall(
            &watchdog_validation,
            tare_button_released,
            calibration_button_released))
    {
        return;
    }

    CONSOLE_PRINTLN(
        "WATCHDOG TEST: intentional main-loop stall started."
    );
    CONSOLE_PRINTLN(
        "No further watchdog kicks will occur."
    );

    /*
     * Arduino interrupts remain enabled so buffered
     * USART output can drain. The loop never returns to
     * the watchdog kick below, which must cause a reset.
     */
    while (true)
    {
    }
}
#endif


void setup(void)
{
    app_init();

#if defined(WATCHDOG_HARDWARE_VALIDATION)
    init_watchdog_hardware_validation();
#endif

    hal_watchdog_enable();
}


void loop(void)
{
    app_update();

#if defined(WATCHDOG_HARDWARE_VALIDATION)
    /*
     * Only the dedicated validation environments can
     * intentionally prevent the next kick.
     */
    run_watchdog_hardware_validation();
#endif

    hal_watchdog_kick();
}
