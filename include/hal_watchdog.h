#ifndef HAL_WATCHDOG_H
#define HAL_WATCHDOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Application-visible reset causes.
 *
 * The values are independent bits because the AVR may
 * expose more than one reset flag at the same time.
 * A zero value means that no supported cause survived
 * the active boot path.
 */
typedef uint8_t hal_reset_cause_t;

enum
{
    HAL_RESET_CAUSE_UNKNOWN = 0U,
    HAL_RESET_CAUSE_POWER_ON = 1U << 0,
    HAL_RESET_CAUSE_EXTERNAL = 1U << 1,
    HAL_RESET_CAUSE_BROWN_OUT = 1U << 2,
    HAL_RESET_CAUSE_WATCHDOG = 1U << 3
};


/*
 * Returns the reset flags captured by the active
 * backend before ordinary C/C++ initialization.
 */
hal_reset_cause_t hal_watchdog_get_reset_cause(void);


/*
 * Enables the watchdog with the backend's configured
 * timeout.
 */
void hal_watchdog_enable(void);


/*
 * Restarts the watchdog countdown.
 */
void hal_watchdog_kick(void);


/*
 * Disables the watchdog.
 *
 * Production startup uses this through the backend's
 * early initialization hook so a watchdog reset cannot
 * create an inherited-reset loop.
 */
void hal_watchdog_disable(void);

#ifdef __cplusplus
}
#endif

#endif
