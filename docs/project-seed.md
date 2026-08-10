# Project Seed: Progressive Learning from Arduino to Bare-Metal with HX711

I am developing a long-term educational project to learn microcontroller programming progressively, without trying to learn too many layers at the same time.

## Functional objective

Build a system that:

* Reads a load cell through an HX711.
* Converts the reading into weight.
* Supports tare.
* Supports calibration using a known mass.
* Activates three LEDs depending on the measured weight or level.
* Serves as preparation for a future automated milk-mixer project.

The initial platform was expected to be:

* Classic Arduino Nano.
* ATmega328P at 16 MHz.
* VS Code.
* PlatformIO.
* HX711.
* `bogde/HX711` library.

The final educational objective is to progressively leave Arduino abstractions behind, write project-owned drivers, work directly with registers and eventually port the project to another platform, probably STM32.

---

# Project philosophy

The project should progress through small, functional versions.

Each stage should change a limited number of things so that errors can be located. The project should not jump directly from a fully functional Arduino library to a complete bare-metal implementation.

Each functional version should be saved with a Git commit and, when it represents an important milestone, with a tag.

Example:

```bash
git add .
git commit -m "Implement minimal HX711 reading"
git tag v0.1-hx711-minimal
```

The goal is not to create a large industrial architecture from the beginning. Separation of responsibilities should be sufficient for learning and for replacing components, while avoiding overengineering.

---

# Agreed learning path

## Stage 1: minimal version with Arduino and the Bogde library

Create a minimal version based on the `bogde/HX711` examples.

Objectives:

* Verify the wiring.
* Confirm that the HX711 responds.
* Read unprocessed ADC values.
* Confirm that the values change when weight is applied.
* Detect saturation, noise or incorrect connections.
* Test tare.
* Obtain an initial calibration factor.
* Display values through `Serial`.

This stage may use:

* `setup()`.
* `loop()`.
* `Serial`.
* `delay()`.
* The `bogde/HX711` library.
* Simple and even partially monolithic code.

The priority is to obtain a known functional reference.

This version should be preserved unchanged as a hardware diagnostic program.

Suggested milestone:

```text
v0.1-hx711-minimal
```

---

## Stage 2: separate responsibilities while retaining Arduino and Bogde

Rewrite the application while preserving exactly the same behaviour, but divide the code into modules.

Because the Bogde library is written in C++, modules that interact with it should initially compile as `.cpp`, not `.c`.

C++ may be used in a procedural style:

* Functions.
* `struct`.
* `enum`.
* Module-private variables.
* No inheritance required.
* No dynamic allocation.
* No complex classes.
* Avoid `String`.

Approximate initial architecture:

```text
src/
├── main.cpp
├── config.h
├── app/
│   ├── app.cpp
│   └── app.h
├── scale/
│   ├── scale.cpp
│   └── scale.h
└── level_indicator/
    ├── level_indicator.cpp
    └── level_indicator.h
```

Responsibilities:

### `main`

Only initializes the modules and repeatedly executes the application.

```cpp
void setup()
{
    scale_init();
    level_indicator_init();
    app_init();
}

void loop()
{
    app_update();
}
```

### `app`

Coordinates overall behaviour:

* Requests new measurements.
* Decides when to update the reading.
* Passes the weight to the indicator.
* Manages general application states.
* Must not know registers or HX711 implementation details.

### `scale`

Represents the complete scale:

* Tare.
* Calibration factor.
* Conversion from ADC counts to grams.
* Filtering.
* Averaging.
* Measurement validation.

The application should request something similar to:

```c
bool scale_read_grams(float *weight_grams);
```

The application must not directly use `HX711` class objects.

### `level_indicator`

Controls the three LEDs:

* Pin initialization.
* Selection of the corresponding LED.
* Possible error or waiting states.

### `config`

Contains:

* Pins.
* Thresholds.
* Sampling periods.
* Initial calibration factor.
* Configurable constants.

Suggested milestone:

```text
v0.2-structured-bogde
```

---

## Stage 3: project-owned HX711 driver while retaining Arduino Core

Remove the `bogde/HX711` dependency while temporarily retaining:

* `setup()` and `loop()`.
* `Serial`.
* `pinMode()`.
* `digitalRead()`.
* `digitalWrite()`.
* `delayMicroseconds()`.
* The Arduino build system.
* Initialization performed by Arduino Core.

Create a project-owned module:

```text
hx711/
├── hx711.cpp
└── hx711.h
```

Approximate interface:

```c
void hx711_init(void);
bool hx711_is_ready(void);
bool hx711_read_raw(int32_t *raw_value);
```

The driver should only communicate with the HX711.

It must:

1. Wait until `DOUT` becomes low.
2. Generate 24 clock pulses.
3. Read the 24 bits.
4. Generate the additional pulses required to select channel and gain.
5. Correctly sign-extend the 24-bit value to 32 bits.
6. Return unprocessed ADC counts.
7. Avoid blocking forever if the HX711 does not respond.
8. Respect the timing restrictions specified in the datasheet.

The HX711 driver must not know:

* Which load cell is connected.
* How many grams each count represents.
* The LED thresholds.
* How the application works.
* How data is displayed.

The correct relationship should be:

```text
app
 ├── scale
 │    └── hx711
 └── level_indicator
```

The `hx711` module produces ADC counts.

The `scale` module converts those counts into weight:

```text
weight = (raw_reading - tare_offset) / calibration_factor
```

Suggested milestone:

```text
v0.3-custom-hx711-arduino
```

---

## Stage 4: introduce a project-owned HAL

Create a small Hardware Abstraction Layer so that the HX711 driver does not depend directly on Arduino.

Approximate architecture:

```text
hal/
├── hal_gpio.h
├── hal_gpio.cpp
├── hal_time.h
├── hal_time.cpp
├── hal_uart.h
└── hal_uart.cpp
```

The HX711 driver should use functions similar to:

```c
void hal_hx711_clock_write(bool level);
bool hal_hx711_data_read(void);
void hal_delay_microseconds(uint16_t microseconds);
```

Initially, the HAL implementation may use Arduino:

```cpp
void hal_hx711_clock_write(bool level)
{
    digitalWrite(HX711_SCK_PIN, level ? HIGH : LOW);
}
```

The dependency chain would be:

```text
app
 └── scale
      └── hx711
           └── HAL
                └── Arduino Core
```

The objective is for `hx711.cpp` to stop knowing about:

* `digitalWrite()`.
* `digitalRead()`.
* Arduino pin numbers.
* ATmega328P-specific registers.

