#ifndef TARE_STORAGE_H
#define TARE_STORAGE_H

#include <stdbool.h>
#include <stdint.h>


/*
 * Loads a valid tare offset from non-volatile memory.
 *
 * Returns true when a valid record was found.
 * The stored offset is written to tare_offset.
 */
bool tare_storage_load(
    int32_t *tare_offset
);


/*
 * Saves a tare offset in non-volatile memory.
 *
 * Returns true when the offset was written and
 * successfully verified.
 */
bool tare_storage_save(
    int32_t tare_offset
);


/*
 * Invalidates the stored tare record.
 *
 * The currently active runtime offset is not changed.
 */
bool tare_storage_clear(void);


#endif
