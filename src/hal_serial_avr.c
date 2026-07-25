#include <avr/io.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "hal_critical.h"
#include "hal_serial.h"


#ifndef F_CPU
#error "F_CPU must be defined"
#endif

#if F_CPU != 16000000UL
#error "hal_serial_avr.c currently requires F_CPU = 16 MHz"
#endif


/*
 * USART0 operates in asynchronous double-speed mode.
 *
 * In this mode:
 *
 *     baud = F_CPU / (8 * (UBRR0 + 1))
 */
#define HAL_SERIAL_ASYNC_DIVISOR 8UL

/*
 * UBRR0 is a 12-bit register on the ATmega328P.
 */
#define HAL_SERIAL_UBRR_MAX 0x0FFFU


/*
 * Disables the USART0 receiver, transmitter and all
 * USART0 interrupts.
 *
 * The remaining USART register configuration may stay
 * unchanged because no communication can occur while
 * RXEN0 and TXEN0 are clear.
 */
static void serial_disable(void)
{
    const hal_critical_state_t previous_state =
        hal_critical_enter();

    UCSR0B = 0U;

    hal_critical_exit(previous_state);
}


/*
 * Calculates the rounded UBRR value for asynchronous
 * double-speed operation.
 *
 * The result is:
 *
 *     round(F_CPU / (8 * baud_rate)) - 1
 *
 * Returns false when:
 *
 * - The output pointer is null.
 * - The baud rate is zero.
 * - An intermediate uint32_t operation would overflow.
 * - The requested baud rate cannot be represented.
 * - The result does not fit inside the 12-bit register.
 */
static bool serial_calculate_ubrr(
    uint32_t baud_rate,
    uint16_t *ubrr_value
)
{
    if (ubrr_value == NULL)
    {
        return false;
    }

    if (baud_rate == 0UL)
    {
        return false;
    }

    /*
     * Protect the multiplication:
     *
     *     8 * baud_rate
     */
    if (baud_rate >
        (UINT32_MAX / HAL_SERIAL_ASYNC_DIVISOR))
    {
        return false;
    }

    const uint32_t denominator =
        HAL_SERIAL_ASYNC_DIVISOR *
        baud_rate;

    const uint32_t rounding_offset =
        denominator / 2UL;

    /*
     * Protect the rounded numerator:
     *
     *     F_CPU + denominator / 2
     */
    if (rounding_offset >
        (UINT32_MAX - (uint32_t)F_CPU))
    {
        return false;
    }

    const uint32_t rounded_divisor =
        (
            (uint32_t)F_CPU +
            rounding_offset
        )
        /
        denominator;

    /*
     * A zero divisor result would require a negative
     * UBRR value after subtracting one.
     */
    if (rounded_divisor == 0UL)
    {
        return false;
    }

    const uint32_t calculated_ubrr =
        rounded_divisor - 1UL;

    if (calculated_ubrr >
        HAL_SERIAL_UBRR_MAX)
    {
        return false;
    }

    *ubrr_value =
        (uint16_t)calculated_ubrr;

    return true;
}


void hal_serial_init(
    uint32_t baud_rate
)
{
    uint16_t ubrr_value = 0U;

    /*
     * A zero or unsupported baud rate leaves USART0
     * disabled instead of risking division by zero or
     * configuring an invalid baud rate.
     */
    if (!serial_calculate_ubrr(
            baud_rate,
            &ubrr_value))
    {
        serial_disable();
        return;
    }

    const hal_critical_state_t previous_state =
        hal_critical_enter();

    /*
     * Disable the USART receiver, transmitter and
     * interrupts while changing its configuration.
     */
    UCSR0B = 0U;

    /*
     * Enable asynchronous double-speed mode.
     *
     * U2X0 = 1
     * MPCM0 = 0
     */
    UCSR0A = _BV(U2X0);

    /*
     * Load the 12-bit baud-rate divisor.
     */
    UBRR0H =
        (uint8_t)(
            ubrr_value >> 8U
        );

    UBRR0L =
        (uint8_t)ubrr_value;

    /*
     * Configure asynchronous 8N1:
     *
     * UMSEL01:0 = 00 -> asynchronous USART
     * UPM01:0   = 00 -> no parity
     * USBS0     = 0  -> one stop bit
     * UCSZ01:0  = 11 -> eight data bits
     *
     * UCSZ02 remains zero in UCSR0B.
     */
    UCSR0C =
        _BV(UCSZ01) |
        _BV(UCSZ00);

    /*
     * Enable the receiver and transmitter.
     *
     * The receive-complete interrupt remains disabled
     * until the receive ring buffer is implemented.
     */
    UCSR0B =
        _BV(RXEN0) |
        _BV(TXEN0);

    hal_critical_exit(previous_state);
}


void hal_serial_write_byte(
    uint8_t transmitted_byte
)
{
    /*
     * Wait until the USART transmit data register can
     * accept another byte.
     *
     * Global interrupts remain in their existing state
     * while waiting.
     */
    while ((UCSR0A & _BV(UDRE0)) == 0U)
    {
        /*
         * Busy wait.
         */
    }

    /*
     * Writing UDR0 starts transmission of this byte
     * when the hardware shift register is available.
     */
    UDR0 = transmitted_byte;
}