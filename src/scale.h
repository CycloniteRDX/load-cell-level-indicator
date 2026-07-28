#ifndef SCALE_H
#define SCALE_H

#include <stdbool.h>
#include <stdint.h>


/*
 * Initializes the load-cell measurement system.
 *
 * Returns true when the HX711 is detected and ready.
 */
bool scale_init(void);


/*
 * Applies a calibration factor at runtime.
 *
 * Negative factors are allowed because the sign
 * depends on the load-cell wiring direction.
 *
 * Returns false when the factor is invalid.
 */
bool scale_set_calibration_factor(
    float calibration_factor
);


/*
 * Applies a previously established tare offset.
 *
 * Every signed 32-bit offset is valid.
 */
void scale_set_offset(
    int32_t tare_offset
);


/*
 * Sets the current load as the zero reference.
 *
 * Returns true when all tare samples were collected
 * and the new offset was applied.
 */
bool scale_tare(void);


/*
 * Attempts to obtain a new weight measurement.
 *
 * Returns true when a new measurement was available.
 * The result is written to weight_grams.
 */
bool scale_read_weight(float *weight_grams);


/*
 * Reads averaged raw ADC counts after subtracting
 * the current tare offset.
 *
 * This operation is blocking while the requested
 * samples are collected.
 */
bool scale_read_net_counts(
    float *net_counts,
    uint8_t samples
);


/*
 * Returns the current tare offset in raw ADC counts.
 */
int32_t scale_get_offset(void);


/*
 * Returns the calibration factor currently in use.
 */
float scale_get_calibration_factor(void);


#endif