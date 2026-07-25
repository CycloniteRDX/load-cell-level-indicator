# AVR GPIO Backend Notes

## 1. Purpose

The purpose of this branch is to replace the temporary Arduino GPIO backend with a direct ATmega328P register implementation.

The public GPIO HAL interface will remain unchanged:

```text
include/hal_gpio.h
```

Project modules will continue calling:

```text
hal_gpio_configure_input()
hal_gpio_configure_input_pullup()
hal_gpio_configure_output()
hal_gpio_read()
hal_gpio_write()
```

Only the backend implementation will change.

The intended architecture is:

```text
Before this branch:

Application modules
        |
        v
Project GPIO HAL
        |
        v
Arduino pinMode(), digitalRead() and digitalWrite()
        |
        v
ATmega328P hardware
```

```text
After this branch:

Application modules
        |
        v
Project GPIO HAL
        |
        v
ATmega328P GPIO registers
        |
        v
ATmega328P hardware
```

---

## 2. Main objectives

This branch will:

* Add a direct AVR implementation of the GPIO HAL.
* Configure GPIO direction through `DDRB`, `DDRC` and `DDRD`.
* control output levels and pull-up resistors through `PORTB`, `PORTC` and `PORTD`.
* Read physical input states through `PINB`, `PINC` and `PIND`.
* Preserve the existing public HAL interface.
* Preserve the existing Arduino pin-number identifiers.
* Keep the Arduino GPIO backend as a reference implementation.
* Select the AVR backend through the PlatformIO build configuration.
* Preserve all existing application behaviour.
* Keep all 56 native tests passing.
* Validate the new backend on the physical Arduino Nano.

---

## 3. Non-objectives

This branch will not:

* Remove the Arduino framework.
* Replace `setup()` or `loop()`.
* Replace `millis()`.
* Replace `delayMicroseconds()`.
* Replace Serial communication.
* Replace EEPROM access.
* Change the public GPIO HAL interface.
* Change the HX711 protocol driver.
* Change button debounce behaviour.
* Change LED thresholds or patterns.
* Change application state machines.
* Add STM32 support.
* Add interrupt-driven GPIO.
* Add pin-change interrupt support.
* Add PWM support.
* Add analog-input support.
* Add dynamic memory allocation.

Only the GPIO backend will be replaced.

---

## 4. Existing public interface

The current public interface is declared in:

```text
include/hal_gpio.h
```

```c
typedef uint8_t hal_gpio_pin_t;

void hal_gpio_configure_input(
    hal_gpio_pin_t pin
);

void hal_gpio_configure_input_pullup(
    hal_gpio_pin_t pin
);

void hal_gpio_configure_output(
    hal_gpio_pin_t pin
);

bool hal_gpio_read(
    hal_gpio_pin_t pin
);

void hal_gpio_write(
    hal_gpio_pin_t pin,
    bool level
);
```

This interface will not change during this branch.

Application modules must not include AVR headers or access AVR registers directly.

---

## 5. New backend

The new implementation will be created in:

```text
src/hal_gpio_avr.c
```

It will be written in C.

The implementation will include:

```c
#include <avr/io.h>

#include "hal_gpio.h"
```

The backend may also use:

```c
#include "hal_critical.h"
```

to protect register read-modify-write operations.

The existing implementation:

```text
src/hal_gpio_arduino.cpp
```

will remain in the repository as a reference, but it will be excluded from the Arduino Nano build when the AVR backend is activated.

---

## 6. Arduino Nano pin numbering

The public HAL will continue receiving Arduino-compatible digital pin numbers.

The supported mapping will be:

| Arduino pin identifier | ATmega328P port | Port bit |
| ---------------------: | --------------- | -------: |
|                      0 | PORTD           |        0 |
|                      1 | PORTD           |        1 |
|                      2 | PORTD           |        2 |
|                      3 | PORTD           |        3 |
|                      4 | PORTD           |        4 |
|                      5 | PORTD           |        5 |
|                      6 | PORTD           |        6 |
|                      7 | PORTD           |        7 |
|                      8 | PORTB           |        0 |
|                      9 | PORTB           |        1 |
|                     10 | PORTB           |        2 |
|                     11 | PORTB           |        3 |
|                     12 | PORTB           |        4 |
|                     13 | PORTB           |        5 |
|                14 / A0 | PORTC           |        0 |
|                15 / A1 | PORTC           |        1 |
|                16 / A2 | PORTC           |        2 |
|                17 / A3 | PORTC           |        3 |
|                18 / A4 | PORTC           |        4 |
|                19 / A5 | PORTC           |        5 |

Arduino Nano pins A6 and A7 are analog-input-only pins on the ATmega328P package used by the Nano.

