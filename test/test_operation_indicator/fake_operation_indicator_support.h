#ifndef TEST_OPERATION_INDICATOR_FAKE_SUPPORT_H
#define TEST_OPERATION_INDICATOR_FAKE_SUPPORT_H

#include <stdbool.h>
#include <stdint.h>

void fake_operation_indicator_reset(void);

void fake_operation_indicator_set_time_ms(
    uint32_t time_ms
);

void fake_operation_indicator_advance_time_ms(
    uint32_t elapsed_ms
);

bool fake_operation_indicator_low_led_is_on(void);
bool fake_operation_indicator_medium_led_is_on(void);
bool fake_operation_indicator_high_led_is_on(void);

#endif
