#include <stdint.h>

#include "hal_time.h"


void hal_time_delay_ms(
    uint32_t milliseconds
)
{
    /*
     * Avoid reading the time source when no delay was
     * requested.
     */
    if (milliseconds == 0UL)
    {
        return;
    }

    const uint32_t start_time =
        hal_time_millis();

    /*
     * Unsigned subtraction keeps the elapsed-time
     * comparison correct when the 32-bit millisecond
     * counter wraps from UINT32_MAX to zero.
     *
     * The function intentionally remains a blocking
     * busy wait. It does not disable interrupts, so the
     * active timer backend can continue advancing the
     * millisecond counter.
     */
    while (
        (uint32_t)(
            hal_time_millis() -
            start_time
        ) < milliseconds
    )
    {
        /*
         * Busy wait.
         */
    }
}
