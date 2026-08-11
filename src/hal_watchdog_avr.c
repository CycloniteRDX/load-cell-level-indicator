#include <avr/io.h>
#include <avr/wdt.h>

#include <stdint.h>

#include "hal_watchdog.h"


/*
 * The raw MCUSR flags must survive ordinary runtime
 * initialization so the application can report them
 * after the console becomes available.
 */
static uint8_t captured_reset_flags
    __attribute__((section(".noinit")));


/*
 * AVR-LibC executes .init3 code before ordinary C/C++
 * initialization and before main()/setup().
 *
 * The naked function intentionally has no generated
 * prologue, epilogue or return. The linker concatenates
 * the .init sections, so execution falls through to the
 * next startup section after this code.
 */
void hal_watchdog_early_init(void)
    __attribute__((naked))
    __attribute__((section(".init3")))
    __attribute__((used));

void hal_watchdog_early_init(void)
{
    captured_reset_flags = MCUSR;

    /*
     * WDRF forces the watchdog enable bit while it is
     * set. Clear all captured flags before disabling the
     * inherited watchdog.
     */
    MCUSR = 0U;
    wdt_disable();
}


hal_reset_cause_t hal_watchdog_get_reset_cause(void)
{
    hal_reset_cause_t cause =
        HAL_RESET_CAUSE_UNKNOWN;

    if ((captured_reset_flags & _BV(PORF)) != 0U)
    {
        cause = (hal_reset_cause_t)(
            cause |
            HAL_RESET_CAUSE_POWER_ON
        );
    }

    if ((captured_reset_flags & _BV(EXTRF)) != 0U)
    {
        cause = (hal_reset_cause_t)(
            cause |
            HAL_RESET_CAUSE_EXTERNAL
        );
    }

    if ((captured_reset_flags & _BV(BORF)) != 0U)
    {
        cause = (hal_reset_cause_t)(
            cause |
            HAL_RESET_CAUSE_BROWN_OUT
        );
    }

    if ((captured_reset_flags & _BV(WDRF)) != 0U)
    {
        cause = (hal_reset_cause_t)(
            cause |
            HAL_RESET_CAUSE_WATCHDOG
        );
    }

    return cause;
}


void hal_watchdog_enable(void)
{
    /*
     * The selected two-second hardware period matches
     * the v1.3 design contract. Enabling the watchdog
     * also restarts its countdown.
     */
    wdt_enable(WDTO_2S);
}


void hal_watchdog_kick(void)
{
    wdt_reset();
}


void hal_watchdog_disable(void)
{
    /*
     * WDRF must be cleared before WDE can be cleared.
     */
    MCUSR &= (uint8_t)~_BV(WDRF);
    wdt_disable();
}
