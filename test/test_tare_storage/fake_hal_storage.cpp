#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fake_hal_storage.h"
#include "hal_storage.h"


static uint8_t storage_bytes[
    FAKE_HAL_STORAGE_MAX_CAPACITY
];

static size_t storage_capacity =
    FAKE_HAL_STORAGE_MAX_CAPACITY;

static size_t read_calls = 0U;
static size_t write_calls = 0U;

static size_t failed_read_call = 0U;
static size_t failed_write_call = 0U;

static bool discard_successful_writes = false;
static bool invalid_access_detected = false;

static size_t read_addresses[
    FAKE_HAL_STORAGE_MAX_CALLS
];

static size_t read_lengths[
    FAKE_HAL_STORAGE_MAX_CALLS
];

static size_t write_addresses[
    FAKE_HAL_STORAGE_MAX_CALLS
];

static size_t write_lengths[
    FAKE_HAL_STORAGE_MAX_CALLS
];

static bool read_override_enabled = false;
static size_t read_override_call = 0U;
static size_t read_override_length = 0U;

static uint8_t read_override_bytes[
    FAKE_HAL_STORAGE_MAX_CAPACITY
];


static bool range_is_valid(
    size_t address,
    size_t length
)
{
    if (address > storage_capacity)
    {
        return false;
    }

    return length <= (storage_capacity - address);
}


static void record_read_call(
    size_t address,
    size_t length
)
{
    if (read_calls <=
        FAKE_HAL_STORAGE_MAX_CALLS)
    {
        const size_t index = read_calls - 1U;

        read_addresses[index] = address;
        read_lengths[index] = length;
    }
}


static void record_write_call(
    size_t address,
    size_t length
)
{
    if (write_calls <=
        FAKE_HAL_STORAGE_MAX_CALLS)
    {
        const size_t index = write_calls - 1U;

        write_addresses[index] = address;
        write_lengths[index] = length;
    }
}


void fake_hal_storage_reset(void)
{
    memset(
        storage_bytes,
        0xFF,
        sizeof(storage_bytes)
    );

    storage_capacity =
        FAKE_HAL_STORAGE_MAX_CAPACITY;

    read_calls = 0U;
    write_calls = 0U;

    failed_read_call = 0U;
    failed_write_call = 0U;

    discard_successful_writes = false;
    invalid_access_detected = false;

    memset(
        read_addresses,
        0,
        sizeof(read_addresses)
    );

    memset(
        read_lengths,
        0,
        sizeof(read_lengths)
    );

    memset(
        write_addresses,
        0,
        sizeof(write_addresses)
    );

    memset(
        write_lengths,
        0,
        sizeof(write_lengths)
    );

    read_override_enabled = false;
    read_override_call = 0U;
    read_override_length = 0U;

    memset(
        read_override_bytes,
        0,
        sizeof(read_override_bytes)
    );
}


void fake_hal_storage_set_capacity(
    size_t capacity
)
{
    if (capacity >
        FAKE_HAL_STORAGE_MAX_CAPACITY)
    {
        storage_capacity =
            FAKE_HAL_STORAGE_MAX_CAPACITY;

        invalid_access_detected = true;
        return;
    }

    storage_capacity = capacity;
}


void fake_hal_storage_fill(
    uint8_t value
)
{
    memset(
        storage_bytes,
        value,
        sizeof(storage_bytes)
    );
}


bool fake_hal_storage_preload(
    size_t address,
    const uint8_t *source,
    size_t length
)
{
    if ((source == nullptr) &&
        (length > 0U))
    {
        return false;
    }

    if (!range_is_valid(address, length))
    {
        return false;
    }

    if (length > 0U)
    {
        memcpy(
            &storage_bytes[address],
            source,
            length
        );
    }

    return true;
}


