#include <avr/io.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hal_critical.h"
#include "hal_storage.h"


/*
 * Returns the number of EEPROM bytes provided by the
 * selected AVR microcontroller.
 *
 * E2END is the final valid EEPROM address, so the
 * capacity is one byte greater.
 *
 * For the ATmega328P:
 *
 *     E2END    = 1023
 *     capacity = 1024 bytes
 */
size_t hal_storage_capacity(void)
{
    return (size_t)E2END + 1U;
}


/*
 * Checks whether the complete requested byte range
 * belongs to the available EEPROM.
 *
 * Using subtraction avoids a possible overflow in:
 *
 *     address + length
 */
static bool storage_range_is_valid(
    size_t address,
    size_t length
)
{
    const size_t capacity =
        hal_storage_capacity();

    /*
     * An address exactly equal to capacity is valid
     * only for a zero-length operation.
     */
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


/*
 * Waits until the EEPROM controller has completed any
 * previous physical write operation.
 *
 * Interrupts remain enabled while waiting.
 */
static void storage_wait_until_ready(void)
{
    while ((EECR & _BV(EEPE)) != 0U)
    {
        /*
         * Busy wait.
         *
         * EEPROM writes are infrequent and the public
         * storage HAL is intentionally synchronous.
         */
    }
}


/*
 * Reads one byte directly from the AVR EEPROM.
 */
static uint8_t storage_read_byte(
    size_t address
)
{
    /*
     * An EEPROM read cannot start while a previous
     * EEPROM write is still in progress.
     *
     * This potentially long wait occurs before entering
     * the critical section.
     */
    storage_wait_until_ready();

    /*
     * Keep the address, read trigger and data-register
     * access together.
     *
     * The previous interrupt state is restored exactly
     * by the project critical-section HAL.
     */
    const hal_critical_state_t previous_state =
        hal_critical_enter();

    EEAR = (uint16_t)address;

    /*
     * Setting EERE starts the EEPROM read.
     *
     * The hardware places the selected byte in EEDR.
     */
    EECR |= _BV(EERE);

    const uint8_t value = EEDR;

    hal_critical_exit(previous_state);

    return value;
}


bool hal_storage_read(
    size_t address,
    uint8_t *destination,
    size_t length
)
{
    /*
     * A null destination is only invalid when one or
     * more bytes must actually be transferred.
     */
    if ((destination == NULL) &&
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

    /*
     * A valid zero-length operation reaches this loop
     * and completes without touching EEPROM.
     */
    for (size_t index = 0U;
         index < length;
         ++index)
    {
        destination[index] =
            storage_read_byte(
                address + index
            );
    }

    return true;
}