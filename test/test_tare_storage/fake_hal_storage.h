#ifndef FAKE_HAL_STORAGE_H
#define FAKE_HAL_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FAKE_HAL_STORAGE_MAX_CAPACITY 64U
#define FAKE_HAL_STORAGE_MAX_CALLS 8U

void fake_hal_storage_reset(void);

void fake_hal_storage_set_capacity(
    size_t capacity
);

void fake_hal_storage_fill(
    uint8_t value
);

bool fake_hal_storage_preload(
    size_t address,
    const uint8_t *source,
    size_t length
);

bool fake_hal_storage_copy(
    size_t address,
    uint8_t *destination,
    size_t length
);

void fake_hal_storage_fail_read_call(
    size_t call_number
);

void fake_hal_storage_fail_write_call(
    size_t call_number
);

void fake_hal_storage_discard_writes(
    bool discard_writes
);

bool fake_hal_storage_set_read_override(
    size_t call_number,
    const uint8_t *source,
    size_t length
);

size_t fake_hal_storage_read_call_count(void);
size_t fake_hal_storage_write_call_count(void);

size_t fake_hal_storage_read_address(
    size_t call_number
);

size_t fake_hal_storage_read_length(
    size_t call_number
);

size_t fake_hal_storage_write_address(
    size_t call_number
);

size_t fake_hal_storage_write_length(
    size_t call_number
);

bool fake_hal_storage_had_invalid_access(void);

#ifdef __cplusplus
}
#endif

#endif
