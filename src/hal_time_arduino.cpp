#include <Arduino.h>

#include "hal_time.h"


extern "C" void hal_time_init(void)
{
    /*
     * Arduino initializes Timer0 and its timekeeping
     * facilities before setup() is called.
     *
     * This backend therefore requires no additional
     * initialization.
     */
}


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