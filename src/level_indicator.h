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
 * Returns a textual representation of the current level.
 *
 * Possible results:
 * "UNKNOWN", "VERY_LOW", "LOW", "MEDIUM" or "HIGH".
 */
const char *level_indicator_get_state_name(void);

#endif