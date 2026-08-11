#ifndef WATCHDOG_VALIDATION_H
#define WATCHDOG_VALIDATION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * State used only by the dedicated watchdog hardware
 * validation builds.
 *
 * The trigger starts disarmed. Both buttons must first
 * be observed released before a simultaneous press can
 * request the intentional main-loop stall. This makes
 * the test one-shot across a watchdog reset while the
 * user is still holding both buttons.
 */
typedef struct
{
    bool trigger_armed;
} watchdog_validation_t;


void watchdog_validation_init(
    watchdog_validation_t *validation
);


/*
 * Updates the validation trigger from active-low button
 * input states.
 *
 * true means released and false means pressed, matching
 * the project GPIO HAL and internal pull-up wiring.
 *
 * Returns true only after both inputs have first been
 * observed released and are later observed pressed
 * together.
 */
bool watchdog_validation_should_stall(
    watchdog_validation_t *validation,
    bool tare_button_released,
    bool calibration_button_released
);

#ifdef __cplusplus
}
#endif

#endif
