#include <avr/interrupt.h>

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
     * Interrupts deliberately remain enabled so USART
     * output and the project timebase may continue. The
     * main execution path never returns to the watchdog
     * kick below, which must cause a hardware reset.
     */
    while (true)
    {
    }
}
#endif


int main(void)
{
    /*
     * The project millisecond timebase and USART
     * reception are interrupt-driven.
     *
     * Global interrupts must be enabled before
     * app_init(), because app_init() eventually calls
     * hal_time_delay_ms() during the startup sequence.
     */
    sei();

    /*
     * Initialize the complete application exactly once,
     * replacing the role previously performed by the
     * Arduino setup() function.
     */
    app_init();

#if defined(WATCHDOG_HARDWARE_VALIDATION)
    init_watchdog_hardware_validation();
#endif

    /*
     * Watchdog supervision begins only after bounded
     * initialization has established safe outputs and
     * a complete application state.
     */
    hal_watchdog_enable();

    /*
     * Embedded firmware has no operating system to
     * return to. Execute application iterations forever,
     * replacing the repeated Arduino loop() calls.
     */
    while (true)
    {
        app_update();

#if defined(WATCHDOG_HARDWARE_VALIDATION)
        /*
         * Only the dedicated validation environments
         * can intentionally prevent the next kick.
         */
        run_watchdog_hardware_validation();
#endif

        /*
         * Reaching this point proves that one complete
         * application iteration returned. Never feed
         * the watchdog from an interrupt.
         */
        hal_watchdog_kick();
    }
}
