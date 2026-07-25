#include <EEPROM.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hal_storage.h"


/*
 * Checks whether the complete requested range belongs
 * to the available storage.
 *
 * The subtraction-based check avoids an overflow that
 * could occur with:
 *
 *     address + length
 */
static bool storage_range_is_valid(
    size_t address,
    size_t length
)
{
    const size_t capacity =
        (size_t)EEPROM.length();

    if (address > capacity)
    {
        return false;
    }

    if (length > (capacity - address))
    {
        return false;
    }

    return true;
}


extern "C" size_t hal_storage_capacity(void)
{
    return (size_t)EEPROM.length();
}


extern "C" bool hal_storage_read(
    size_t address,
    uint8_t *destination,
    size_t length
)
{
    /*
     * A null pointer is only invalid when bytes must
     * actually be transferred.
     */
    if ((destination == nullptr) &&
        (length > 0U))
    {
        return false;
    }

    if (!storage_range_is_valid(
            address,
            length))
    {
        return false;
    }

    for (size_t index = 0U;
         index < length;
         ++index)
    {
        destination[index] =
            EEPROM.read(
                (int)(address + index)
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
    /*
     * A null pointer is only invalid when bytes must
     * actually be transferred.
     */
    if ((source == nullptr) &&
        (length > 0U))
    {
        return false;
    }

    if (!storage_range_is_valid(
            address,
            length))
    {
        return false;
    }

    for (size_t index = 0U;
         index < length;
         ++index)
    {
        /*
         * EEPROM.update() only performs a physical
         * EEPROM write when the byte has changed.
         *
         * This reduces unnecessary EEPROM wear.
         */
        EEPROM.update(
            (int)(address + index),
            source[index]
        );
    }

    return true;
}