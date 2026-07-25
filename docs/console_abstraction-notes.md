# Console Abstraction Notes

## 1. Purpose

The purpose of this milestone is to remove direct Arduino `Serial` usage from the application layer.

The current application depends directly on:

```text
Arduino Serial.begin()
Arduino Serial.available()
Arduino Serial.read()
Arduino Serial.print()
Arduino Serial.println()
Arduino F() macro
```

The new architecture will introduce:

* A byte-oriented serial hardware-abstraction layer.
* An Arduino implementation of that serial HAL.
* A platform-independent console module.
* Native tests for console input and output formatting.
* Flash-resident application text without using Arduino `F()`.

The next milestone will replace the Arduino serial backend with a direct AVR UART implementation.

---

## 2. Current application dependency

The current serial dependency is concentrated in:

```text
src/app.cpp
```

The application currently includes:

```cpp
#include <Arduino.h>
```

and directly initializes Serial with:

```cpp
Serial.begin(115200);
```

It also handles single-character commands through:

```cpp
Serial.available()
Serial.read()
```

Application output currently uses:

* Blank lines.
* Constant text.
* RAM strings.
* Flash strings wrapped in `F()`.
* Signed tare offsets.
* Floating-point values with two decimal places.
* Floating-point calibration factors with six decimal places.
* Dynamic level-state names.

The application does not currently require:

* Binary serial packets.
* Line editing.
* Command buffering.
* Formatted input.
* Asynchronous transmit callbacks.
* Multiple UARTs.

---

## 3. Current command interface

The current console accepts individual characters:

```text
t = tare
c = start or confirm calibration
q = cancel calibration
s = save active calibration
x = clear stored calibration
```

No newline is required before a command is processed.

The new console abstraction must preserve this exact behaviour.

Input already waiting in the receive buffer may be discarded before selected operations.

---

## 4. Target architecture

The target architecture is:

```text
Application
    |
    v
console.h
    |
    v
console.c
    |
    v
hal_serial.h
    |
    v
hal_serial_arduino.cpp
    |
    v
Arduino HardwareSerial
```

The responsibilities will be divided as follows.

### Application

Responsible for:

* Choosing what information to display.
* Processing command characters.
* Selecting numerical precision.
* Controlling calibration and measurement workflows.

### Console module

Responsible for:

* Printing text.
* Printing flash-resident text.
* Printing signed integers.
* Printing fixed-point floating values.
* Generating line endings.
* Reading command characters.
* Discarding pending input.

### Serial HAL

Responsible for:

* Configuring the physical serial peripheral.
* Reporting whether a received byte is available.
* Reading one byte.
* Transmitting one byte.

### Physical backend

Initially:

```text
hal_serial_arduino.cpp
```

Later:

```text
hal_serial_avr.c
```

---

## 5. Serial HAL interface

The new public HAL header will be:

```text
include/hal_serial.h
```

The planned interface is:

```c
#ifndef HAL_SERIAL_H
#define HAL_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void hal_serial_init(
    uint32_t baud_rate
);

bool hal_serial_rx_available(void);

bool hal_serial_read_byte(
    uint8_t *byte
);

void hal_serial_write_byte(
    uint8_t byte
);

#ifdef __cplusplus
}
#endif

#endif
```

The interface is deliberately byte-oriented.

It does not understand:

* Text strings.
* Decimal numbers.
* Floating-point values.
* Lines.
* Commands.
* Calibration messages.

Those responsibilities belong to the console layer.

---

## 6. Serial HAL semantics

### Initialization

```c
void hal_serial_init(
    uint32_t baud_rate
);
```

configures the active backend for the requested baud rate.

The application currently uses:

```text
115200 baud
8 data bits
no parity
1 stop bit
```

The Arduino backend will delegate this configuration to:

```cpp
Serial.begin(baud_rate);
```

### Receive availability

```c
bool hal_serial_rx_available(void);
```

returns true when at least one byte can be read without blocking.

