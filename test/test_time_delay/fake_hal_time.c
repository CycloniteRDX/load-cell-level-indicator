#include "fake_hal_time.h"

#include <stdint.h>

#include "hal_time.h"


static uint32_t current_time = 0UL;
static uint32_t configured_time_step = 1UL;
static uint32_t configured_reads_per_time_value = 1UL;
static uint32_t reads_at_current_time = 0UL;
static uint32_t read_call_count = 0UL;
static uint32_t last_returned_time = 0UL;


void fake_hal_time_reset(void)
{
    current_time = 0UL;
    configured_time_step = 1UL;
    configured_reads_per_time_value = 1UL;
    reads_at_current_time = 0UL;
    read_call_count = 0UL;
    last_returned_time = 0UL;
}


void fake_hal_time_configure(
    uint32_t initial_time,
    uint32_t time_step,
    uint32_t reads_per_time_value
)
{
    current_time = initial_time;
    configured_time_step = time_step;

    /*
     * A zero value would prevent the fake from having a
     * meaningful read cadence. Treat it as one read per
     * time value so tests can never configure an invalid
     * fake clock accidentally.
     */
    configured_reads_per_time_value =
        (reads_per_time_value == 0UL)
            ? 1UL
            : reads_per_time_value;

    reads_at_current_time = 0UL;
    read_call_count = 0UL;
    last_returned_time = initial_time;
}


uint32_t fake_hal_time_get_read_count(void)
{
    return read_call_count;
}


uint32_t fake_hal_time_get_last_returned_time(void)
{
    return last_returned_time;
}


uint32_t fake_hal_time_get_next_time(void)
{
    return current_time;
}


uint32_t hal_time_millis(void)
{
    last_returned_time = current_time;
    ++read_call_count;
    ++reads_at_current_time;

    if (reads_at_current_time >=
        configured_reads_per_time_value)
    {
        reads_at_current_time = 0UL;

        /*
         * Unsigned addition intentionally wraps modulo
         * 2^32, matching the production millisecond
         * counter.
         */
        current_time += configured_time_step;
    }

    return last_returned_time;
}
