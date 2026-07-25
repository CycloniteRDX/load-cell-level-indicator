#include <avr/interrupt.h>
#include <avr/io.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
 * The receive buffer size must be a power of two.
 *
 * The head and tail variables are monotonically
 * incrementing uint8_t sequence counters. Their lower
 * six bits select one of the 64 physical buffer slots.
 *
 * This representation allows all 64 slots to be used:
 *
 *     queued bytes = (uint8_t)(head - tail)
 */
#define HAL_SERIAL_RX_BUFFER_SIZE 64U
#define HAL_SERIAL_RX_BUFFER_MASK \
    (HAL_SERIAL_RX_BUFFER_SIZE - 1U)

#if (HAL_SERIAL_RX_BUFFER_SIZE == 0U)
#error "HAL_SERIAL_RX_BUFFER_SIZE must not be zero"
#endif

#if ((HAL_SERIAL_RX_BUFFER_SIZE & \
      (HAL_SERIAL_RX_BUFFER_SIZE - 1U)) != 0U)
#error "HAL_SERIAL_RX_BUFFER_SIZE must be a power of two"
#endif

#if (HAL_SERIAL_RX_BUFFER_SIZE > 128U)
#error "HAL_SERIAL_RX_BUFFER_SIZE must not exceed 128 bytes"
#endif


/*
 * Bytes received by USART0 are copied here by the
 * receive-complete interrupt.
 *
 * The ISR is the only code that modifies rx_head.
 * Application code is the only code that modifies
 * rx_tail.
 *
 * Both indices are eight-bit values and are therefore
 * read and written atomically by the eight-bit AVR.
 */
static volatile uint8_t rx_buffer[
    HAL_SERIAL_RX_BUFFER_SIZE
];

static volatile uint8_t rx_head = 0U;
static volatile uint8_t rx_tail = 0U;


/*
 * Clears all queued receive bytes.
 *
 * The caller must already have entered a critical
 * section or otherwise prevented the receive ISR from
 * modifying the buffer state.
 */
static void serial_reset_rx_buffer(void)
{
    rx_head = 0U;
    rx_tail = 0U;
}


/*
 * Disables the USART0 receiver, transmitter and all
 * USART0 interrupts.
 *
 * Any bytes already queued in the project-owned receive
 * buffer are discarded.
 */
static void serial_disable(void)
{
    const hal_critical_state_t previous_state =
        hal_critical_enter();

    UCSR0B = 0U;

    serial_reset_rx_buffer();

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
     * Discard bytes queued before reinitialization.
     */
    serial_reset_rx_buffer();

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
     * Enable:
     *
     * RXCIE0 -> receive-complete interrupt
     * RXEN0  -> receiver
     * TXEN0  -> transmitter
     *
     * The previous global interrupt state is restored
     * after the complete configuration is ready.
     */
    UCSR0B =
        _BV(RXCIE0) |
        _BV(RXEN0) |
        _BV(TXEN0);

    hal_critical_exit(previous_state);
}


bool hal_serial_rx_available(void)
{
    /*
     * The ISR modifies only rx_head.
     * Application code modifies only rx_tail.
     *
     * Individual eight-bit reads are atomic on the
     * ATmega328P, so no critical section is required.
     */
    return rx_head != rx_tail;
}


bool hal_serial_read_byte(
    uint8_t *received_byte
)
{
    if (received_byte == NULL)
    {
        return false;
    }

    /*
     * Take local snapshots of the volatile indices.
     *
     * If a new byte arrives after these reads, it can
     * be consumed by a later call. That does not violate
     * the non-blocking read contract.
     */
    const uint8_t tail_snapshot =
        rx_tail;

    const uint8_t head_snapshot =
        rx_head;

    if (head_snapshot == tail_snapshot)
    {
        return false;
    }

    /*
     * The sequence counter may wrap naturally at 255.
     * Its lower bits select the physical array slot.
     */
    *received_byte =
        rx_buffer[
            tail_snapshot &
            HAL_SERIAL_RX_BUFFER_MASK
        ];

    /*
     * Publish consumption only after the output byte
     * has been copied successfully.
     */
    rx_tail =
        (uint8_t)(
            tail_snapshot + 1U
        );

    return true;
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


/*
 * USART0 receive-complete interrupt.
 *
 * Reading UDR0 acknowledges the received byte and makes
 * room for the following hardware byte.
 */
ISR(USART_RX_vect)
{
    const uint8_t received_byte =
        UDR0;

    const uint8_t head_snapshot =
        rx_head;

    const uint8_t tail_snapshot =
        rx_tail;

    /*
     * Since head and tail are uint8_t sequence counters,
     * unsigned subtraction gives the queued-byte count
     * modulo 256.
     *
     * The invariant maintained by this ISR guarantees
     * that the count never exceeds the physical buffer
     * size.
     */
    const uint8_t queued_byte_count =
        (uint8_t)(
            head_snapshot -
            tail_snapshot
        );

    /*
     * Overflow policy:
     *
     * - Preserve every byte already queued.
     * - Discard the newly received byte.
     */
    if (queued_byte_count >=
        HAL_SERIAL_RX_BUFFER_SIZE)
    {
        return;
    }

    rx_buffer[
        head_snapshot &
        HAL_SERIAL_RX_BUFFER_MASK
    ] = received_byte;

    /*
     * Publish the new byte only after it has been stored
     * in the buffer.
     */
    rx_head =
        (uint8_t)(
            head_snapshot + 1U
        );
}
