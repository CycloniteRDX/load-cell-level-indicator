#ifndef SCALE_H
#define SCALE_H

#include <stdbool.h>

/*
 * Initializes the load-cell measurement system.
 *
 * Returns true when the HX711 is detected and ready.
 */
bool scale_init(void);


/*
 * Sets the current load as the zero reference.
 */
void scale_tare(void);


/*
 * Attempts to obtain a new weight measurement.
 *
 * Returns true when a new measurement was available.
 * The result is written to weight_grams.
 */
bool scale_read_weight(float *weight_grams);


/*
 * Returns the current tare offset in raw ADC counts.
 *
 * This is mainly useful for diagnostics.
 */
long scale_get_offset(void);


/*
 * Returns the calibration factor currently in use.
 */
float scale_get_calibration_factor(void);

#endif