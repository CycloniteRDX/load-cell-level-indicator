#ifndef HAL_STORAGE_H
#define HAL_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns the number of addressable bytes provided
 * by the active non-volatile storage backend.
 */
size_t hal_storage_capacity(void);


/*
 * Reads length bytes beginning at address and copies
 * them into destination.
 *
 * Returns true when the complete operation succeeds.
 *
 * A zero-length operation succeeds without accessing
 * the storage backend.
 */
bool hal_storage_read(
    size_t address,
    uint8_t *destination,
    size_t length
);


/*
 * Writes length bytes from source beginning at address.
 *
 * Returns true when the complete operation succeeds.
 *
 * A zero-length operation succeeds without accessing
 * the storage backend.
 */
bool hal_storage_write(
    size_t address,
    const uint8_t *source,
    size_t length
);

#ifdef __cplusplus
}
#endif

#endif