Suggested milestone:

```text
v0.4-hal-on-arduino
```

---

## Stage 5: implement the HAL through direct register access

Temporarily retain Arduino Core and `Serial` to simplify debugging, but internally replace the Arduino HAL functions with direct ATmega328P register access.

Learn progressively:

* `DDRx`: pin direction.
* `PORTx`: output writing and pull-up resistors.
* `PINx`: input reading.
* Bit masks.
* AND, OR, XOR and shift operations.
* Register read-modify-write operations.

Conceptual example:

```c
DDRD |= (1 << DDD2);
PORTD |= (1 << PORTD2);
PORTD &= ~(1 << PORTD2);

bool level = (PIND & (1 << PIND3)) != 0;
```

At this stage:

* `hx711` should not change.
* `scale` should not change.
* `app` should not change.
* Only the HAL implementation should change.

This will demonstrate that the separation between layers is working.

Suggested milestone:

```text
v0.5-register-hal-arduino-core
```

---

## Stage 6: completely remove the Arduino framework

Create a bare-metal project with:

```c
int main(void)
{
    hardware_init();
    app_init();

    while (1)
    {
        app_update();
    }
}
```

Remove:

* `setup()`.
* `loop()`.
* `Serial`.
* `millis()`.
* `delay()`.
* `digitalRead()`.
* `digitalWrite()`.
* The Arduino Core dependency.

Retain:

* AVR-GCC.
* AVR-LibC.
* Headers such as `<avr/io.h>`.
* AVR-LibC interrupt macros.
* Startup code supplied by the toolchain, unless it is intentionally studied later.

Implement progressively:

### GPIO

Direct input and output configuration.

### UART

Replace `Serial` while preserving debugging capability.

Approximate functions:

```c
void uart_init(uint32_t baudrate);
void uart_write_byte(uint8_t byte);
void uart_write_string(const char *text);
```

### Timebase

Configure a timer to generate a periodic tick, for example every 1 ms.

Approximate functions:

```c
void time_init(void);
uint32_t time_millis(void);
```

Pay attention to:

* Prescaler.
* Clock frequency.
* CTC mode.
* Compare register.
* Timer interrupt.
* Atomic access to variables wider than 8 bits.
* Correct use of `volatile`.
* Counter overflow.

### Interrupts

Use them only when they provide a clear advantage.

It is not necessary to use interrupts for everything. Polling may be used when it is sufficient.

### Clock

On the classic Arduino Nano, the fuses are normally already configured to use the board clock. Defining:

```c
#define F_CPU 16000000UL
```

does not physically configure the clock; it informs the code and some libraries of the expected frequency.

Fuse configuration should be treated as a separate topic and handled carefully.

Suggested milestone:

```text
v1.0-bare-metal-avr
```

---

## Stage 7: port the project to another platform

Port the project to a platform such as STM32.

The intention is to preserve, with minimal changes:

* `app`.
* `scale`.
* Calibration logic.
* Three-LED logic.
* Part or all of the HX711 driver.

Change mainly:

* GPIO HAL.
* Time HAL.
* UART HAL.
* Microcontroller initialization.
* Clock configuration.
* Toolchain and build system.

STM32 may be used to learn progressively:

* CMSIS.
* HAL.
* LL.
* Registers.
* NVIC.
* SysTick.
* Timers.
* GPIO.
* UART.
* DMA.

It is not necessary to begin STM32 with absolute bare-metal programming. Development may start with HAL or LL and progressively move to lower levels.

Suggested milestone:

```text
v2.0-stm32-port
```

---

# Target architecture

The approximate final architecture would be:

```text
src/
├── main.c
├── config.h
├── app/
│   ├── app.c
│   └── app.h
├── scale/
│   ├── scale.c
│   └── scale.h
├── drivers/
│   ├── hx711.c
│   └── hx711.h
├── indicators/
│   ├── level_indicator.c
│   └── level_indicator.h
└── hal/
    ├── hal_gpio.c
    ├── hal_gpio.h
    ├── hal_time.c
    ├── hal_time.h
    ├── hal_uart.c
    └── hal_uart.h
```

It is not necessary to create all these files from the beginning. They should only be added when there is a real responsibility to separate.

---

# Dependency rules

The application must not know the specific hardware.

```text
app → scale → hx711 → HAL → microcontroller
```

The dependency direction should be preserved.

## `app`

Knows that a scale exists, but does not know how the HX711 works.

## `scale`

Knows that it receives ADC counts from a converter, but manages grams, tare, calibration and filtering.

## `hx711`

Knows how to communicate with the chip, but does not know grams or system logic.

## HAL

Knows how to control GPIO, timers and UART on the specific platform.

## Platform

Contains registers specific to the ATmega328P, STM32 or another microcontroller.

---

# Important principles

## Do not change too many things simultaneously

Each stage should preserve a previous functional reference.

## Do not rewrite without a reason

Each refactor should have a clear objective:

* Better isolation.
* Greater testability.
* Replace a dependency.
* Learn a specific layer.
* Simplify future porting.

## Avoid overengineering

Separation of responsibilities does not mean creating many files.

A module should exist because it has its own reason to change.

## Keep the program observable

Preserve for as long as possible:

* Serial output.
* Error indicators.
* Raw values.
* Calculated weight.
* HX711 state.
* Calibration information.

## Work from the bottom up through small tests

Before integrating a component:

1. Test GPIO.
2. Test timing.
3. Test UART.
4. Test raw HX711 reading.
5. Test tare.
6. Test calibration.
7. Test filtering.
8. Test thresholds.
9. Integrate the complete application.

---

# Possible Git branches

A stable branch and educational branches may be used:

```text
main
feature/minimal-hx711
refactor/separation-of-responsibilities
feature/custom-hx711-driver
feature/hal
feature/register-gpio
feature/bare-metal-avr
feature/stm32-port
```

It is not necessary to keep all branches forever. What matters is having clear commits and recoverable versions.

---

# Initial state when resuming the project

When the project is resumed, first identify the stage at which it stopped.

Questions the assistant should answer:

1. Does a functional `bogde/HX711` reading already exist?
2. Has the scale been calibrated?
3. Are the pin assignments known?
4. Which exact board is being used?
5. What file structure exists?
6. Which modules have already been separated?
7. Is the Bogde library still being used?
8. Does a project-owned driver already exist?
9. Does the HAL use Arduino or registers?
10. Is `Serial` still retained?
11. What is the latest functional commit or tag?
12. What concrete error or next objective exists?

