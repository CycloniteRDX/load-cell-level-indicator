#ifndef SCALE_H
#define SCALE_H

#include <stdbool.h>
#include <stdint.h>


typedef enum
{
    SCALE_SAMPLE_COLLECTION_IDLE,
    SCALE_SAMPLE_COLLECTION_IN_PROGRESS,
    SCALE_SAMPLE_COLLECTION_COMPLETE,
    SCALE_SAMPLE_COLLECTION_ERROR
} scale_sample_collection_status_t;


/*
 * Initializes the load-cell measurement system.
 *
 * This function only configures the HX711. It does not
 * wait for the first conversion to become ready.
 *
 * Returns true when the device was configured.
 */
bool scale_init(void);


/*
 * Returns true when the HX711 has a conversion ready.
 */
bool scale_is_ready(void);


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
 * Starts an incremental raw-sample collection.
 *
 * Returns false when sample_count is zero or the
 * previous collection has not returned to idle.
 */
bool scale_start_sample_collection(
    uint8_t sample_count
);


/*
 * Advances an incremental collection by at most one
 * ready HX711 conversion and returns its current state.
 */
scale_sample_collection_status_t
scale_update_sample_collection(void);


/*
 * Copies the completed integer average and returns the
 * collector to idle.
 *
 * Returns false when average_raw is null or no complete
 * result is available.
 */
bool scale_take_sample_average(
    int32_t *average_raw
);


/*
 * Discards any partial, completed or failed collection
 * and returns the collector to idle.
 */
void scale_cancel_sample_collection(void);


/*
 * Attempts to obtain exactly one new weight measurement.
 *
 * Returns immediately when no conversion is ready.
 * The result is written only when a read succeeds.
 */
bool scale_try_read_weight(float *weight_grams);


/*
 * Returns the current tare offset in raw ADC counts.
 */
int32_t scale_get_offset(void);


/*
 * Returns the calibration factor currently in use.
 */
float scale_get_calibration_factor(void);


#endif