They will not be supported as digital GPIO by this backend.

---

## 7. Pins currently used by the project

The current project uses:

| Function           | Arduino pin | AVR port and bit |
| ------------------ | ----------: | ---------------- |
| HX711 DOUT         |           2 | PD2              |
| HX711 PD_SCK       |           3 | PD3              |
| Tare button        |           4 | PD4              |
| Low-level LED      |           5 | PD5              |
| Medium-level LED   |           6 | PD6              |
| High-level LED     |           7 | PD7              |
| Calibration button |           8 | PB0              |

The current hardware therefore uses:

```text
PORTD bits 2–7
PORTB bit 0
```

The backend will nevertheless support all standard digital pins from 0 through 19.

---

## 8. Internal pin mapping

A private mapping structure will associate each HAL pin identifier with its AVR registers and bit mask.

The preliminary internal representation is:

```c
typedef struct
{
    volatile uint8_t *direction_register;
    volatile uint8_t *output_register;
    volatile uint8_t *input_register;
    uint8_t bit_mask;
    bool valid;
} avr_gpio_pin_mapping_t;
```

The register pointers represent:

```text
direction_register → DDRx
output_register    → PORTx
input_register     → PINx
```

The bit mask will be calculated as:

```c
(uint8_t)(1U << bit_position)
```

The mapping function will remain private to `hal_gpio_avr.c`.

A preliminary signature is:

```c
static avr_gpio_pin_mapping_t
avr_gpio_get_pin_mapping(
    hal_gpio_pin_t pin
);
```

---

## 9. Meaning of volatile register pointers

AVR peripheral registers can change independently of normal program flow.

For example:

* An input pin can change because of an external electrical signal.
* Hardware can update status-register bits.
* Interrupts can alter hardware-related state.

The register pointers must therefore use:

```c
volatile uint8_t *
```

The `volatile` qualifier tells the compiler that every read and write is significant and must not be removed or replaced with a previously cached value.

The pointer itself stores the address of the register.

Dereferencing the pointer accesses the hardware register:

```c
*register_pointer
```

---

## 10. GPIO configuration behaviour

### Input without pull-up

The direction bit must be cleared:

```c
DDRx &= (uint8_t)~mask;
```

The corresponding output-latch bit must also be cleared to disable the internal pull-up:

```c
PORTx &= (uint8_t)~mask;
```

Conceptually:

```text
DDRx bit  = 0
PORTx bit = 0
```

### Input with internal pull-up

The direction bit must be cleared:

```c
DDRx &= (uint8_t)~mask;
```

The output-latch bit must be set:

```c
PORTx |= mask;
```

Conceptually:

```text
DDRx bit  = 0
PORTx bit = 1
```

### Output

The direction bit must be set:

```c
DDRx |= mask;
```

Configuring a pin as an output will not intentionally change the existing output-latch value.

The caller may set the output level separately through:

```text
hal_gpio_write()
```

---

## 11. Reading and writing pins

### Reading

The physical pin state will be read from `PINx`:

```c
return ((*input_register & bit_mask) != 0U);
```

The function returns:

```text
true  → pin is HIGH
false → pin is LOW
```

### Writing

A HIGH level will set the corresponding `PORTx` bit:

```c
PORTx |= mask;
```

A LOW level will clear it:

```c
PORTx &= (uint8_t)~mask;
```

The operation must preserve every unrelated bit in the same port register.

---

## 12. Read-modify-write operations

Operations such as:

```c
*register_pointer |= bit_mask;
```

are read-modify-write operations.

Conceptually, the processor performs:

1. Read the complete register.
2. Modify one bit in a CPU register.
3. Write the complete value back.

An interrupt modifying another bit in the same hardware register between those steps could cause an update to be lost.

The backend should protect relevant `DDRx` and `PORTx` read-modify-write operations using the project critical-section HAL.

The intended pattern is:

```c
const hal_critical_state_t previous_state =
    hal_critical_enter();

/* Register read-modify-write operation. */

hal_critical_exit(previous_state);
```

Nested critical sections are expected to remain safe because the previous interrupt state is saved and restored exactly.

Reading `PINx` does not require a critical section.

---

## 13. Invalid pin behaviour

The public GPIO HAL currently returns no status code.

The backend must therefore handle invalid pin identifiers safely.

For an invalid pin:

```text
Configuration functions → perform no operation
Write function          → perform no operation
Read function           → return false
```

Invalid pins must never cause:

* Null-pointer dereferences.
* Access to arbitrary memory addresses.
* Writes to unrelated hardware registers.
* Undefined bit shifts.

The internal mapping structure will use:

```c
valid = false;
```

to represent an unsupported pin.

---

## 14. Backend selection