The project should not be restarted from zero if a functional stage already exists.

---

# Current project state

## v0.3: persistent calibration completed

The project currently uses an Arduino Nano, ATmega328P, Arduino Core and the `bogde/HX711` library.

The application is separated into modules:

```text
src/
├── main.cpp
├── app.cpp
├── app.h
├── button.cpp
├── button.h
├── calibration_storage.cpp
├── calibration_storage.h
├── config.h
├── indicator_leds.cpp
├── indicator_leds.h
├── level_indicator.cpp
├── level_indicator.h
├── operation_indicator.cpp
├── operation_indicator.h
├── scale.cpp
└── scale.h
```

Completed functionality:

* Load-cell reading through the HX711.
* Conversion from ADC counts to grams.
* Automatic tare during startup.
* Manual tare through a physical button or serial port.
* Three weight levels with hysteresis.
* Separation between level logic and physical LED control.
* Runtime-modifiable calibration factor.
* Persistent factor storage in EEPROM.
* EEPROM record with identifier, version and CRC.
* Validation of invalid factors or corrupted EEPROM data.
* Recovery of the default factor when no valid calibration exists.
* Calibration state machine.
* Calibration through the serial port.
* Physical calibration button.
* Long press to avoid starting calibration accidentally.
* Calibration cancellation through the tare button or serial command.
* LED indication for tare, calibration states, success and error.
* Diagnostic string literals stored in Flash through `F()` to reduce SRAM usage.

Current controls:

```text
D4:
    normal operation   → tare
    active calibration → cancel

D8:
    hold               → start calibration
    short press        → confirm step

Serial port:
    t → tare
    c → start or confirm calibration
    q → cancel calibration
    s → save the active factor
    x → invalidate the stored calibration
```

The current calibration factor remains provisional because the final mechanical platform has not yet been built.

`DEFAULT_CALIBRATION_FACTOR` is a fallback compiled into the firmware. The most recent device-specific calibration is stored in EEPROM.

## v0.4: very-low-level warning completed

* Four level states: `VERY_LOW`, `LOW`, `MEDIUM` and `HIGH`.
* Hysteresis between all levels.
* Very-empty-container warning through non-blocking blinking of the `LOW` LED.
* Distinct visual frequencies:
  - `VERY_LOW`: 250 ms per transition.
  - Calibration wait: 500 ms per transition.
  - Calibration success and error: 150 ms per transition.
* Operation patterns take priority over normal level indication.

Provisional levels:

```text
VERY_LOW:
    below 100 g

LOW:
    from 100 g to below 500 g

MEDIUM:
    from 500 g to below 1000 g

HIGH:
    1000 g and above
```

With 20 g hysteresis:

```text
VERY_LOW → LOW:
    120 g or more

LOW → VERY_LOW:
    80 g or less
```

## Next major educational stage

After completing the planned application functionality, the `bogde/HX711` dependency will be removed and a project-owned HX711 driver will be written while initially retaining Arduino Core.

The project-owned driver will be developed in an independent branch:

```text
feature/custom-hx711-driver
```

The application logic and communication driver will not be changed simultaneously.


## Custom HX711 driver milestone

The project now uses a custom HX711 driver instead of the Bogde HX711 library.

The HX711 protocol logic is implemented primarily in C and is separated from the temporary Arduino platform adapter. The driver supports initialization, readiness checking, finite timeouts, signed 24-bit raw readings, channel and gain selection, and power control.

The scale module preserves the previous tare, averaging and calibration behaviour. Persistent calibration, the physical tare button, the serial calibration workflow and LED level indication continue to operate correctly.

The Bogde dependency has been removed from `platformio.ini`, and the project has been successfully compiled, uploaded and physically tested on the Arduino Nano.

The current platform backend is `hx711_platform_arduino.cpp`. A future custom HAL should replace this backend without rewriting the HX711 protocol implementation.

Dedicated physical testing of the power-down and power-up functions and automated driver tests remain future improvements.

## Project HAL milestone

The `feature/project-hal` milestone introduced a project-specific hardware abstraction layer while retaining the Arduino framework as the temporary platform backend.

The project now provides C-compatible HAL interfaces for:

* Digital GPIO.
* Millisecond timing.
* Microsecond delays.
* Critical sections.

The implemented backend files are:

```text
hal_gpio_arduino.cpp
hal_time_arduino.cpp
hal_critical_avr.c
```

The HX711 platform adapter now uses the project HAL and no longer depends directly on Arduino or AVR functions.

The following modules were migrated:

* HX711 platform adapter.
* Physical indicator LED module.
* Button module.
* Level-indicator timing.
* Operation-indicator timing.
* Periodic application timing.

The HX711 protocol driver itself remained unchanged.

The existing behaviour was preserved:

* Weight measurement.
* Initial and physical tare.
* Persistent calibration.
* Button debounce.
* Long-hold calibration entry.
* Very-low-level blinking.
* Low, medium and high level indication.
* Operation-indicator patterns.
* Periodic serial output.

The project still uses the Arduino framework.

The remaining direct Arduino dependencies are primarily:

* Arduino startup through `setup()` and `loop()`.
* Serial communication and flash-string support.
* EEPROM storage.
* Temporary Arduino GPIO and time backends.

The critical-section backend is currently AVR-specific by design.

A future custom AVR backend should replace the Arduino GPIO and time backends without requiring changes to the HX711 driver or migrated application modules.

The next recommended major milestone is to add automated host-side tests using fake or simulated HAL backends before replacing additional platform infrastructure.

## Native unit tests — v0.7

The project now includes native host-side unit tests using PlatformIO, GCC and Unity.

Four isolated test environments were added:

* `native_button`
* `native_hx711`
* `native_level_indicator`
* `native_operation_indicator`

The native implementations replace hardware dependencies with fake GPIO, time, LED, HX711 platform and critical-section backends.

Current automated coverage:

* Button: 10 tests.
* HX711 driver: 18 tests.
* Level indicator: 14 tests.
* Operation indicator: 14 tests.
* Total: 56 native tests.

The tests cover debounce, long presses, contact bounce, timer overflow, HX711 bit reconstruction, signed 24-bit conversion, timeouts, gain pulses, critical sections, power control, level thresholds, hysteresis, blinking and operation-status patterns.

All native tests pass and the normal Arduino Nano firmware continues to compile.

The completed milestone will be tagged as `v0.7-native-unit-tests`.

