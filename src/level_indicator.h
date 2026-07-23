#ifndef LEVEL_INDICATOR_H
#define LEVEL_INDICATOR_H

/*
 * Configures the LED pins and leaves the indicator
 * in the unknown state.
 */
void level_indicator_init(void);

/*
 * Clears the current level and turns off all LEDs.
 */
void level_indicator_reset(void);

/*
 * Updates the current level using the measured weight
 * and the configured hysteresis.
 */
void level_indicator_update(float weight_grams);

/*
 * Updates time-dependent visual effects of the
 * current level.
 *
 * This must be called repeatedly from the application
 * loop so that VERY_LOW can blink without blocking.
 */
void level_indicator_update_visual(void);

/*
 * Returns a textual representation of the current level.
 *
 * Possible results:
 * "UNKNOWN", "VERY_LOW", "LOW", "MEDIUM" or "HIGH".
 */
const char *level_indicator_get_state_name(void);

#endif