#ifndef CALIBRATION_STORAGE_H
#define CALIBRATION_STORAGE_H

#include <stdbool.h>


/*
 * Loads a valid calibration factor from non-volatile memory.
 *
 * Returns true when a valid record was found.
 * The stored factor is written to calibration_factor.
 */
bool calibration_storage_load(
    float *calibration_factor
);


/*
 * Saves a calibration factor in non-volatile memory.
 *
 * Returns true when the factor was written and
 * successfully verified.
 */
bool calibration_storage_save(
    float calibration_factor
);


/*
 * Invalidates the stored calibration record.
 *
 * The currently active calibration factor is not changed.
 */
bool calibration_storage_clear(void);


#endif