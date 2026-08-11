#ifndef TARE_STORAGE_H
#define TARE_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "storage_load_status.h"


/*
 * Loads a valid tare offset from non-volatile memory.
 *
 * Distinguishes a valid record, absent storage, invalid
 * record bytes and a storage access error.
 *
 * The output is modified only for STORAGE_LOAD_VALID.
 */
storage_load_status_t tare_storage_load(
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
