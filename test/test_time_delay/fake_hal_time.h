#ifndef TEST_TIME_DELAY_FAKE_HAL_TIME_H
#define TEST_TIME_DELAY_FAKE_HAL_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void fake_hal_time_reset(void);

void fake_hal_time_configure(
    uint32_t initial_time,
    uint32_t time_step,
    uint32_t reads_per_time_value
);

uint32_t fake_hal_time_get_read_count(void);
uint32_t fake_hal_time_get_last_returned_time(void);
uint32_t fake_hal_time_get_next_time(void);

#ifdef __cplusplus
}
#endif

#endif