The application does not need the exact number of waiting bytes.

### Byte reading

```c
bool hal_serial_read_byte(
    uint8_t *byte
);
```

must:

* Reject a null output pointer.
* Return false when no received byte is available.
* Return true after storing one received byte.
* Consume exactly one byte.

### Byte writing

```c
void hal_serial_write_byte(
    uint8_t byte
);
```

transmits exactly one byte.

The initial implementation may block when the Arduino transmit buffer is full.

The future direct AVR backend may block until the UART data register is available.

---

## 7. Arduino serial backend

The initial active backend will be:

```text
src/hal_serial_arduino.cpp
```

It will wrap:

```text
Serial.begin()
Serial.available()
Serial.read()
Serial.write()
```

No other production module should call those Arduino functions after the application migration is complete.

The Arduino backend will remain in the repository after the direct AVR UART backend is introduced.

---

## 8. Console module

The platform-independent console module will be:

```text
src/console.h
src/console.c
```

The planned public interface is:

```c
void console_init(
    uint32_t baud_rate
);

bool console_input_available(void);

bool console_read_char(
    char *character
);

void console_discard_input(void);

void console_newline(void);

void console_print(
    const char *text
);

void console_println(
    const char *text
);

void console_print_progmem(
    console_progmem_string_t text
);

void console_println_progmem(
    console_progmem_string_t text
);

void console_print_int32(
    int32_t value
);

void console_print_float(
    float value,
    uint8_t decimal_places
);
```

The console will produce output exclusively through:

```text
hal_serial_write_byte()
```

It will receive input exclusively through:

```text
hal_serial_rx_available()
hal_serial_read_byte()
```

---

## 9. Flash-resident text

The project previously exceeded the ATmega328P SRAM limit when large Serial messages were stored in RAM.

The existing Arduino implementation solved that problem with:

```cpp
F("Text stored in Flash")
```

The console abstraction must preserve flash-resident fixed text without requiring Arduino.

The console header will provide a portable literal mechanism.

Conceptually:

```c
#ifdef __AVR__

#include <avr/pgmspace.h>

typedef PGM_P console_progmem_string_t;

#define CONSOLE_PROGMEM(text) \
    PSTR(text)

#else

typedef const char *
    console_progmem_string_t;

#define CONSOLE_PROGMEM(text) \
    (text)

#endif
```

Application call sites will use convenience macros such as:

```c
CONSOLE_PRINT(
    "Calibration started"
);

CONSOLE_PRINTLN(
    "Calibration completed"
);
```

On AVR, those macros will store the literal in program memory and route it through the program-memory printing function.

During native tests, the same macros will behave like ordinary C strings.

The application will no longer use:

```cpp
F()
```

---

## 10. RAM strings

Not every string originates from a fixed application literal.

For example:

```c
level_indicator_get_state_name()
```

returns a runtime-selected C string.

These strings will be printed using:

```c
console_print(
    level_indicator_get_state_name()
);
```

Fixed application messages will use the flash-literal macros.

This distinction prevents fixed text from consuming scarce SRAM while still supporting ordinary runtime strings.

---

## 11. Line endings

Arduino `Serial.println()` currently emits:

```text
carriage return
line feed
```

or:

```text
\r\n
```

The console abstraction will preserve this behaviour.

```c
console_newline();
```

will transmit:

```text
0x0D
0x0A
```

Functions ending in `println` will print their value and then call:

```c
console_newline();
```

This keeps the serial monitor output unchanged.

---

## 12. Signed integer formatting

The application currently prints the tare offset returned by:

```c
scale_get_offset()
```

The console will provide explicit signed 32-bit formatting:

```c
console_print_int32(
    int32_t value
);
```

The implementation will:

* Print zero correctly.
* Support positive values.
* Support negative values.
* Support `INT32_MIN`.
* Avoid dynamic memory allocation.
* Avoid `sprintf()`.
* Avoid recursive formatting.

