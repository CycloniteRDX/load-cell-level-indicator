#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __AVR__
#include <avr/pgmspace.h>

typedef PGM_P console_progmem_string_t;

#define CONSOLE_PROGMEM(text_literal) \
    PSTR(text_literal)
#else
typedef const char *console_progmem_string_t;

#define CONSOLE_PROGMEM(text_literal) \
    (text_literal)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initializes the console through the active serial
 * backend.
 */
void console_init(
    uint32_t baud_rate
);


/*
 * Returns true when at least one input character can
 * be read without blocking.
 */
bool console_input_available(void);


/*
 * Reads exactly one input character.
 *
 * A failed operation does not modify the caller's
 * output variable.
 */
bool console_read_char(
    char *character
);


/*
 * Consumes every byte currently waiting in the input
 * buffer.
 */
void console_discard_input(void);


/*
 * Emits a CRLF line ending.
 */
void console_newline(void);


/*
 * Prints an ordinary null-terminated C string from
 * data memory.
 */
void console_print(
    const char *text
);


/*
 * Prints an ordinary C string followed by CRLF.
 */
void console_println(
    const char *text
);


/*
 * Prints a null-terminated string stored in program
 * memory on AVR.
 *
 * In native builds, program-memory strings are ordinary
 * C strings.
 */
void console_print_progmem(
    console_progmem_string_t text
);


/*
 * Prints a program-memory string followed by CRLF.
 */
void console_println_progmem(
    console_progmem_string_t text
);


/*
 * Prints an unsigned 32-bit integer in decimal notation.
 */
void console_print_uint32(
    uint32_t value
);


/*
 * Prints a signed 32-bit integer in decimal notation.
 */
void console_print_int32(
    int32_t value
);


/*
 * Prints a floating-point value using fixed-point
 * notation.
 *
 * Decimal precision is limited to six places.
 */
void console_print_float(
    float value,
    uint8_t decimal_places
);

#ifdef __cplusplus
}
#endif


/*
 * Convenience macros for fixed application literals.
 *
 * On AVR, the literal is stored in Flash through PSTR().
 * In native builds, it behaves like an ordinary string.
 */
#define CONSOLE_PRINT(text_literal) \
    console_print_progmem(          \
        CONSOLE_PROGMEM(text_literal) \
    )

#define CONSOLE_PRINTLN(text_literal) \
    console_println_progmem(          \
        CONSOLE_PROGMEM(text_literal) \
    )

#endif