The next planned milestone is to introduce direct AVR HAL backends incrementally while preserving the Arduino implementation as a reference and using the native tests to prevent regressions.


## Direct AVR GPIO backend — v0.8

The project now uses a direct ATmega328P register backend for digital GPIO.

The public project HAL remains unchanged:

```text
hal_gpio_configure_input()
hal_gpio_configure_input_pullup()
hal_gpio_configure_output()
hal_gpio_read()
hal_gpio_write()
```

The production GPIO path is now:

```text
Application modules
        |
        v
Project GPIO HAL
        |
        v
hal_gpio_avr.c
        |
        v
DDRx / PORTx / PINx
        |
        v
ATmega328P hardware
```

The direct AVR backend supports Arduino-compatible digital pin identifiers:

```text
D0-D7   -> PORTD
D8-D13  -> PORTB
D14-D19 -> PORTC
```

Nano pins A6 and A7 are not accepted as digital GPIO because they are analog-input-only pins.

The backend provides:

* Safe translation from Arduino pin identifiers to AVR registers and bit masks.
* Input configuration through `DDRx`.
* Internal pull-up control through `PORTx`.
* Output configuration through `DDRx`.
* Digital input reads through `PINx`.
* Digital output writes through `PORTx`.
* Safe handling of invalid pin identifiers.
* Critical-section protection for register read-modify-write operations.
* Preservation of unrelated bits in shared port registers.

The previous Arduino implementation remains available in:

```text
hal_gpio_arduino.cpp
```

but is excluded from the production Nano build through `platformio.ini`.

No changes were required in:

* Button logic.
* Indicator LED logic.
* Level-indicator logic.
* Operation-indicator logic.
* HX711 driver.
* HX711 platform adapter.
* Scale module.
* Application state machine.

This confirms that the project GPIO abstraction boundary works correctly.

Validation completed successfully:

* 56 native tests pass.
* Arduino Nano firmware compiles.
* HX711 communication works physically.
* Weight readings remain functional.
* Tare and calibration buttons work.
* Internal pull-ups work.
* Short-press and long-press detection work.
* All three LEDs work.
* Very-low warning blinking works.
* Tare, calibration, success and error patterns work.
* Persistent calibration continues loading.
* Complete calibration flow works.

Memory comparison:

```text
Arduino GPIO backend:
RAM:   744 bytes
Flash: 12406 bytes

Direct AVR GPIO backend:
RAM:   744 bytes
Flash: 12450 bytes
```

The direct backend therefore leaves SRAM usage unchanged and increases flash usage by 44 bytes in the current build.

The completed milestone is tagged:

```text
v0.8-avr-gpio-backend
```

The next planned milestone is a direct AVR time backend. It should replace `hal_time_arduino.cpp` incrementally while retaining the Arduino Core for startup, Serial and EEPROM during the transition.

## Direct AVR time backend — v0.9

The project now uses a direct ATmega328P time backend for project timing.

The public time HAL provides:

```text
hal_time_init()
hal_time_millis()
hal_time_delay_us()
```

The active production backend is:

```text
src/hal_time_avr.c
```

The previous Arduino implementation remains available as a reference:

```text
src/hal_time_arduino.cpp
```

but is excluded from the Arduino Nano production build.

The project time architecture is:

```text
Application timing
        |
        v
Project time HAL
        |
        +--> Timer1 CTC interrupt every 1 ms
        |        |
        |        v
        |   volatile uint32_t counter
        |
        +--> calibrated AVR busy loop
                 |
                 v
          microsecond delays
```

Timer1 configuration:

```text
CPU frequency:    16 MHz
Prescaler:        64
Timer frequency:  250 kHz
OCR1A:            249
Interrupt period: 1 ms
Interrupt vector: TIMER1_COMPA_vect
```

The 32-bit millisecond counter is incremented by a short Timer1 Compare Match A ISR.

`hal_time_millis()` reads the counter inside a critical section because the ATmega328P cannot read a 32-bit value atomically.

Natural `uint32_t` overflow is preserved, and application modules continue using unsigned subtraction for overflow-safe timing.

`hal_time_delay_us()` no longer calls Arduino `delayMicroseconds()`.

It uses AVR libc `_delay_loop_2()` with a calculation specific to the Nano's 16 MHz CPU frequency.

Large delays are divided into safe chunks, and a zero-microsecond request is handled explicitly.

The project now reserves Timer1.

PWM on D9 and D10, Timer1-based Servo implementations and other Timer1-dependent libraries must not be introduced without redesigning the timebase.

Timer0 remains controlled by Arduino during the transition and continues supporting remaining direct calls to:

```text
millis()
micros()
delay()
```

The active direct AVR low-level backends are now:

```text
hal_gpio_avr.c
hal_time_avr.c
hal_critical_avr.c
```

The Arduino Core remains temporarily for:

```text
startup
setup() and loop()
Serial
EEPROM
remaining direct delay() calls
```

Validation completed successfully:

* 56 native tests pass.
* Arduino Nano firmware compiles.
* Firmware starts normally.
* Serial remains functional.
* HX711 communication works.
* Weight readings remain functional.
* Tare works.
* Short and long button presses work.
* Button debounce remains correct.
* Level and operation-indicator timing remains correct.
* Complete calibration works.
* Persistent calibration continues loading after restart.

Memory usage with the direct AVR GPIO and time backends:

```text
RAM:   748 bytes
Flash: 12620 bytes
```

The completed milestone will be tagged:

```text
v0.9-avr-timebase
```

The next recommended milestone is to add native tests for the `scale` module before replacing persistent storage or Serial communication.

The likely following architectural stages are:

```text
v0.10: native tests for scale
v0.11: storage HAL and testable calibration records
v0.12: direct AVR EEPROM backend
v0.13: console/UART abstraction
v1.0: remove the Arduino Core and provide a project-owned main()
```

## Native scale tests — v0.10

The project now contains a native host-side unit-test suite for the scale module.

The environment is:

```text
native_scale
```

It compiles the real production implementation:

```text
src/scale.cpp
```

against a fake implementation of the public HX711 driver API:

```text
test/test_scale/fake_hx711_driver.c
```

The test architecture is:

```text
scale.cpp
    |
    v
hx711_driver.h
    |
    v
fake_hx711_driver.c
    |
    v
controlled readings, errors and call records
```

The fake HX711 driver can:

* Control initialization results.
* Control startup readiness results.
* Control current readiness.
* Supply sequences of raw readings.
* Inject read failures at selected positions.
* Record initialization pins.
* Record timeout values.
* Count driver calls.
* Count consumed readings.
* Detect unexpected extra reads.