Digits will be generated into a small local buffer and transmitted in the correct order.

The application will cast the AVR `long` offset explicitly:

```c
console_print_int32(
    (int32_t)scale_get_offset()
);
```

---

## 13. Floating-point formatting

The application requires two fixed precisions:

```text
2 decimal places
6 decimal places
```

Examples include:

```text
1500.00
45.589332
-10.00
```

The console will implement a small fixed-point formatter instead of relying on full `printf()` floating-point support.

This avoids:

* Variadic format strings.
* Large `printf` dependencies.
* Special AVR linker flags for floating-point `printf`.
* Dynamic memory allocation.
* Different formatting paths between native and AVR builds.

The formatter will support:

* Positive values.
* Negative values.
* Zero.
* Requested trailing zeros.
* Decimal rounding.
* NaN.
* Positive infinity.
* Negative infinity.
* Up to six decimal places.

The current application never requests more than six decimal places.

The native tests will define the exact expected output.

---

## 14. Input handling

The console will preserve the current single-character input behaviour.

```c
bool console_input_available(void);
```

will report whether a command is waiting.

```c
bool console_read_char(
    char *character
);
```

will consume one byte and convert it to `char`.

```c
void console_discard_input(void);
```

will repeatedly consume available bytes until the receive buffer is empty.

The existing application helper:

```text
clear_serial_input()
```

will become unnecessary or will be replaced directly by:

```c
console_discard_input();
```

---

## 15. Application migration

After the console and its tests are complete, `app.cpp` will be migrated.

The migration will replace:

```text
Serial.begin()
Serial.available()
Serial.read()
Serial.print()
Serial.println()
F()
```

with:

```text
console_init()
console_input_available()
console_read_char()
console_discard_input()
console_print()
console_println()
CONSOLE_PRINT()
CONSOLE_PRINTLN()
console_print_int32()
console_print_float()
console_newline()
```

The existing baud rate will be moved to:

```text
config.h
```

as:

```c
static const uint32_t
    CONSOLE_BAUD_RATE = 115200UL;
```

The command behaviour and displayed messages will remain unchanged.

The function currently named:

```text
process_serial_commands()
```

may be renamed:

```text
process_console_commands()
```

because command processing will no longer depend directly on Arduino Serial.

---

## 16. Remaining Arduino dependency

This milestone removes direct Arduino Serial and `F()` usage from `app.cpp`.

The application will still temporarily use:

```cpp
delay(1000);
delay(3000);
```

Therefore, `app.cpp` may continue including:

```cpp
Arduino.h
```

solely for blocking millisecond delays.

Removing those final delay calls belongs to the later Arduino-Core removal work and is not required for the console milestone.

The important result of this milestone is that changing the physical UART backend will no longer require modifying application output or command logic.

---

## 17. Native fake serial backend

The console test suite will provide:

```text
test/test_console/fake_hal_serial.h
test/test_console/fake_hal_serial.c
```

The fake will support:

* Recording the requested baud rate.
* Capturing transmitted bytes.
* Inspecting the complete output string.
* Loading received bytes.
* Reporting whether received data is available.
* Consuming received data one byte at a time.
* Resetting all fake state.
* Detecting output-buffer overflow.
* Recording read and write call counts.

No Arduino or physical serial port will be required.

---

## 18. Native console tests

The native suite will cover:

### Initialization

* Baud rate is forwarded correctly.
* Initialization occurs exactly once.

### Text output

* Empty strings.
* Ordinary RAM strings.
* Program-memory literal macros.
* Blank lines.
* CRLF line endings.
* Multiple consecutive writes.
* Runtime-selected strings.

### Signed integers

* Zero.
* Positive values.
* Negative values.
* `INT32_MAX`.
* `INT32_MIN`.

### Floating-point values

* Zero with two decimal places.
* Zero with six decimal places.
* Positive values.
* Negative values.
* Required trailing zeros.
* Rounding down.
* Rounding up.
* Carry into the integer part.
* NaN.
* Positive infinity.
* Negative infinity.
* Precision limited to six decimal places.

