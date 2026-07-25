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
 * Waits until any CPU-initiated Flash programming
 * operation has completed.
 *
 * The current application does not perform Flash
 * self-programming, but the hardware programming
 * sequence requires this state to be checked.
 */
static void storage_wait_until_flash_ready(void)
{
    while ((SPMCSR & _BV(SPMEN)) != 0U)
    {
        /*
         * Busy wait with global interrupts unchanged.
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


/*
 * Starts a physical EEPROM erase-and-write operation.
 *
 * The caller must:
 *
 * - Ensure EEPROM and Flash programming are idle.
 * - Prepare EEAR and EEDR.
 * - Select the required programming mode.
 * - Disable interrupts before calling this function.
 *
 * Inline assembly guarantees that EEPE is set within
 * the required four CPU cycles after EEMPE.
 */
static void storage_start_write(void)
{
    __asm__ __volatile__(
        "sbi %[eecr], %[eempe]" "\n\t"
        "sbi %[eecr], %[eepe]"
        :
        : [eecr] "I" (_SFR_IO_ADDR(EECR)),
          [eempe] "I" (EEMPE),
          [eepe] "I" (EEPE)
        : "memory"
    );
}


/*
 * Physically writes one EEPROM byte.
 *
 * The complete programming operation is synchronous,
 * but interrupts are disabled only for the short
 * register-programming sequence.
 */
static void storage_write_byte(
    size_t address,
    uint8_t value
)
{
    /*
     * Wait with interrupts enabled.
     */
    storage_wait_until_ready();
    storage_wait_until_flash_ready();

    /*
     * Protect the address, data and timed write-start
     * sequence from interruption.
     */
    const hal_critical_state_t previous_state =
        hal_critical_enter();

    EEAR = (uint16_t)address;
    EEDR = value;

    /*
     * Select atomic erase-and-write mode:
     *
     *     EEPM1 = 0
     *     EEPM0 = 0
     */
    EECR &=
        (uint8_t)~(
            _BV(EEPM1) |
            _BV(EEPM0)
        );

    /*
     * EEMPE and EEPE must be set in the required timed
     * sequence while interrupts remain disabled.
     */
    storage_start_write();

    /*
     * The physical EEPROM programming operation
     * continues after interrupts are restored.
     */
    hal_critical_exit(previous_state);

    /*
     * Keep the public HAL synchronous: when this
     * function returns, the byte is fully programmed.
     */
    storage_wait_until_ready();
}


/*
 * Applies EEPROM.update()-style behaviour.
 *
 * A physical EEPROM write is skipped when the existing
 * byte already equals the requested value.
 */
static void storage_update_byte(
    size_t address,
    uint8_t value
)
{
    const uint8_t current_value =
        storage_read_byte(address);

    if (current_value == value)
    {
        return;
    }

    storage_write_byte(
        address,
        value
    );
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


bool hal_storage_write(
    size_t address,
    const uint8_t *source,
    size_t length
)
{
    /*
     * A null source is only invalid when one or more
     * bytes must actually be transferred.
     */
    if ((source == NULL) &&
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
     * Each byte is compared before deciding whether a
     * physical EEPROM programming cycle is necessary.
     */
    for (size_t index = 0U;
         index < length;
         ++index)
    {
        storage_update_byte(
            address + index,
            source[index]
        );
    }

    return true;
}