The scale suite covers:

* Successful initialization.
* HX711 initialization failure.
* HX711 startup timeout.
* State reset after successful initialization.
* State preservation after failed initialization.
* Positive calibration factors.
* Negative calibration factors.
* Rejection of zero, NaN and infinity.
* Rejection of factors below the minimum magnitude.
* Acceptance of exact boundary factors.
* Preservation of the previous valid factor.
* Successful tare using 20 samples.
* Positive, negative and mixed tare readings.
* Failed tare at the first, intermediate and final sample.
* Preservation of the previous offset after failed tare.
* Repeated tare.
* Single-sample net-count calculations.
* Multi-sample net-count calculations.
* Signed integer truncation.
* Null output pointers.
* Zero sample counts.
* Read failures at multiple positions.
* Preservation of caller output values after failures.
* Positive weight conversions.
* Negative weight conversions.
* Positive and negative calibration-factor signs.
* HX711-not-ready behaviour.
* Weight-read failures.
* Arithmetic limits using 255 maximum or minimum HX711 readings.

The scale suite contains:

```text
32 tests
0 failures
```

The complete native regression now contains:

```text
native_button:              10 tests
native_hx711:               18 tests
native_level_indicator:     14 tests
native_operation_indicator: 14 tests
native_scale:               32 tests

Total:                      88 tests
Failures:                    0
```

The production Arduino Nano firmware continues compiling successfully.

This branch does not modify the public scale API or production behaviour.

The current test-layer architecture is:

```text
Application logic
    |
    +--> native_button
    +--> native_level_indicator
    +--> native_operation_indicator
    +--> native_scale

Hardware protocol logic
    |
    +--> native_hx711
```

The completed milestone is tagged:

```text
v0.10-native-scale-tests
```

The next recommended milestone is to separate calibration-record validation from physical EEPROM access.

A suitable progression is:

```text
v0.11  Testable calibration-record format and storage HAL
v0.12  Direct AVR EEPROM backend
v0.13  Console abstraction
v0.14  Direct AVR UART backend
v1.0   Project-owned main() without the Arduino Core
```

Production memory usage:

```text
RAM:   748 bytes
Flash: 12620 bytes
```

## Calibration storage HAL — v0.11

The project now separates calibration-record logic from physical non-volatile storage.

The architecture is:

```text
calibration_storage.cpp
        |
        +--> calibration_record.cpp
        |
        +--> hal_storage.h
                 |
                 v
        hal_storage_arduino.cpp
                 |
                 v
        Arduino EEPROM library
```

The public calibration-storage API remains unchanged:

```text
calibration_storage_load()
calibration_storage_save()
calibration_storage_clear()
```

The storage HAL provides:

```text
hal_storage_capacity()
hal_storage_read()
hal_storage_write()
```

It operates on byte ranges and does not understand calibration records.

The active production backend is:

```text
src/hal_storage_arduino.cpp
```

It uses:

```text
EEPROM.length()
EEPROM.read()
EEPROM.update()
```

The calibration record is now an explicit fixed-size binary format:

```text
Size: 12 bytes

Offset  Size  Field
0       4     Magic
4       2     Version
6       4     Calibration-factor bits
10      2     CRC-16/CCITT
```

Multibyte values use explicit little-endian encoding.

Preserved format values:

```text
Magic:       0x4C43414C
Version:     1
CRC:         CRC-16/CCITT
Polynomial:  0x1021
Initial CRC: 0xFFFF
```

For `45.5F`, the complete record is:

```text
4C 41 43 4C 01 00 00 00 36 42 90 F3
```

The explicit format remains compatible with calibration records written by the previous implementation on the ATmega328P.

The native environment:

```text
native_calibration_storage
```

tests the real:

```text
src/calibration_record.cpp
src/calibration_storage.cpp
```

against:

```text
test/test_calibration_storage/fake_hal_storage.cpp
```

The suite covers:

* Record encoding and decoding.
* Byte ordering.
* CRC generation and validation.
* Magic and version validation.
* Calibration-factor validation.
* Output preservation after failures.
* Successful load, save and clear operations.
* Insufficient storage capacity.
* Simulated read and write failures.
* Verification failures.
* Corrupted read-back data.
* Mismatched saved factors.
* Exact access addresses and lengths.
* Magic-only record invalidation.

Test totals:

```text
native_button:                       10
native_hx711:                        18
native_level_indicator:              14
native_operation_indicator:          14
native_scale:                        32
native_calibration_storage:          40

Total:                              128
Failures:                             0
```

Physical validation confirms:

* An existing calibration remains readable.
* The old and new record formats are compatible.
* A new calibration can be saved.
* Calibration persists across restart.
* Persistent calibration can be cleared.
* The default factor is restored after clearing.
* Existing scale, button, indicator and Serial behaviour remains functional.

Production memory usage:

```text
RAM:   748 bytes
Flash: 12606 bytes
```

The completed milestone is tagged:

```text
v0.11-calibration-storage-hal
```

The next recommended milestone is:

```text
v0.12-direct-avr-eeprom
```

It will replace `hal_storage_arduino.cpp` with a direct AVR EEPROM backend while preserving:

```text
calibration_storage.cpp
calibration_record.cpp
hal_storage.h
native calibration-storage tests
```

## Direct AVR EEPROM backend — v0.12

The project now uses a direct ATmega328P EEPROM backend for non-volatile calibration storage.

The active production storage backend is:

```text
src/hal_storage_avr.c
```

The previous Arduino backend remains available as a reference:

```text
src/hal_storage_arduino.cpp
```

but is excluded from the Nano production build.

The storage architecture is:

```text
calibration_storage.cpp
        |
        +--> calibration_record.cpp
        |
        +--> hal_storage.h
                 |
                 v
          hal_storage_avr.c
                 |
                 v
         ATmega328P EEPROM
```

The storage HAL remains unchanged:

```text
hal_storage_capacity()
hal_storage_read()
hal_storage_write()
```

The direct backend accesses:

```text
EEAR
EEDR
EECR
SPMCSR
```

EEPROM capacity is derived from:

```text
E2END + 1
```

For the ATmega328P:

```text
Capacity: 1024 bytes
```

Reads wait for any previous EEPROM write, load `EEAR`, activate `EERE` and retrieve the byte from `EEDR`.

Writes:

```text
wait for EEPE
wait for SPMEN
load EEAR and EEDR
select erase-and-write mode
protect the timed EEMPE -> EEPE sequence
restore interrupts
wait for physical completion
```