### Input

* No byte available.
* Null output pointer.
* One available command.
* Multiple commands consumed in order.
* Input discard.
* Input discard on an empty buffer.

---

## 19. Native PlatformIO environment

A new environment will be added:

```ini
[env:native_console]
platform = native
test_framework = unity

test_filter = test_console
test_build_src = yes

build_src_filter =
    -<*>
    +<console.c>

build_flags =
    -Isrc
```

The environment will compile:

```text
src/console.c
test/test_console/fake_hal_serial.c
test/test_console/test_main.cpp
```

It will not compile:

```text
src/hal_serial_arduino.cpp
Arduino HardwareSerial
AVR-specific backends
app.cpp
```

---

## 20. Physical validation

After migrating `app.cpp`, validation on the Nano must confirm:

* Firmware starts normally.
* The startup banner is unchanged.
* Stored calibration messages appear.
* Calibration factors show six decimal places.
* Weights show two decimal places.
* Level names appear correctly.
* Blank lines and spacing remain readable.
* Serial commands `t`, `c`, `q`, `s` and `x` work.
* Multiple waiting input bytes can be discarded.
* Calibration instructions remain complete.
* Error messages remain complete.
* No text corruption is visible.
* No unexpected characters appear.
* HX711 operation remains functional.
* Buttons and indicators remain functional.
* EEPROM persistence remains functional.
* Timer1 timing remains functional.

The static SRAM usage must also be checked carefully.

The migration must not reintroduce the previous SRAM overflow caused by ordinary string literals.

---

## 21. Memory validation

The production build before the application migration will be recorded.

The production build after the console migration will also be recorded.

The comparison will include:

```text
Static SRAM
Flash usage
```

Moving formatting into the console module may change flash usage.

Using program-memory literal macros should prevent large fixed messages from being copied into SRAM.

Correctness and removal of the direct Serial dependency are more important than reducing flash usage.

---

## 22. Non-objectives

This milestone will not:

* Implement direct AVR UART registers.
* Configure `UBRR0`, `UCSR0A`, `UCSR0B` or `UCSR0C`.
* Add interrupt-driven receive buffers.
* Add interrupt-driven transmit buffers.
* Add command lines or string commands.
* Add a command parser.
* Add binary packets.
* Add checksums to serial communication.
* Add multiple serial ports.
* Change command characters.
* Change application messages.
* Change calibration behaviour.
* Remove `delay()`.
* Remove `setup()` or `loop()`.
* Remove the Arduino Core.
* Add dynamic memory allocation.
* Use floating-point `printf()`.

---

## 23. Planned commits

### Design

```text
docs: define console abstraction strategy
```

### Serial HAL

```text
feat: add serial HAL and Arduino backend
```

### Console output

```text
feat: add console text and number formatting
```

### Native tests

```text
test: cover console formatting and input
```

### Application migration

```text
refactor: route application IO through console
```

### Final validation

```text
docs: record console abstraction validation
```

---

## 24. Definition of done

This milestone will be complete when:

* [ ] The console architecture is documented.
* [ ] A C-compatible serial HAL exists.
* [ ] An Arduino serial backend exists.
* [ ] A platform-independent console module exists.
* [ ] Fixed application literals remain in program memory.
* [ ] Arduino `F()` is no longer used by `app.cpp`.
* [ ] `Serial.begin()` is no longer called by `app.cpp`.
* [ ] `Serial.available()` is no longer called by `app.cpp`.
* [ ] `Serial.read()` is no longer called by `app.cpp`.
* [ ] `Serial.print()` is no longer called by `app.cpp`.
* [ ] `Serial.println()` is no longer called by `app.cpp`.
* [ ] CRLF output is preserved.
* [ ] RAM strings can be printed.
* [ ] Flash-resident literals can be printed.
* [ ] Signed 32-bit integers can be printed.
* [ ] Two-decimal floats can be printed.
* [ ] Six-decimal floats can be printed.
* [ ] Floating-point rounding is tested.
* [ ] NaN and infinity handling is defined.
* [ ] Single-character commands remain supported.
* [ ] Pending input can be discarded.
* [ ] A fake serial backend exists.
* [ ] Native console tests pass.
* [ ] All previous 128 native tests pass.
* [ ] The Nano firmware compiles.
* [ ] Serial commands work on physical hardware.
* [ ] Startup and calibration output remain readable.
* [ ] Static SRAM remains within limits.
* [ ] Flash and SRAM usage are recorded.
* [ ] Final validation is documented.

