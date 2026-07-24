#ifndef HAL_TIME_H
#define HAL_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t hal_time_millis(void);

void hal_time_delay_us(
    uint16_t microseconds
);

#ifdef __cplusplus
}
#endif

#endif