Both implementations will remain in the repository:

```text
src/hal_gpio_arduino.cpp
src/hal_gpio_avr.c
```

Only one may be compiled into the Nano firmware because both implement the same public functions.

The Arduino Nano environment will eventually exclude the Arduino backend:

```ini
[env:nanoatmega328new]
platform = atmelavr
board = nanoatmega328new
framework = arduino
monitor_speed = 115200

build_src_filter =
    +<*>
    -<hal_gpio_arduino.cpp>
```

The new `hal_gpio_avr.c` file will then be included automatically.

The native test environments already select only the source files required by each suite, so they should not compile either physical GPIO backend.

---

## 15. Planned implementation order

### Stage 1: design documentation

Create this document and define:

* Scope.
* Pin mapping.
* Invalid-pin behaviour.
* Register-access rules.
* Backend-selection strategy.
* Validation plan.

Planned commit:

```text
docs: define AVR GPIO backend strategy
```

### Stage 2: AVR pin mapping

Create:

```text
src/hal_gpio_avr.c
```

Implement the private mapping between Arduino pin numbers and AVR registers.

Do not activate the backend yet.

Planned commit:

```text
feat: add AVR GPIO pin mapping
```

### Stage 3: input and output configuration

Implement:

```text
hal_gpio_configure_input()
hal_gpio_configure_input_pullup()
hal_gpio_configure_output()
```

Planned commit:

```text
feat: configure GPIO through AVR registers
```

### Stage 4: digital reading and writing

Implement:

```text
hal_gpio_read()
hal_gpio_write()
```

Planned commit:

```text
feat: read and write GPIO through AVR registers
```

### Stage 5: backend activation

Exclude:

```text
hal_gpio_arduino.cpp
```

from the Nano build.

Compile and physically validate the firmware with the AVR backend.

Planned commit:

```text
build: select AVR GPIO backend
```

### Stage 6: final documentation

Record:

* Compilation result.
* Physical validation.
* Flash and SRAM usage.
* Any implementation decisions changed during development.

Planned commit:

```text
docs: record AVR GPIO backend validation
```

---

## 16. Automated validation

The complete native regression must continue passing:

```text
pio test -e native_button
pio test -e native_hx711
pio test -e native_level_indicator
pio test -e native_operation_indicator
```

The Nano firmware must continue compiling:

```text
pio run -e nanoatmega328new
```

The native tests validate the modules above the GPIO backend.

They do not directly execute ATmega328P GPIO registers on the development computer.

---

## 17. Physical validation

After activating the AVR backend, the following physical checks will be performed:

* HX711 initialization succeeds.
* HX711 raw readings continue updating.
* Weight measurement remains stable.
* Tare button input works.
* Calibration button short press works.
* Calibration button long press works.
* Button pull-up configuration works.
* Button debounce behaviour is unchanged.
* Low-level LED works.
* Medium-level LED works.
* High-level LED works.
* Very-low warning blinking works.
* Tare-operation pattern works.
* Calibration-operation patterns work.
* Success pattern works.
* Error pattern works.
* Complete calibration flow works.
* Stored calibration continues loading correctly.

---

## 18. Production-code constraints

The following modules should not require changes:

```text
button.cpp
indicator_leds.cpp
level_indicator.cpp
operation_indicator.cpp
hx711_driver.c
hx711_platform.c
scale.cpp
app.cpp
main.cpp
```

A required change to one of these modules may indicate that the GPIO abstraction boundary is incomplete or has been violated.

The expected production changes are limited to:

```text
src/hal_gpio_avr.c
platformio.ini
documentation
```

The existing Arduino backend may remain unchanged.

---

## 19. Definition of done

This branch will be complete when:

* [ ] The AVR GPIO design is documented.
* [ ] Arduino pins 0–19 are mapped safely.
* [ ] Invalid pins are handled safely.
* [ ] Inputs can be configured without pull-ups.
* [ ] Inputs can be configured with internal pull-ups.
* [ ] Outputs can be configured.
* [ ] Digital inputs can be read through `PINx`.
* [ ] Digital outputs can be controlled through `PORTx`.
* [ ] Unrelated port bits are preserved.
* [ ] Read-modify-write operations are protected.
* [ ] The Arduino GPIO backend remains available as a reference.
* [ ] The AVR GPIO backend is selected for the Nano build.
* [ ] All 56 native tests pass.
* [ ] The Arduino Nano firmware compiles.
* [ ] Physical buttons work correctly.
* [ ] Physical LEDs work correctly.
* [ ] Physical HX711 communication works correctly.
* [ ] Calibration behaviour remains unchanged.
* [ ] Flash and SRAM usage are recorded.
* [ ] Final validation is documented.
