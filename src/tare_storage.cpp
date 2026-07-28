#include <stddef.h>
#include <stdint.h>

#include "hal_storage.h"
#include "storage_layout.h"
#include "tare_record.h"
#include "tare_storage.h"


/*
 * Only the four magic bytes need to be invalidated
 * when clearing a stored tare record.
 */
static const size_t TARE_MAGIC_SIZE =
    4U;


static bool storage_has_enough_space(void)
{
    const size_t capacity =
        hal_storage_capacity();

    if (TARE_STORAGE_ADDRESS > capacity)
    {
        return false;
    }

    return
        TARE_RECORD_SIZE <=
        (capacity - TARE_STORAGE_ADDRESS);
}


bool tare_storage_load(
    int32_t *tare_offset
)
{
    if (tare_offset == nullptr)
    {
        return false;
    }

    if (!storage_has_enough_space())
    {
        return false;
    }

    uint8_t record_bytes[TARE_RECORD_SIZE] = {};

    if (!hal_storage_read(
            TARE_STORAGE_ADDRESS,
            record_bytes,
            TARE_RECORD_SIZE))
    {
        return false;
    }

    int32_t decoded_offset = 0;

    if (!tare_record_decode(
            record_bytes,
            TARE_RECORD_SIZE,
            &decoded_offset))
    {
        return false;
    }

    *tare_offset = decoded_offset;

    return true;
}


bool tare_storage_save(
    int32_t tare_offset
)
{
    if (!storage_has_enough_space())
    {
        return false;
    }

    uint8_t record_bytes[TARE_RECORD_SIZE] = {};

    if (!tare_record_encode(
            tare_offset,
            record_bytes,
            TARE_RECORD_SIZE))
    {
        return false;
    }

    if (!hal_storage_write(
            TARE_STORAGE_ADDRESS,
            record_bytes,
            TARE_RECORD_SIZE))
    {
        return false;
    }

    uint8_t verification_bytes[TARE_RECORD_SIZE] = {};

    if (!hal_storage_read(
            TARE_STORAGE_ADDRESS,
            verification_bytes,
            TARE_RECORD_SIZE))
    {
        return false;
    }

    int32_t verified_offset = 0;

    if (!tare_record_decode(
            verification_bytes,
            TARE_RECORD_SIZE,
            &verified_offset))
    {
        return false;
    }

    if (verified_offset != tare_offset)
    {
        return false;
    }

    return true;
}


bool tare_storage_clear(void)
{
    if (!storage_has_enough_space())
    {
        return false;
    }

    const uint8_t invalid_magic[TARE_MAGIC_SIZE] = {};

    if (!hal_storage_write(
            TARE_STORAGE_ADDRESS,
            invalid_magic,
            TARE_MAGIC_SIZE))
    {
        return false;
    }

    uint8_t verification_magic[TARE_MAGIC_SIZE] = {};

    if (!hal_storage_read(
            TARE_STORAGE_ADDRESS,
            verification_magic,
            TARE_MAGIC_SIZE))
    {
        return false;
    }

    for (size_t index = 0U;
         index < TARE_MAGIC_SIZE;
         ++index)
    {
        if (verification_magic[index] != 0U)
        {
            return false;
        }
    }

    return true;
}
