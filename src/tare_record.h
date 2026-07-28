#ifndef TARE_RECORD_H
#define TARE_RECORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fixed binary size of one encoded tare record.
 */
#define TARE_RECORD_SIZE 12U


/*
 * Encodes one signed tare offset into the explicit
 * fixed-size binary record format.
 *
 * Returns false when a pointer is null or the supplied
 * buffer is too small.
 */
bool tare_record_encode(
    int32_t tare_offset,
    uint8_t *record_bytes,
    size_t record_size
);


/*
 * Decodes and validates one fixed-size tare record.
 *
 * The output value is modified only after the complete
 * record has passed all validation checks.
 */
bool tare_record_decode(
    const uint8_t *record_bytes,
    size_t record_size,
    int32_t *tare_offset
);

#ifdef __cplusplus
}
#endif

#endif
