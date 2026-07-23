#ifndef OPERATION_INDICATOR_H
#define OPERATION_INDICATOR_H

#include <stdbool.h>


typedef enum
{
    /*
     * The operation indicator does not own the LEDs.
     * The normal level indicator may control them.
     */
    OPERATION_INDICATOR_NONE,

    /*
     * All LEDs remain on while tare samples
     * are being collected.
     */
    OPERATION_INDICATOR_TARE,

    /*
     * The LOW LED blinks while waiting for
     * the empty platform confirmation.
     */
    OPERATION_INDICATOR_CALIBRATION_ZERO,

    /*
     * The MEDIUM LED blinks while waiting for
     * the reference calibration mass.
     */
    OPERATION_INDICATOR_CALIBRATION_MASS,

    /*
     * All LEDs blink temporarily after a
     * successful calibration.
     */
    OPERATION_INDICATOR_SUCCESS,

    /*
     * The HIGH LED blinks temporarily when
     * a calibration error occurs.
     */
    OPERATION_INDICATOR_ERROR

} operation_indicator_mode_t;


/*
 * Initializes the internal operation-indicator state.
 *
 * indicator_leds_init() must have been called first.
 */
void operation_indicator_init(void);


/*
 * Selects a persistent operation mode.
 *
 * Persistent modes remain active until another mode
 * is selected or operation_indicator_clear() is called.
 */
void operation_indicator_set_mode(
    operation_indicator_mode_t mode
);


/*
 * Shows the temporary successful-calibration pattern.
 *
 * When the pattern finishes, the LEDs are released.
 */
void operation_indicator_show_success(void);


/*
 * Shows the temporary calibration-error pattern.
 *
 * When the pattern finishes, the module automatically
 * returns to return_mode.
 */
void operation_indicator_show_error(
    operation_indicator_mode_t return_mode
);


/*
 * Returns true while a finite success or error
 * pattern is being displayed.
 */
bool operation_indicator_is_temporary_active(void);


/*
 * Updates all non-blocking blinking patterns.
 *
 * This function must be called repeatedly from
 * the application loop.
 */
void operation_indicator_update(void);


/*
 * Releases the LEDs and turns all of them off.
 */
void operation_indicator_clear(void);


#endif