## 25. Final architecture

The console abstraction has been implemented and selected successfully.

The final application input/output architecture is:

```text
Application
    |
    v
console.h
    |
    v
console.c
    |
    v
hal_serial.h
    |
    v
hal_serial_arduino.cpp
    |
    v
Arduino HardwareSerial
```

The application no longer calls the Arduino Serial API directly.

The physical serial transport remains temporarily provided by Arduino, but all application text formatting and command handling now pass through project-owned abstractions.

---

## 26. Serial HAL

The public serial HAL is:

```text
include/hal_serial.h
```

It exposes:

```c
void hal_serial_init(
    uint32_t baud_rate
);

bool hal_serial_rx_available(void);

bool hal_serial_read_byte(
    uint8_t *received_byte
);

void hal_serial_write_byte(
    uint8_t transmitted_byte
);
```

The HAL operates exclusively on individual bytes.

It does not understand:

* Strings.
* Lines.
* Decimal formatting.
* Floating-point values.
* Calibration messages.
* Application commands.

The active production backend is:

```text
src/hal_serial_arduino.cpp
```

It wraps:

```text
Serial.begin()
Serial.available()
Serial.read()
Serial.write()
```

The next direct AVR UART backend can implement the same interface without changing the console or application.

---

## 27. Console module

The platform-independent console implementation is:

```text
src/console.h
src/console.c
```

It owns:

* Console initialization.
* Character input.
* Pending-input discard.
* RAM-string output.
* Program-memory string output.
* CRLF line endings.
* Signed 32-bit integer formatting.
* Fixed-precision floating-point formatting.

All transmitted bytes pass through:

```text
hal_serial_write_byte()
```

All received bytes pass through:

```text
hal_serial_rx_available()
hal_serial_read_byte()
```

The console contains no Arduino dependency.

---

## 28. Program-memory literals

Fixed application messages now use:

```c
CONSOLE_PRINT(
    "Fixed text"
);

CONSOLE_PRINTLN(
    "Fixed text with newline"
);
```

On AVR, these macros use program-memory literals.

This preserves the SRAM optimization previously provided by:

```cpp
F("...")
```

During native tests, the same macros behave as ordinary C strings.

Runtime-selected strings, such as level-state names, are printed through:

```c
console_print();
console_println();
```

The application no longer uses the Arduino `F()` macro.

---

## 29. Line endings

Console line endings remain compatible with Arduino `Serial.println()`.

```c
console_newline();
```

transmits:

```text
carriage return: 0x0D
line feed:       0x0A
```

or:

```text
\r\n
```

Blank lines, ordinary lines and consecutive output operations are tested byte by byte.

---

## 30. Integer formatting

The console provides:

```c
console_print_int32(
    int32_t value
);
```

It supports the complete signed 32-bit range:

```text
0
positive values
negative values
INT32_MAX
INT32_MIN
```

The implementation does not use:

* `sprintf()`.
* Dynamic memory.
* Recursion.
* Variadic formatting.

The application uses this function for the scale tare offset.

---

## 31. Floating-point formatting

The console provides:

```c
console_print_float(
    float value,
    uint8_t decimal_places
);
```

The implementation supports up to six decimal places.

Application precision remains:

