#include <Arduino.h>

#include "hal_time.h"

extern "C" uint32_t hal_time_millis(void)
{
    return millis();
}

extern "C" void hal_time_delay_us(
    uint16_t microseconds
)
{
    delayMicroseconds(microseconds);
}