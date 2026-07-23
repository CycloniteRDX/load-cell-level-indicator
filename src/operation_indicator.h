#ifndef OPERATION_INDICATOR_H
#define OPERATION_INDICATOR_H


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
    OPERATION_INDICATOR_CALIBRATION_MASS

} operation_indicator_mode_t;


/*
 * Initializes the internal operation-indicator state.
 *
 * indicator_leds_init() must have been called first.
 */
void operation_indicator_init(void);


/*
 * Selects an operation indication mode and applies
 * its initial LED state immediately.
 */
void operation_indicator_set_mode(
    operation_indicator_mode_t mode
);


/*
 * Updates non-blocking blinking patterns.
 *
 * This function must be called repeatedly from
 * the main application loop.
 */
void operation_indicator_update(void);


/*
 * Releases the LEDs and turns all of them off.
 *
 * The level indicator may take control afterwards.
 */
void operation_indicator_clear(void);


#endif