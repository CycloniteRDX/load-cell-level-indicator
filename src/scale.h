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


typedef enum
{
    SCALE_READ_NO_DATA,
    SCALE_READ_VALUE,
    SCALE_READ_ERROR
} scale_read_status_t;


/*
 * Values produced from one successful ADC conversion.
 *
 * Keeping the raw, net and calibrated representations
 * together prevents callers from accidentally combining
 * values obtained from different conversions.
 */
typedef struct
{
    int32_t raw_counts;
    int32_t net_counts;
    float weight_grams;
} scale_measurement_t;


/*
 * Initializes the load-cell measurement system.
 *
 * This function only configures the selected ADC. It does not
 * wait for the first conversion to become ready.
 *
 * Returns true when the device was configured.
 */
bool scale_init(void);


/*
 * Returns true when the selected ADC has a conversion ready.
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
 * ready ADC conversion and returns its current state.
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
 * Performs one bounded ADC power-down/power-up cycle.
 *
 * Any active sample collection is cancelled before the
 * cycle begins. The current tare offset and calibration
 * factor are preserved.
 *
 * Returns false when either driver operation fails.
 */
bool scale_recover(void);


/*
 * Attempts to obtain exactly one new scale measurement.
 *
 * Returns immediately with SCALE_READ_NO_DATA when no
 * conversion is ready. Raw counts, net counts and grams
 * are all derived from the same conversion. The output is
 * written only when SCALE_READ_VALUE is returned. A driver
 * read failure is reported separately as SCALE_READ_ERROR.
 */
scale_read_status_t scale_try_read_measurement(
    scale_measurement_t *measurement
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
