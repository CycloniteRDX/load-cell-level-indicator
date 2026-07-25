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


void hal_time_delay_us(
    uint16_t microseconds
);

#ifdef __cplusplus
}
#endif

#endif