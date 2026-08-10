#include <avr/interrupt.h>

#include "app.h"
#include "hal_watchdog.h"


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

        /*
         * Reaching this point proves that one complete
         * application iteration returned. Never feed
         * the watchdog from an interrupt.
         */
        hal_watchdog_kick();
    }
}
