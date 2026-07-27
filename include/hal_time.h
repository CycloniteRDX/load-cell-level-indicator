#ifndef HAL_TIME_H
#define HAL_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initializes the active time backend.
 *
 * The Arduino backend currently performs no work.
 * Future hardware-specific backends may configure
 * timers and interrupts here.
 */
void hal_time_init(void);


uint32_t hal_time_millis(void);


/*
 * Blocks until the requested number of milliseconds
 * has elapsed according to hal_time_millis().
 *
 * A zero-duration request returns immediately.
 *
 * This function requires the active millisecond time
 * source to have been initialized and able to advance.
 * It must not be called from an interrupt service
 * routine or while the timer interrupt is disabled.
 */
void hal_time_delay_ms(
    uint32_t milliseconds
);


void hal_time_delay_us(
    uint16_t microseconds
);

#ifdef __cplusplus
}
#endif

#endif
