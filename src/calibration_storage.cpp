#include <stddef.h>
#include <stdint.h>

#include "calibration_record.h"
#include "calibration_storage.h"
#include "hal_storage.h"
#include "storage_layout.h"


/*
 * Only the four magic bytes need to be invalidated
 * when clearing a stored calibration record.
 */
static const size_t CALIBRATION_MAGIC_SIZE = 4U;


static bool record_is_absent(
    const uint8_t *record_bytes
)
{
    bool magic_is_erased = true;
    bool magic_is_cleared = true;

    for (size_t index = 0U;
         index < CALIBRATION_MAGIC_SIZE;
         ++index)
    {
        if (record_bytes[index] != 0xFFU)
        {
            magic_is_erased = false;
        }

        if (record_bytes[index] != 0x00U)
        {
            magic_is_cleared = false;
        }
    }

    return magic_is_erased || magic_is_cleared;
}


static bool storage_has_enough_space(void)
{
    const size_t capacity =
        hal_storage_capacity();

    if (CALIBRATION_STORAGE_ADDRESS > capacity)
    {
        return false;
    }

    return
        CALIBRATION_RECORD_SIZE <=
        (capacity - CALIBRATION_STORAGE_ADDRESS);
}


storage_load_status_t calibration_storage_load(
    float *calibration_factor
)
{
    if (calibration_factor == nullptr)
    {
        return STORAGE_LOAD_ACCESS_ERROR;
    }

    if (!storage_has_enough_space())
    {
        return STORAGE_LOAD_ACCESS_ERROR;
    }

    uint8_t record_bytes[CALIBRATION_RECORD_SIZE] = {};

    if (!hal_storage_read(
            CALIBRATION_STORAGE_ADDRESS,
            record_bytes,
            CALIBRATION_RECORD_SIZE))
    {
        return STORAGE_LOAD_ACCESS_ERROR;
    }

    if (record_is_absent(record_bytes))
    {
        return STORAGE_LOAD_ABSENT;
    }

    float decoded_factor = 0.0F;

    if (!calibration_record_decode(
            record_bytes,
            CALIBRATION_RECORD_SIZE,
            &decoded_factor))
    {
        return STORAGE_LOAD_INVALID;
    }

    *calibration_factor = decoded_factor;

    return STORAGE_LOAD_VALID;
}


bool calibration_storage_save(
    float calibration_factor
)
{
    if (!storage_has_enough_space())
    {
        return false;
    }

    uint8_t record_bytes[CALIBRATION_RECORD_SIZE] = {};

    if (!calibration_record_encode(
            calibration_factor,
            record_bytes,
            CALIBRATION_RECORD_SIZE))
    {
        return false;
    }

    if (!hal_storage_write(
            CALIBRATION_STORAGE_ADDRESS,
            record_bytes,
            CALIBRATION_RECORD_SIZE))
    {
        return false;
    }

    uint8_t verification_bytes[CALIBRATION_RECORD_SIZE] = {};

    if (!hal_storage_read(
            CALIBRATION_STORAGE_ADDRESS,
            verification_bytes,
            CALIBRATION_RECORD_SIZE))
    {
        return false;
    }

    float verified_factor = 0.0F;

    if (!calibration_record_decode(
            verification_bytes,
            CALIBRATION_RECORD_SIZE,
            &verified_factor))
    {
        return false;
    }

    if (verified_factor != calibration_factor)
    {
        return false;
    }

    return true;
}


bool calibration_storage_clear(void)
{
    if (!storage_has_enough_space())
    {
        return false;
    }

    const uint8_t invalid_magic[CALIBRATION_MAGIC_SIZE] = {};

    if (!hal_storage_write(
            CALIBRATION_STORAGE_ADDRESS,
            invalid_magic,
            CALIBRATION_MAGIC_SIZE))
    {
        return false;
    }

    uint8_t verification_magic[CALIBRATION_MAGIC_SIZE] = {};

    if (!hal_storage_read(
            CALIBRATION_STORAGE_ADDRESS,
            verification_magic,
            CALIBRATION_MAGIC_SIZE))
    {
        return false;
    }

    for (size_t index = 0U;
         index < CALIBRATION_MAGIC_SIZE;
         ++index)
    {
        if (verification_magic[index] != 0U)
        {
            return false;
        }
    }

    return true;
}
