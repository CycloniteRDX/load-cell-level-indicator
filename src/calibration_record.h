#ifndef CALIBRATION_RECORD_H
#define CALIBRATION_RECORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fixed binary size of one encoded calibration record.
 */
#define CALIBRATION_RECORD_SIZE 12U


/*
 * Returns true when the calibration factor can safely
 * be stored and used for weight conversion.
 */
bool calibration_record_factor_is_valid(
    float calibration_factor
);


/*
 * Encodes one calibration factor into the explicit
 * fixed-size binary record format.
 *
 * Returns false when the factor is invalid, a pointer
 * is null or the supplied buffer is too small.
 */
bool calibration_record_encode(
    float calibration_factor,
    uint8_t *record_bytes,
    size_t record_size
);


/*
 * Decodes and validates one fixed-size calibration
 * record.
 *
 * The output value is modified only after the complete
 * record has passed all validation checks.
 */
bool calibration_record_decode(
    const uint8_t *record_bytes,
    size_t record_size,
    float *calibration_factor
);

#ifdef __cplusplus
}
#endif

#endif