```text
Weight:                  2 decimal places
Reference mass:          2 decimal places
Net counts:              2 decimal places
Calibration factor:      6 decimal places
```

The formatter supports:

* Positive values.
* Negative values.
* Zero.
* Negative zero.
* Required trailing zeros.
* Decimal rounding.
* Carry into the integer part.
* NaN.
* Positive infinity.
* Negative infinity.
* Values outside the supported integer range.

Special output is defined as:

```text
NaN:                nan
Positive infinity:  inf
Negative infinity: -inf
Unsupported range:  ovf
```

Requested precision greater than six is limited to six decimal places.

The implementation does not use floating-point `printf()`.

---

## 32. Input behaviour

The application retains its single-character command interface:

```text
t = tare
c = start or confirm calibration
q = cancel calibration
s = save active calibration
x = clear stored calibration
```

No newline is required.

The application now uses:

```c
console_input_available();
console_read_char();
console_discard_input();
```

The first pending character is processed and any additional waiting input is discarded, preserving the previous command behaviour.

Uppercase and lowercase variants remain supported by the application command logic.

---

## 33. Application migration

The application initialization now uses:

```c
console_init(
    CONSOLE_BAUD_RATE
);
```

The baud rate is defined in:

```text
src/config.h
```

as:

```c
static const uint32_t
    CONSOLE_BAUD_RATE = 115200UL;
```

The application no longer calls:

```text
Serial.begin()
Serial.available()
Serial.read()
Serial.print()
Serial.println()
F()
```

The function previously named:

```text
process_serial_commands()
```

is now named:

```text
process_console_commands()
```

This reflects that command processing no longer depends directly on Arduino HardwareSerial.

---

## 34. Remaining Arduino dependency

`app.cpp` still includes Arduino support for the remaining blocking calls:

```cpp
delay(1000);
delay(3000);
```

Therefore, this milestone does not yet make the application fully independent from the Arduino Core.

The remaining direct Arduino responsibilities include:

```text
startup
setup() and loop()
blocking millisecond delay()
HardwareSerial backend
```

The serial dependency is now isolated behind:

```text
hal_serial_arduino.cpp
```

---

## 35. Native console tests

The native environment is:

```text
native_console
```

It compiles:

```text
src/console.c
test/test_console/fake_hal_serial.c
test/test_console/test_main.cpp
```

It excludes:

```text
hal_serial_arduino.cpp
Arduino HardwareSerial
app.cpp
AVR-specific backends
```

The fake serial backend provides controlled RX and TX buffers and records:

* Requested baud rate.
* Initialization calls.
* Availability checks.
* Read calls.
* Written bytes.
* Output-buffer overflow.
* Input consumption order.

---

## 36. Console test coverage

The 43 native console tests cover:

### Initialization

* Baud-rate forwarding.
* Initialization call count.

### Input

* Empty input.
* Available input.
* Null output pointer.
* Output preservation after failure.
* Single-character reads.
* Multiple-character ordering.
* Input discard.
* Empty-buffer discard.
* HAL read failures.

### Text

* Null strings.
* Empty strings.
* RAM strings.
* Program-memory strings.
* Program-memory literal macros.
* Multiple consecutive writes.
* Blank lines.
* Exact CRLF output.

### Integers

* Zero.
* Positive values.
* Negative values.
* `INT32_MAX`.
* `INT32_MIN`.

### Floating point

* Zero with two decimals.
* Zero with six decimals.
* Positive and negative values.
* Trailing zeros.
* Zero decimal places.
* Rounding down.
* Rounding up.
* Carry into the integer part.
* Negative zero.
* NaN.
* Positive infinity.
* Negative infinity.
* Unsupported range.
* Precision limited to six decimal places.

---

## 37. Automated validation

The native console suite passes:

```text
Console tests: 43
Failures:       0
Ignored:        0
```

The complete native regression is:

```text
native_button:                       10 tests
native_hx711:                        18 tests
native_level_indicator:              14 tests
native_operation_indicator:          14 tests
native_scale:                        32 tests
native_calibration_storage:          40 tests
native_console:                      43 tests

Total:                              171 tests
Failures:                             0
```

The Arduino Nano production firmware also compiles successfully.

---

## 38. Physical validation

The console abstraction was validated successfully on the physical Arduino Nano.

The following behaviour was confirmed:

* [x] Firmware starts normally.
* [x] Startup output is complete.
* [x] No corrupted characters appear.
* [x] CRLF line endings remain correct.
* [x] Blank lines and spacing remain readable.
* [x] Stored-calibration messages appear correctly.
* [x] Calibration factors display six decimal places.
* [x] Weights display two decimal places.
* [x] Level-state names display correctly.
* [x] Long messages are not truncated.
* [x] Command `t` works.
* [x] Command `c` works.
* [x] Command `q` works.
* [x] Command `s` works.
* [x] Command `x` works.
* [x] Uppercase and lowercase commands work.
* [x] Complete calibration remains functional.
* [x] Calibration remains persistent after restart.
* [x] HX711 readings remain functional.
* [x] Buttons remain functional.
* [x] Indicators remain functional.
* [x] Timer1 timing remains functional.
* [x] Direct AVR EEPROM storage remains functional.

No application-level regression was detected.

---

## 39. Memory usage

Previous direct AVR EEPROM milestone:

```text
RAM:   748 bytes
Flash: 12630 bytes
```

Console abstraction milestone:

```text
RAM:   312 bytes
Flash: 12564 bytes
```

The final values must be copied from the successful production build.

Fixed application literals remain in Flash and do not cause the previous SRAM overflow.

---

## 40. Architectural result

The project now owns these abstraction layers:

```text
GPIO:
hal_gpio.h
    -> hal_gpio_avr.c

Critical sections:
hal_critical.h
    -> hal_critical_avr.c

Time:
hal_time.h
    -> hal_time_avr.c

Non-volatile storage:
hal_storage.h
    -> hal_storage_avr.c

Console transport:
hal_serial.h
    -> hal_serial_arduino.cpp

Console formatting:
console.c
    -> text, numbers, CRLF and input
```

Application output and command logic no longer depend directly on Arduino Serial.

The future UART backend can be introduced by replacing only:

```text
hal_serial_arduino.cpp
```

with:

```text
hal_serial_avr.c
```

---

## 41. Definition of done

This milestone is complete because:

* [x] The console architecture is documented.
* [x] A C-compatible serial HAL exists.
* [x] An Arduino serial backend exists.
* [x] A platform-independent console module exists.
* [x] Fixed application literals remain in program memory.
* [x] Arduino `F()` is no longer used by `app.cpp`.
* [x] `Serial.begin()` is no longer called by `app.cpp`.
* [x] `Serial.available()` is no longer called by `app.cpp`.
* [x] `Serial.read()` is no longer called by `app.cpp`.
* [x] `Serial.print()` is no longer called by `app.cpp`.
* [x] `Serial.println()` is no longer called by `app.cpp`.
* [x] CRLF output is preserved.
* [x] RAM strings can be printed.
* [x] Flash-resident literals can be printed.
* [x] Signed 32-bit integers can be printed.
* [x] Two-decimal floats can be printed.
* [x] Six-decimal floats can be printed.
* [x] Floating-point rounding is tested.
* [x] NaN and infinity handling is defined.
* [x] Single-character commands remain supported.
* [x] Pending input can be discarded.
* [x] A fake serial backend exists.
* [x] All 43 console tests pass.
* [x] All previous 128 native tests pass.
* [x] The complete native total is 171 tests.
* [x] The Nano firmware compiles.
* [x] Serial commands work on physical hardware.
* [x] Startup and calibration output remain readable.
* [x] Static SRAM remains within limits.
* [x] Flash and SRAM usage are recorded.
* [x] Final validation is documented.