The `EEMPE` and `EEPE` sequence is implemented with consecutive inline-assembly instructions.

The backend preserves update-style behaviour:

```text
existing byte equals requested byte
    -> skip physical write

existing byte differs
    -> perform erase-and-write
```

The storage HAL remains synchronous.

When `hal_storage_write()` returns, all changed bytes have completed programming.

The calibration record remains unchanged:

```text
Address:       0
Size:          12 bytes
Magic:         0x4C43414C
Version:       1
Byte order:    little endian
Checksum:      CRC-16/CCITT
```

Physical validation confirms:

* A calibration written by the Arduino backend remains readable.
* A new calibration can be saved by the AVR backend.
* The saved factor survives reset.
* The saved factor survives a complete power cycle.
* Persistent calibration can be cleared.
* The default factor is selected after clearing.
* A new calibration can be saved again after clearing.
* HX711, buttons, indicators, Timer1 and Serial remain functional.

The complete native regression remains:

```text
native_button:                       10
native_hx711:                        18
native_level_indicator:              14
native_operation_indicator:          14
native_scale:                        32
native_calibration_storage:          40

Total:                              128
Failures:                             0
```

Production memory usage:

```text
RAM:   748 bytes
Flash: 12630 bytes
```

The active direct AVR low-level backends are now:

```text
hal_gpio_avr.c
hal_critical_avr.c
hal_time_avr.c
hal_storage_avr.c
```

The completed milestone is tagged:

```text
v0.12-direct-avr-eeprom
```

The next recommended milestone is to introduce a console or serial HAL.

A suitable progression is:

```text
v0.13  Console abstraction and native formatting tests
v0.14  Direct AVR UART backend
v1.0   Project-owned main() without the Arduino Core
```

## Console abstraction — v0.13

The application no longer uses Arduino Serial directly.

The console architecture is:

```text
app.cpp
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

The byte-oriented serial HAL exposes:

```text
hal_serial_init()
hal_serial_rx_available()
hal_serial_read_byte()
hal_serial_write_byte()
```

The platform-independent console module provides:

```text
console_init()
console_input_available()
console_read_char()
console_discard_input()
console_newline()
console_print()
console_println()
console_print_progmem()
console_println_progmem()
console_print_int32()
console_print_float()
```

Fixed application messages now use program-memory literal macros:

```text
CONSOLE_PRINT()
CONSOLE_PRINTLN()
```

This replaces Arduino `F()` while preserving the SRAM optimization.

The application no longer calls:

```text
Serial.begin()
Serial.available()
Serial.read()
Serial.print()
Serial.println()
F()
```

The existing command interface remains:

```text
t = tare
c = start or confirm calibration
q = cancel calibration
s = save active calibration
x = clear stored calibration
```

The console prints:

```text
CRLF line endings
signed 32-bit integers
floats with up to six decimal places
NaN as nan
positive infinity as inf
negative infinity as -inf
unsupported large values as ovf
```

The native console suite contains:

```text
43 tests
0 failures
```

The complete native regression now contains:

```text
native_button:                       10
native_hx711:                        18
native_level_indicator:              14
native_operation_indicator:          14
native_scale:                        32
native_calibration_storage:          40
native_console:                      43

Total:                              171
Failures:                             0
```

Physical validation confirms that startup output, calibration messages, numerical formatting and commands remain functional.

Production memory usage:

```text
RAM:   312 bytes
Flash: 12564 bytes
```

The active serial backend remains:

```text
src/hal_serial_arduino.cpp
```

The completed milestone is tagged:

```text
v0.13-console-abstraction
```

The next recommended milestone is:

```text
v0.14-direct-avr-uart
```

It will replace the Arduino HardwareSerial backend with a direct AVR USART0 implementation without changing `app.cpp`, `console.c` or `hal_serial.h`.

## Direct AVR UART — v0.14

The production firmware now accesses ATmega328P USART0 directly.

The active console architecture is:

```text
app.cpp
    |
    v
console.c
    |
    v
hal_serial.h
    |
    v
hal_serial_avr.c
    |
    v
ATmega328P USART0
```

The previous Arduino backend remains available as a reference:

```text
src/hal_serial_arduino.cpp
```

but is excluded from the Nano production build.

USART0 is configured for:

```text
115200 requested baud
asynchronous double-speed mode
UBRR0 = 16
8 data bits
no parity
1 stop bit
interrupt-driven reception
polling transmission
```

Reception uses a project-owned 64-byte circular buffer.

The receive-complete ISR stores incoming bytes while application code consumes them through:

```text
hal_serial_rx_available()
hal_serial_read_byte()
```

When the receive buffer is full, the newly received byte is discarded and existing queued data is preserved.

A physical validation identified that UART commands could remain queued during blocking operations even though physical button presses were not processed during those operations.

The application now discards console input received during:

```text
ordinary taring
calibration taring
calibration sample collection
temporary success or error indication patterns
```

This prevents stale commands from executing after a busy operation has completed.

The complete native regression remains:

```text
171 tests
0 failures
```

Production memory usage:

```text
RAM:   203 bytes
Flash: 11762 bytes
```

The completed milestone is tagged:

```text
v0.14-direct-avr-uart
```

## Project-owned millisecond delay — v0.15

The application no longer uses the Arduino `delay()` function.

The active millisecond-delay architecture is:

```text
app.cpp
    |
    v
hal_time_delay_ms()
    |
    v
hal_time_millis()
    |
    v
hal_time_avr.c
    |
    v
ATmega328P Timer1
```

The new API is:

```c
void hal_time_delay_ms(
    uint32_t milliseconds
);
```

Its common implementation is located in:

```text
src/hal_time_delay.c
```

The implementation:

* Returns immediately for a zero-duration request.
* Uses unsigned subtraction for overflow-safe elapsed-time calculation.
* Does not disable interrupts.
* Remains a blocking busy wait.
* Depends only on the public `hal_time_millis()` interface.

The native delay suite contains:

```text
6 tests
0 failures
```

The complete native regression now contains:

```text
native_button:                       10
native_hx711:                        18
native_level_indicator:              14
native_operation_indicator:          14
native_scale:                        32
native_calibration_storage:          40
native_console:                      43
native_time_delay:                    6