bool fake_hal_storage_copy(
    size_t address,
    uint8_t *destination,
    size_t length
)
{
    if ((destination == nullptr) &&
        (length > 0U))
    {
        return false;
    }

    if (!range_is_valid(address, length))
    {
        return false;
    }

    if (length > 0U)
    {
        memcpy(
            destination,
            &storage_bytes[address],
            length
        );
    }

    return true;
}


void fake_hal_storage_fail_read_call(
    size_t call_number
)
{
    failed_read_call = call_number;
}


void fake_hal_storage_fail_write_call(
    size_t call_number
)
{
    failed_write_call = call_number;
}


void fake_hal_storage_discard_writes(
    bool discard_writes
)
{
    discard_successful_writes = discard_writes;
}


bool fake_hal_storage_set_read_override(
    size_t call_number,
    const uint8_t *source,
    size_t length
)
{
    if ((call_number == 0U) ||
        (source == nullptr) ||
        (length > sizeof(read_override_bytes)))
    {
        return false;
    }

    memcpy(
        read_override_bytes,
        source,
        length
    );

    read_override_enabled = true;
    read_override_call = call_number;
    read_override_length = length;

    return true;
}


size_t fake_hal_storage_read_call_count(void)
{
    return read_calls;
}


size_t fake_hal_storage_write_call_count(void)
{
    return write_calls;
}


static size_t call_value_or_zero(
    const size_t *values,
    size_t call_number,
    size_t call_count
)
{
    if ((call_number == 0U) ||
        (call_number > call_count) ||
        (call_number >
         FAKE_HAL_STORAGE_MAX_CALLS))
    {
        return 0U;
    }

    return values[call_number - 1U];
}


size_t fake_hal_storage_read_address(
    size_t call_number
)
{
    return call_value_or_zero(
        read_addresses,
        call_number,
        read_calls
    );
}


size_t fake_hal_storage_read_length(
    size_t call_number
)
{
    return call_value_or_zero(
        read_lengths,
        call_number,
        read_calls
    );
}


size_t fake_hal_storage_write_address(
    size_t call_number
)
{
    return call_value_or_zero(
        write_addresses,
        call_number,
        write_calls
    );
}


size_t fake_hal_storage_write_length(
    size_t call_number
)
{
    return call_value_or_zero(
        write_lengths,
        call_number,
        write_calls
    );
}


bool fake_hal_storage_had_invalid_access(void)
{
    return invalid_access_detected;
}


extern "C" size_t hal_storage_capacity(void)
{
    return storage_capacity;
}


extern "C" bool hal_storage_read(
    size_t address,
    uint8_t *destination,
    size_t length
)
{
    ++read_calls;
    record_read_call(address, length);

    if ((destination == nullptr) &&
        (length > 0U))
    {
        invalid_access_detected = true;
        return false;
    }

    if (!range_is_valid(address, length))
    {
        invalid_access_detected = true;
        return false;
    }

    if (read_calls == failed_read_call)
    {
        return false;
    }

    if (read_override_enabled &&
        (read_calls == read_override_call))
    {
        if (read_override_length != length)
        {
            invalid_access_detected = true;
            return false;
        }

        memcpy(
            destination,
            read_override_bytes,
            length
        );

        return true;
    }

    if (length > 0U)
    {
        memcpy(
            destination,
            &storage_bytes[address],
            length
        );
    }

    return true;
}


extern "C" bool hal_storage_write(
    size_t address,
    const uint8_t *source,
    size_t length
)
{
    ++write_calls;
    record_write_call(address, length);

    if ((source == nullptr) &&
        (length > 0U))
    {
        invalid_access_detected = true;
        return false;
    }

    if (!range_is_valid(address, length))
    {
        invalid_access_detected = true;
        return false;
    }

    if (write_calls == failed_write_call)
    {
        return false;
    }

    if (!discard_successful_writes &&
        (length > 0U))
    {
        memcpy(
            &storage_bytes[address],
            source,
            length
        );
    }

    return true;
}
