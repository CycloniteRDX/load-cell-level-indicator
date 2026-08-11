#include <stdbool.h>
#include <stddef.h>

#include "watchdog_validation.h"


void watchdog_validation_init(
    watchdog_validation_t *validation
)
{
    if (validation == NULL)
    {
        return;
    }

    validation->trigger_armed = false;
}


bool watchdog_validation_should_stall(
    watchdog_validation_t *validation,
    bool tare_button_released,
    bool calibration_button_released
)
{
    if (validation == NULL)
    {
        return false;
    }

    if (!validation->trigger_armed)
    {
        if (tare_button_released &&
            calibration_button_released)
        {
            validation->trigger_armed = true;
        }

        return false;
    }

    return (
        (!tare_button_released) &&
        (!calibration_button_released)
    );
}
