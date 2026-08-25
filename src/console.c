#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "hal_serial.h"


#define CONSOLE_MAX_DECIMAL_PLACES 6U

/*
 * Largest finite value that can be converted safely to
 * uint32_t by the fixed-point formatter.
 *
 * This is also the limit traditionally used by the
 * Arduino Print floating-point implementation.
 */
#define CONSOLE_MAX_FORMATTABLE_FLOAT 4294967040.0F


static void console_write_character(
    char character
)
{
    hal_serial_write_byte(
        (uint8_t)character
    );
}


void console_print_uint32(
    uint32_t value
)
{
    /*
     * A 32-bit unsigned integer requires at most ten
     * decimal digits.
     */
    char digits[10];
    uint8_t digit_count = 0U;

    do
    {
        const uint32_t quotient =
            value / 10U;

        const uint32_t remainder =
            value - (quotient * 10U);

        digits[digit_count] =
            (char)('0' + remainder);

        ++digit_count;
        value = quotient;
    }
    while (value > 0U);

    while (digit_count > 0U)
    {
        --digit_count;

        console_write_character(
            digits[digit_count]
        );
    }
}


void console_init(
    uint32_t baud_rate
)
{
    hal_serial_init(baud_rate);
}


bool console_input_available(void)
{
    return hal_serial_rx_available();
}


bool console_read_char(
    char *character
)
{
    if (character == NULL)
    {
        return false;
    }

    uint8_t received_byte = 0U;

    if (!hal_serial_read_byte(
            &received_byte))
    {
        return false;
    }

    *character = (char)received_byte;

    return true;
}


void console_discard_input(void)
{
    uint8_t discarded_byte = 0U;

    while (hal_serial_rx_available())
    {
        if (!hal_serial_read_byte(
                &discarded_byte))
        {
            /*
             * Avoid an infinite loop if a backend
             * reports availability but cannot complete
             * the read.
             */
            break;
        }
    }
}


void console_newline(void)
{
    console_write_character('\r');
    console_write_character('\n');
}


void console_print(
    const char *text
)
{
    if (text == NULL)
    {
        return;
    }

    while (*text != '\0')
    {
        console_write_character(*text);
        ++text;
    }
}


void console_println(
    const char *text
)
{
    console_print(text);
    console_newline();
}


void console_print_progmem(
    console_progmem_string_t text
)
{
    if (text == NULL)
    {
        return;
    }

#ifdef __AVR__
    while (true)
    {
        const char character =
            (char)pgm_read_byte(text);

        if (character == '\0')
        {
            break;
        }

        console_write_character(character);
        ++text;
    }
#else
    console_print(text);
#endif
}


void console_println_progmem(
    console_progmem_string_t text
)
{
    console_print_progmem(text);
    console_newline();
}


void console_print_int32(
    int32_t value
)
{
    uint32_t magnitude = 0U;

    if (value < 0)
    {
        console_write_character('-');

        /*
         * This expression also handles INT32_MIN
         * without attempting to evaluate -INT32_MIN.
         */
        magnitude =
            (uint32_t)(-(value + 1)) + 1U;
    }
    else
    {
        magnitude = (uint32_t)value;
    }

    console_print_uint32(magnitude);
}


void console_print_float(
    float value,
    uint8_t decimal_places
)
{
    if (isnan(value))
    {
        console_print("nan");
        return;
    }

    if (isinf(value))
    {
        if (value < 0.0F)
        {
            console_write_character('-');
        }

        console_print("inf");
        return;
    }

    if ((value > CONSOLE_MAX_FORMATTABLE_FLOAT) ||
        (value < -CONSOLE_MAX_FORMATTABLE_FLOAT))
    {
        console_print("ovf");
        return;
    }

    if (decimal_places >
        CONSOLE_MAX_DECIMAL_PLACES)
    {
        decimal_places =
            CONSOLE_MAX_DECIMAL_PLACES;
    }

    /*
     * Do not print a minus sign for negative zero.
     */
    if (value < 0.0F)
    {
        console_write_character('-');
        value = -value;
    }

    /*
     * Apply decimal rounding before separating the
     * integer and fractional parts.
     */
    float rounding = 0.5F;

    for (uint8_t index = 0U;
         index < decimal_places;
         ++index)
    {
        rounding *= 0.1F;
    }

    value += rounding;

    const uint32_t integer_part =
        (uint32_t)value;

    float fractional_part =
        value - (float)integer_part;

    console_print_uint32(integer_part);

    if (decimal_places == 0U)
    {
        return;
    }

    console_write_character('.');

    for (uint8_t index = 0U;
         index < decimal_places;
         ++index)
    {
        fractional_part *= 10.0F;

        uint8_t digit =
            (uint8_t)fractional_part;

        /*
         * Guard against a floating-point edge case that
         * could otherwise produce a non-decimal byte.
         */
        if (digit > 9U)
        {
            digit = 9U;
        }

        console_write_character(
            (char)('0' + digit)
        );

        fractional_part -= (float)digit;
    }
}