Total:                              177
Failures:                             0
```

The three remaining Arduino delay calls in `app.cpp` were replaced by:

```text
hal_time_delay_ms(1000UL)
hal_time_delay_ms(1000UL)
hal_time_delay_ms(3000UL)
```

`app.cpp` no longer includes `Arduino.h` and no longer calls any direct Arduino API.

Production memory usage:

```text
RAM:   203 bytes
Flash: 11664 bytes
```

The Arduino Core remains temporarily responsible for startup and the `setup()`/`loop()` entry model through `main.cpp`.

The completed milestone is tagged:

```text
v0.15-remove-arduino-delay
```

## Direct AVR entry point — v0.16

The production firmware no longer uses the Arduino application framework.

The production startup architecture is:

```text
AVR reset
    |
    v
Nano bootloader
    |
    v
AVR-LibC startup
    |
    v
main_avr.cpp
    |
    v
app_init()
    |
    v
app_update() forever
```

The project entry point explicitly enables global interrupts before application initialization:

```c
sei();
```

This allows the Timer1 timebase and interrupt-driven USART reception to operate during `app_init()`.

The primary PlatformIO environment is:

```text
nanoatmega328new
```

It does not contain:

```ini
framework = arduino
```

The Arduino execution model remains available only as a reference environment:

```text
nanoatmega328new_arduino
```

The production ELF contains:

```text
one main()
one Timer1 compare-match ISR
one USART0 receive ISR
```

It does not contain active Arduino Core symbols for:

```text
setup()
loop()
Arduino Timer0 timekeeping
HardwareSerial
global Serial
```

The existing Nano bootloader remains functional. Direct AVR firmware, Arduino reference firmware and direct AVR firmware were uploaded sequentially without affecting bootloader operation.

The complete native regression remains:

```text
177 tests
0 failures
```

Memory usage:

```text
Arduino reference:
RAM:   203 bytes
Flash: 11664 bytes

Direct AVR production:
RAM:   194 bytes
Flash: 11356 bytes
```

The active production architecture now uses project-owned entry, application, console, GPIO, time, EEPROM, UART and HX711 implementations.

The completed milestone is tagged:

```text
v0.16-direct-avr-entrypoint
```

# Functional prototype release — v1.0

The progressive development phase from `v0.1` through `v0.16` was consolidated as the first stable functional prototype.

Release tag:

```text
v1.0-functional-prototype
```

The release did not add a new firmware feature beyond `v0.16`. It established a reproducible baseline with:

- A complete project README.
- English project documentation.
- Release notes.
- A one-command native regression script.
- A cleaned branch structure.
- The direct AVR production build as the supported firmware.
- The Arduino entry-point environment retained only for controlled comparison.

Validated baseline:

```text
Native suites: 8/8
Tests:         177
Failures:        0
```

Memory usage:

```text
Direct AVR production:
RAM:   194 bytes
Flash: 11356 bytes

Arduino reference:
RAM:   203 bytes
Flash: 11664 bytes
```

The release tag is:

```text
v1.0-functional-prototype
```

# Safe startup tare — v1.1

The first post-`v1.0` milestone corrected a field-safety problem in the startup sequence.

Before this milestone, every restart performed an automatic tare. If power failed while the permanent container was partially filled, the current load became the new zero after reboot.

The new startup policy is:

```text
load calibration
    |
    v
load persistent tare
    |
    +--> valid
    |       → apply offset
    |       → normal measurement
    |
    +--> missing or invalid
            → TARE_REQUIRED
            → disable normal level indication
```

Automatic startup tare was removed.

## Persistent tare format

The project now owns a fixed 12-byte tare record:

```text
bytes 0–3
    magic: "TARE"

bytes 4–5
    format version

bytes 6–9
    signed int32_t offset, little-endian

bytes 10–11
    CRC-16/CCITT
```

The codec:

- Accepts positive and negative offsets.
- Accepts zero.
- Accepts `INT32_MIN` and `INT32_MAX`.
- Rejects null pointers.
- Rejects short buffers.
- Rejects invalid magic.
- Rejects unsupported versions.
- Rejects corrupted payload or checksum.
- Preserves output values after failed decoding.

The new modules are:

```text
src/tare_record.h
src/tare_record.cpp
```

## Shared EEPROM layout

Storage addresses are defined centrally in:

```text
src/storage_layout.h
```

Layout:

```text
EEPROM 0–11
    calibration record

EEPROM 12–23
    tare record

EEPROM 24 onward
    unused
```

The new storage module is:

```text
src/tare_storage.h
src/tare_storage.cpp
```

It provides:

```cpp
bool tare_storage_load(
    int32_t *tare_offset
);

bool tare_storage_save(
    int32_t tare_offset
);

bool tare_storage_clear(void);
```

Saving includes a write, read-back, complete decode and value comparison.

Clearing invalidates and verifies only the tare magic bytes.

Calibration storage and tare storage are tested to remain non-overlapping.

## Restorable scale offset

The scale module now provides:

```cpp
void scale_set_offset(
    int32_t tare_offset
);

int32_t scale_get_offset(void);

bool scale_tare(void);
```

`scale_tare()` reports success or failure.

A failed tare preserves the previous offset.

A stored offset can be restored without taking a new HX711 measurement.

## TARE_REQUIRED

When no valid tare exists:

```text
normal level indication disabled
all three LEDs blink slowly
console requests an empty-container tare
```

This state is distinct from `VERY_LOW`, because the firmware does not yet have a valid measurement zero.

An integration error was found during physical validation: `level_indicator_reset()` initially overwrote the operational blink output. The final application ownership order resets the level indicator first and then applies `TARE_REQUIRED`.

## Deliberate tare and serial commands

Normal-operation physical control:

```text
short D4 press
    → ignored

hold D4 for approximately 3 seconds
    → perform tare
    → save EEPROM
    → verify EEPROM
```

Serial service control:

```text
t
    → immediate persistent tare

z
    → clear only persistent tare
    → enter TARE_REQUIRED
```

The existing:

```text
x
```

continues clearing only the calibration record.

Physical and serial testing confirmed that `x` and `z` remain independent.

## Transactional tare behaviour

Before a new tare, the application preserves:

```text
previous runtime offset
previous tare-valid state
```

If sampling fails, no EEPROM write occurs.

If storage or verification fails:

```text
restore previous runtime offset
restore previous tare-valid state
report error
```

RAM and EEPROM therefore do not silently retain different zero references.

## Calibration interaction

The calibration-zero confirmation now:

```text
performs tare
    → saves and verifies tare
    → advances to reference-mass stage
