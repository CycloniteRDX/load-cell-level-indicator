#ifndef TEST_LEVEL_INDICATOR_FAKE_SUPPORT_H
#define TEST_LEVEL_INDICATOR_FAKE_SUPPORT_H

#include <stdbool.h>
#include <stdint.h>

void fake_level_indicator_support_reset(void);

void fake_level_indicator_set_time_ms(
    uint32_t time_ms
);

void fake_level_indicator_advance_time_ms(
    uint32_t elapsed_ms
);

bool fake_level_indicator_low_led_is_on(void);
bool fake_level_indicator_medium_led_is_on(void);
bool fake_level_indicator_high_led_is_on(void);

#endif