```

If tare sampling or storage fails, calibration remains at zero confirmation.

If calibration is cancelled after zero confirmation:

- The new tare remains stored.
- The previous calibration factor remains active.

D4 still cancels calibration immediately.

The press used to cancel is suppressed until release, preventing the same long press from later triggering a normal-operation tare.

## Native regression

New environments:

```text
native_tare_record
native_tare_storage
```

Updated complete inventory:

```text
native_button:                       11
native_hx711:                        18
native_level_indicator:              14
native_operation_indicator:          16
native_scale:                        36
native_tare_record:                  20
native_tare_storage:                 21
native_calibration_storage:          40
native_console:                      43
native_time_delay:                    6

Total:                              225
Failures:                             0
Suites:                           10/10
```

The regression script returned:

```text
exit code 0
```

## Memory usage

Direct AVR production:

```text
RAM:   195 bytes
Flash: 13566 bytes
```

Arduino reference:

```text
RAM:   204 bytes
Flash: 13862 bytes
```

## Physical validation

Confirmed on the physical Nano:

- Missing tare produces `TARE_REQUIRED`.
- All three LEDs blink correctly.
- Serial and physical tare persist across reset.
- Short D4 presses do not tare.
- Long D4 holds tare exactly once.
- A load remains correctly measured after complete power loss.
- Calibration can begin from `TARE_REQUIRED`.
- Calibration zero persists tare.
- Complete calibration restores both records.
- A held D4 cancellation does not later trigger tare.
- Calibration and tare clear commands remain independent.

The completed milestone is tagged:

```text
v1.1-safe-startup-tare
```

## Cooperative application state machine — v1.2

The remaining multi-second application stalls have been removed from HX711
startup, operational tare and calibration sampling.

The application now coordinates all long-lived work through one explicit state
type:

```text
APP_STATE_STARTUP_WAIT_FOR_SCALE
APP_STATE_STARTUP_LOAD_CONFIGURATION
APP_STATE_TARE_REQUIRED
APP_STATE_NORMAL_OPERATION
APP_STATE_TARE_SAMPLING
APP_STATE_CALIBRATION_WAITING_FOR_ZERO
APP_STATE_CALIBRATION_ZERO_SAMPLING
APP_STATE_CALIBRATION_WAITING_FOR_MASS
APP_STATE_CALIBRATION_MASS_SAMPLING
APP_STATE_RESULT_PATTERN
APP_STATE_FAULT
```

The execution model remains a bare-metal cooperative superloop:

```cpp
while (true)
{
    app_update();
}
```

Each update performs a bounded state-machine step, reads at most one
already-ready HX711 conversion and returns. No RTOS, dynamic allocation or heap
storage was introduced.

### Incremental scale collection

The scale module now owns a reusable collector with four states:

```text
IDLE
IN_PROGRESS
COMPLETE
ERROR
```

The application uses:

```cpp
scale_start_sample_collection(sample_count);
scale_update_sample_collection();
scale_take_sample_average(&average_raw);
scale_cancel_sample_collection();
```

The collector does not apply tare or calibration changes. It only produces a
raw integer average. The application applies a candidate value after the
required EEPROM save and read-back verification succeeds.

The former blocking interfaces were removed:

```text
scale_tare()
scale_read_net_counts()
scale_read_weight()
```

Normal operation uses `scale_try_read_weight()` for exactly one ready sample.

### Cooperative startup and fault state

`scale_init()` now configures the HX711 without waiting for its first
conversion. `app_update()` polls readiness and loads configuration exactly
once.

If the HX711 does not become ready within 2000 ms:

```text
APP_STATE_STARTUP_WAIT_FOR_SCALE
    -> APP_STATE_FAULT
```

The superloop continues running. Only the HIGH LED blinks and UART input
receives:

```text
FAULT: Reset required.
```

The fault is deliberately latched until reset in this milestone.

### Non-blocking tare and calibration

Operational tare, calibration zero and calibration mass each collect 20
samples incrementally. At the current real rate of approximately 10 SPS, the
two-second operation no longer prevents input processing between samples.

All multi-sample operations have a 5000 ms total timeout. Elapsed-time checks
use unsigned subtraction and remain correct across millisecond-counter
overflow.

Cancellation is checked before acquiring the next sample. Serial `q` or a new
D4 press can therefore stop an operation without waiting for the remaining
conversions.

The persistence boundaries remain transactional:

```text
collect candidate
    -> save and verify candidate
    -> apply candidate only after success
```

An incomplete or failed tare preserves the previous offset. An incomplete or
failed mass phase preserves the previous calibration factor. A completed
calibration-zero phase persists its new tare independently, even if the later
mass phase is cancelled.

### Result and input policy

Temporary success and error feedback now owns the application through:

```text
APP_STATE_RESULT_PATTERN
```

UART bytes received during the pattern receive:

```text
Result indication is active.
```

and are discarded. D4 and D8 presses born during the pattern are suppressed
until stable release. Neither serial nor physical input can execute later in a
different state.

During sample collection, only cancellation is accepted. Other commands
receive an immediate state-specific response and are discarded.

### Native regression

The complete final inventory is:

```text
native_button:                       11
native_hx711:                        18
native_level_indicator:              14
native_operation_indicator:          16
native_scale:                        32
native_app:                          48
native_tare_record:                  20
native_tare_storage:                 21
native_calibration_storage:          40
native_console:                      43
native_time_delay:                    6

Total:                              269
Failures:                             0
Suites:                           11/11
```

The official PlatformIO runner returned exit code 0.

### Memory usage

Direct AVR production:

```text
RAM:   217 bytes
Flash: 16898 bytes
```

Arduino reference:

```text
RAM:   226 bytes
Flash: 17194 bytes
```

### Physical validation

Ten hardware scenarios passed on the Nano, real HX711, load cell, buttons and
LEDs. They covered:

- Recovery of stored tare and factor.
- Forced startup timeout without freezing the superloop.
- Serial and physical cancellation between samples.
- Button suppression until release.
- Completed tare and calibration persistence after power loss.
- Cancellation on both calibration sampling phases.
- Missing reference-mass rejection and retry.
- Input suppression during result patterns.
- Responsiveness throughout 20-sample operations at 10 SPS.

Physical HX711 power-down and power-up validation remains a separate pending
item and is not implied by this release.

Detailed records:

```text
docs/non-blocking-application-notes.md
docs/v1.2-non-blocking-application-validation.md
docs/v1.2-release-notes.md
```

The completed milestone is tagged:

```text
v1.2-non-blocking-application
```

The next educational step is Lesson 20 in the separate study repository. The
next planned production milestone is fault handling and watchdog policy.
