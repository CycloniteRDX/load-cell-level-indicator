# Project HAL Design Notes

## 1. Purpose

The purpose of this branch is to introduce a small Hardware Abstraction Layer, or HAL, for the project.

The HAL will separate application and driver code from the platform-specific functions currently provided by Arduino.

The first version of the HAL will continue using the Arduino framework internally.

The initial goal is not to remove Arduino immediately. The goal is to ensure that most project modules no longer call Arduino functions directly.

The intended progression is:

```text
Current architecture:

Application and modules
        |
        v
Arduino functions and AVR-specific code
        |
        v
ATmega328P hardware
```

```text
Architecture after this branch:

Application and modules
        |
        v
Project HAL
        |
        v
Temporary Arduino and AVR backends
        |
        v
ATmega328P hardware
```

```text
Future architecture:

Application and modules
        |
        v
Project HAL
        |
        v
Custom AVR backend
        |
        v
ATmega328P hardware
```

A future STM32 or other microcontroller backend may also implement the same HAL interfaces.

---

## 2. Main objectives

This branch will:

* Define a small project-specific HAL.
* Use C-compatible public interfaces.
* Abstract basic GPIO operations.
* Abstract millisecond timing.
* Abstract microsecond delays.
* Abstract critical sections.
* Preserve the existing application behaviour.
* Route the HX711 platform adapter through the project HAL.
* Replace direct Arduino GPIO access inside the LED module.
* Replace direct Arduino GPIO and timing access inside the button module.
* Keep the Arduino framework as the temporary backend.
* Prepare the project for a future custom AVR implementation.

---

## 3. Non-objectives

This branch will not:

* Remove `framework = arduino`.
* Replace `setup()` and `loop()`.
* Replace the Arduino serial implementation.
* Replace EEPROM access.
* Implement a complete general-purpose HAL.
* Add support for every possible GPIO mode.
* Add a STM32 backend.
* Rewrite the application state machine.
* Change the calibration workflow.
* Change the tare behaviour.
* Change weight calculations.
* Change LED thresholds or hysteresis.
* Change button debounce behaviour.
* Convert every project module from C++ to C.
* Introduce dynamic memory allocation.
* Directly configure all ATmega328P registers.

Serial communication and persistent storage will be addressed in later branches.

---

## 4. What the HAL represents

The HAL provides a stable interface between project code and platform-specific hardware access.

Project modules should request operations such as:

```text
Configure this pin as an output.
Read this input pin.
Write a logical level.
Read the current millisecond counter.
Wait for a short number of microseconds.
Enter a critical section.
Restore the previous interrupt state.
```

The modules should not need to know whether these operations are implemented using:

```text
Arduino functions
AVR registers
STM32 HAL
STM32 LL
another microcontroller platform
```

---

## 5. HAL responsibilities

The initial HAL will be responsible for three areas.

### GPIO

* Configure a pin as a digital input.
* Configure a pin as an input with an internal pull-up.
* Configure a pin as a digital output.
* Read the logical state of a digital input.
* Write the logical state of a digital output.

### Time

* Return the number of milliseconds elapsed since startup.
* Provide short microsecond delays.

### Critical sections

* Save the current interrupt state.
* Disable interrupts.
* Restore the exact previous interrupt state.

---

## 6. Responsibilities outside the HAL

The HAL will not contain:

* HX711 protocol logic.
* Button debounce logic.
* LED level-selection logic.
* Weight averaging.
* Tare calculations.
* Calibration calculations.
* EEPROM data formats.
* Serial commands.
* User-interface messages.
* Application state machines.

The HAL provides primitive hardware operations.

Drivers and modules use those primitive operations to implement their own behaviour.

---

## 7. Layer boundaries

The intended dependency direction is:

```text
Application
    |
    v
Application modules
    |
    v
Hardware drivers
    |
    v
Project HAL interfaces
    |
    v
Platform backends
    |
    v
Microcontroller hardware
```

Dependencies should point downward.

The HAL must not call application modules.

The HAL must not contain project-specific decisions such as LED thresholds, tare rules or calibration logic.

---

## 8. Initial HAL scope

The first HAL version will contain:

```text
hal_gpio
hal_time
hal_critical
```

The scope is intentionally small.

Additional interfaces should only be added when an existing module genuinely needs them.

This avoids creating a large theoretical abstraction that is not required by the current project.

---

## 9. GPIO interface

The preliminary GPIO interface will be declared in:

```text
include/hal_gpio.h
```

The proposed interface is:

```c
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
```

The pin type is initially an unsigned 8-bit integer because the current Arduino Nano pin identifiers fit within this type.

The alias `hal_gpio_pin_t` prevents modules from depending directly on the underlying representation.

The representation may be changed in a future platform migration if required.

---

## 10. GPIO backend

The initial implementation will use Arduino functions.

The expected file is:

```text
src/hal_gpio_arduino.cpp
```

The backend will translate HAL operations into:

```text
pinMode()
digitalRead()
digitalWrite()
```

Only the backend should include `Arduino.h`.

Modules using `hal_gpio.h` should not include Arduino headers merely to control pins.

A future custom AVR backend may replace this file with:

```text
src/hal_gpio_avr.c
```

The AVR backend would use data-direction, input and output registers directly.

---

## 11. Time interface

The preliminary time interface will be declared in:

```text
include/hal_time.h
```

The proposed interface is:

```c
#ifndef HAL_TIME_H
#define HAL_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t hal_time_millis(void);

void hal_time_delay_us(
    uint16_t microseconds
);

#ifdef __cplusplus
}
#endif

#endif
```

The first version contains only the operations currently required by the project.

A millisecond delay function will not be added unless an actual migrated module requires it.

Elapsed-time calculations should continue using unsigned subtraction:

```c
elapsed_time = current_time - start_time;
```

This preserves correct behaviour across the normal overflow of the millisecond counter.

---

## 12. Time backend

The initial implementation will be located in:

```text
src/hal_time_arduino.cpp
```

It will translate the HAL interface into:

```text
millis()
delayMicroseconds()
```

A future custom AVR backend will need to provide:

* A system time base.
* A millisecond counter.
* A microsecond delay mechanism.
* Any required timer initialization.

The implementation of that future time base is outside the scope of this branch.

---

## 13. Critical-section interface

The preliminary critical-section interface will be declared in:

```text
include/hal_critical.h
```

The proposed interface is:

```c
#ifndef HAL_CRITICAL_H
#define HAL_CRITICAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t hal_critical_state_t;

hal_critical_state_t hal_critical_enter(void);

void hal_critical_exit(
    hal_critical_state_t previous_state
);

#ifdef __cplusplus
}
#endif

#endif
```

Entering a critical section must:

1. Save the previous interrupt state.
2. Disable interrupts.
3. Return the saved state.

Leaving a critical section must restore the exact saved state.

It must not always enable interrupts unconditionally.

This is important because the caller may enter a critical section while interrupts are already disabled.

---

## 14. Critical-section backend

The current board uses an AVR ATmega328P.

The initial backend may use the AVR status register and interrupt instructions already used by the HX711 platform adapter.

The expected file may be:

```text
src/hal_critical_avr.c
```

This name makes the architecture dependency explicit.

Although the project still uses the Arduino framework, saving and restoring the AVR status register is an AVR-specific operation rather than a portable Arduino operation.

A future STM32 backend would implement the same public HAL interface using the appropriate ARM interrupt-state mechanism.

---

## 15. C and C++ compatibility

The HAL interfaces will be compatible with both C and C++.

Public headers will use:

```c
#ifdef __cplusplus
extern "C" {
#endif
```

and:

```c
#ifdef __cplusplus
}
#endif
```

This permits:

* C modules to call the HAL.
* C++ modules to call the HAL.
* Arduino C++ backends to implement functions used from C.
* Gradual migration of project modules from C++ to C.

The HAL itself will use procedural functions rather than C++ classes.

---

## 16. Header dependencies

Public HAL headers must not include:

```text
Arduino.h
avr/io.h
avr/interrupt.h
STM32 headers
platform-specific framework headers
```

They should include only standard C headers required by their public types.

Platform-specific includes belong exclusively in backend implementation files.

This keeps the public interfaces independent of the selected platform.

---

## 17. Proposed file structure

The initial structure is expected to become:

```text
include/
├── hal_gpio.h
├── hal_time.h
├── hal_critical.h
├── hx711_driver.h
└── existing public headers

src/
├── hal_gpio_arduino.cpp
├── hal_time_arduino.cpp
├── hal_critical_avr.c
├── hx711_driver.c
├── hx711_platform.c
├── hx711_platform.h
├── button.cpp
├── indicator_leds.cpp
├── level_indicator.cpp
├── scale.cpp
└── existing source files
```

The exact organization may be refined during implementation.

The public HAL headers belong in `include/` because several independent modules will use them.

Backend implementations belong in `src/`.

---

## 18. HX711 integration

The HX711 protocol driver will remain separated from the project HAL.

The driver will continue using its internal platform interface:

```text
hx711_driver.c
        |
        v
hx711_platform.h
```

The current implementation is:

```text
hx711_driver.c
        |
        v
hx711_platform_arduino.cpp
        |
        v
Arduino and AVR functions
```

After this branch, the intended structure is:

```text
hx711_driver.c
        |
        v
hx711_platform.c
        |
        v
Project HAL
        |
        v
Arduino and AVR backends
```

The new `hx711_platform.c` will translate driver-specific platform calls into the general project HAL.

For example:

```c
void hx711_platform_write_pin(
    uint8_t pin,
    bool level
)
{
    hal_gpio_write(pin, level);
}
```

This preserves the portability and independence of `hx711_driver.c`.

The HX711 protocol implementation should not be rewritten during this migration.

---

## 19. LED module migration

The physical LED module will use the GPIO HAL.

The module that configures and writes the LED pins should replace direct calls to:

```text
pinMode()
digitalWrite()
```

with:

```text
hal_gpio_configure_output()
hal_gpio_write()
```

The higher-level level-indicator state machine should remain unchanged because it does not need to know how GPIO operations are implemented.

The following behaviour must remain unchanged:

* LED pin assignments.
* Active-high or active-low behaviour.
* Low-level indication.
* Medium-level indication.
* High-level indication.
* All-LEDs-off behaviour.
* Thresholds.
* Hysteresis.

---

## 20. Button module migration

The button module will use:

```text
hal_gpio
hal_time
```

Direct calls to:

```text
pinMode()
digitalRead()
millis()
```

will be replaced with their HAL equivalents.

The following behaviour must remain unchanged:

* Internal pull-up configuration.
* Pressed and released logic levels.
* Debounce timing.
* Press-event generation.
* Long-press or calibration behaviour, if currently present.
* Non-blocking execution.

The migration must change hardware access only, not button logic.

---

## 21. Application modules that remain unchanged

This branch should avoid changing modules that do not directly access the migrated hardware primitives.

In particular, the following logic should remain unchanged unless a compilation dependency requires a minimal adjustment:

```text
Application state machine
Calibration state machine
Persistent calibration logic
Scale calculations
Level classification
Serial command handling
Main application behaviour
```

A module should not be modified merely to make the branch appear more comprehensive.

---

## 22. Arduino dependency after this branch

After completing this branch, the project will still depend on Arduino.

The difference will be where the dependency exists.

Before this branch, Arduino functions may be called from several modules.

After this branch, direct use of the migrated Arduino functions should be concentrated in:

```text
hal_gpio_arduino.cpp
hal_time_arduino.cpp
main or application code not yet migrated
serial-related modules
EEPROM-related modules
```

The project will therefore not yet be bare metal.

It will be structurally prepared for a later bare-metal migration.

---

## 23. Implementation order

The planned implementation order is:

### Step 1: document the architecture

Create and review this HAL design document.

### Step 2: add the GPIO HAL

Create:

```text
include/hal_gpio.h
src/hal_gpio_arduino.cpp
```

Verify that the project still compiles before any module uses the new interface.

### Step 3: add the time and critical-section HAL

Create:

```text
include/hal_time.h
include/hal_critical.h
src/hal_time_arduino.cpp
src/hal_critical_avr.c
```

Verify compilation.

### Step 4: migrate the HX711 platform adapter

Replace the Arduino-specific HX711 adapter with a C adapter that calls:

```text
hal_gpio
hal_time
hal_critical
```

Verify raw readings and normal weight measurement.

### Step 5: migrate the LED hardware module

Replace direct GPIO access in the LED module.

Verify all three LED states and the off state.

### Step 6: migrate the button module

Replace direct GPIO and timing access in the button module.

Verify debounce and tare-button behaviour.

### Step 7: clean up remaining migrated dependencies

Search the migrated modules for direct references to:

```text
pinMode
digitalRead
digitalWrite
millis
delayMicroseconds
```

Any remaining use must be intentional and documented.

### Step 8: document validation

Record the final architecture, physical test results and remaining Arduino dependencies.

---

## 24. Planned commits

The branch should use small, focused commits.

Suggested commits are:

```text
docs: define project HAL architecture
```

```text
feat: add GPIO HAL with Arduino backend
```

```text
feat: add time and critical-section HAL
```

```text
refactor: route HX711 platform through project HAL
```

```text
refactor: use GPIO HAL in indicator LEDs
```

```text
refactor: use project HAL in button module
```

```text
docs: record project HAL validation
```

Additional commits may be added if a change becomes too large to remain understandable.

---

## 25. Validation plan

### Build validation

After every commit:

```text
Clean build succeeds.
Normal build succeeds.
Firmware uploads successfully when physical testing is required.
```

### HX711 validation

Verify that:

* Initialization succeeds.
* Initial tare succeeds.
* Raw readings remain coherent.
* Weight readings remain coherent.
* A known load changes the result correctly.
* Removing the load returns near zero.
* No new timeouts occur.
* Calibration remains valid.

### LED validation

Verify that:

* All LEDs initialize correctly.
* Low level activates the expected LED.
* Medium level activates the expected LED.
* High level activates the expected LED.
* Reset or unknown state turns the LEDs off.
* No LED polarity has changed.

### Button validation

Verify that:

* The input pull-up remains enabled.
* An idle button is not interpreted as pressed.
* One physical press produces one logical press event.
* Contact bounce does not produce repeated events.
* Tare still works.
* Calibration-button behaviour remains unchanged.

### Regression validation

Verify that:

* Persistent calibration still loads.
* The serial calibration workflow still works.
* Weight calculations are unchanged.
* LED thresholds and hysteresis are unchanged.
* Startup behaviour is unchanged.
* The application remains responsive.

---

## 26. Design principles

The HAL will follow these principles:

### Keep interfaces small

Only add operations required by existing code.

### Prefer C-compatible APIs

Use functions, fixed-width types and explicit data types.

### Keep platform code isolated

Only backend files should include platform-specific headers.

### Preserve behaviour during refactoring

Changing architecture must not silently change application logic.

### Avoid dynamic allocation

The project does not require heap allocation for HAL operations.

### Avoid premature generalization

The HAL is designed for this project first.

It may later support other projects, but current requirements take priority over hypothetical features.

### Keep drivers independent

A device driver should not directly depend on application logic.

Where useful, a driver-specific platform adapter may translate between the driver and the general HAL.

---

## 27. Known design limitations

The initial HAL has deliberate limitations:

* GPIO pin identifiers use an 8-bit representation.
* GPIO operations do not currently return detailed error codes.
* GPIO interrupt support is not included.
* Analog input support is not included.
* PWM support is not included.
* Timer configuration is not included.
* Serial communication is not included.
* EEPROM access is not included.
* The critical-section backend is AVR-specific.
* The time backend still depends on Arduino.
* The application still uses the Arduino runtime.
* No automated HAL tests are currently available.

These limitations are acceptable for the current branch.

---

## 28. Future work

Possible future branches include:

```text
feature/c-module-migration
feature/hal-serial
feature/hal-storage
feature/avr-gpio-backend
feature/avr-timebase
feature/bare-metal-avr
feature/hal-tests
```

The likely migration path is:

```text
1. Introduce project HAL interfaces.
2. Route existing modules through the HAL.
3. Add automated tests where practical.
4. Replace Arduino GPIO with an AVR backend.
5. Replace Arduino timing with an AVR timer implementation.
6. Abstract serial communication.
7. Abstract persistent storage.
8. Replace setup() and loop() with a custom entry point.
9. Remove framework = arduino.
```

---

## 29. Definition of done

This branch will be complete when:

* [ ] The HAL architecture is documented.
* [ ] GPIO HAL interfaces exist.
* [ ] The Arduino GPIO backend exists.
* [ ] Time HAL interfaces exist.
* [ ] The Arduino time backend exists.
* [ ] Critical-section HAL interfaces exist.
* [ ] The AVR critical-section backend exists.
* [ ] Public HAL headers are C-compatible.
* [ ] Public HAL headers contain no Arduino dependencies.
* [ ] The HX711 platform adapter uses the project HAL.
* [ ] The HX711 protocol driver remains functionally unchanged.
* [ ] The physical LED module uses the GPIO HAL.
* [ ] The button module uses the GPIO and time HAL.
* [ ] Weight measurement still works.
* [ ] Initial and physical tare still work.
* [ ] Persistent calibration still works.
* [ ] LED level indication still works.
* [ ] Button debounce still works.
* [ ] The serial calibration workflow still works.
* [ ] The project builds and uploads successfully.
* [ ] Remaining direct Arduino dependencies are documented.
* [ ] Final physical validation is documented.

## 30. Final implementation

The project HAL has been implemented and integrated into the modules included in the scope of this branch.

The final HAL files are:

```text
include/
├── hal_gpio.h
├── hal_time.h
└── hal_critical.h

src/
├── hal_gpio_arduino.cpp
├── hal_time_arduino.cpp
└── hal_critical_avr.c
```

The project also contains the HX711-specific adapter:

```text
src/
├── hx711_platform.h
└── hx711_platform.c
```

The HX711 adapter now uses the project HAL and no longer calls Arduino or AVR functions directly.

---

## 31. Implemented HAL interfaces

### GPIO HAL

The GPIO HAL provides:

```c
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

The current backend translates these operations into Arduino GPIO functions.

### Time HAL

The time HAL provides:

```c
uint32_t hal_time_millis(void);

void hal_time_delay_us(
    uint16_t microseconds
);
```

The current backend translates these operations into Arduino timing functions.

### Critical-section HAL

The critical-section HAL provides:

```c
hal_critical_state_t hal_critical_enter(void);

void hal_critical_exit(
    hal_critical_state_t previous_state
);
```

The current AVR backend saves the complete AVR status register, disables interrupts and later restores the exact previous state.

It does not enable interrupts unconditionally.

---

## 32. Migrated modules

### HX711 platform adapter

The previous Arduino-specific adapter:

```text
hx711_platform_arduino.cpp
```

was replaced by:

```text
hx711_platform.c
```

The new dependency chain is:

```text
hx711_driver.c
        |
        v
hx711_platform.c
        |
        v
hal_gpio
hal_time
hal_critical
        |
        v
Arduino and AVR backends
```

The HX711 protocol implementation itself did not need to be rewritten.

### Indicator LEDs

The physical LED module now uses:

```text
hal_gpio_configure_output()
hal_gpio_write()
```

It no longer calls:

```text
pinMode()
digitalWrite()
```

directly.

LED polarity and visible behaviour remain unchanged.

### Button

The button module now uses:

```text
hal_gpio_configure_input_pullup()
hal_gpio_read()
hal_time_millis()
```

It no longer directly uses Arduino GPIO or timing functions.

The existing debounce, short-press and long-hold behaviour remains unchanged.

### Level indicator

The level indicator now uses:

```text
hal_time_millis()
```

for the very-low-level warning blink.

The level thresholds, hysteresis and state transitions remain unchanged.

### Operation indicator

The operation indicator now uses:

```text
hal_time_millis()
```

for normal, success and error patterns.

The pattern timing and LED sequences remain unchanged.

### Application timing

The application now uses:

```text
hal_time_millis()
```

for periodic weight output.

The application still includes Arduino because serial communication and flash-string support have not yet been abstracted.

---

## 33. Type alignment

The project timing values now consistently use:

```c
uint32_t
```

This includes:

* HAL millisecond values.
* Button debounce timing.
* Button hold timing.
* Periodic output timing.
* Level-indicator timing.
* Operation-indicator timing.
* Configuration constants expressed in milliseconds.

Unsigned subtraction continues to be used for elapsed-time calculations:

```c
elapsed = current_time - start_time;
```

This preserves correct behaviour across the normal overflow of the 32-bit millisecond counter.

GPIO configuration constants remain represented with standard integer types in the project configuration.

The HAL currently defines:

```c
typedef uint8_t hal_gpio_pin_t;
```

No explicit conversion is required when the source value is already represented as a `uint8_t`.

---

## 34. Remaining direct Arduino dependencies

The Arduino framework has not been removed.

The remaining direct Arduino dependencies are intentional and outside the scope of this branch.

### `main.cpp`

Still depends on the Arduino runtime for:

```text
setup()
loop()
```

### `app.cpp`

Still depends on Arduino for:

```text
Serial
F()
```

Its timing operations now use the project HAL.

### `calibration_storage.cpp`

Still depends on the Arduino EEPROM implementation.

### `hal_gpio_arduino.cpp`

Provides the temporary GPIO backend using:

```text
pinMode()
digitalRead()
digitalWrite()
```

### `hal_time_arduino.cpp`

Provides the temporary time backend using:

```text
millis()
delayMicroseconds()
```

These remaining dependencies will be addressed by future branches.

---

## 35. AVR-specific dependency

The current critical-section backend is:

```text
hal_critical_avr.c
```

It directly uses AVR facilities such as:

```text
SREG
cli()
```

This dependency is intentional.

The public interface remains platform-independent, while the implementation is explicitly identified as AVR-specific.

A future STM32 or other microcontroller backend would implement the same public interface using the interrupt-control mechanism of that architecture.

---

## 36. Final validation

A clean build was performed after completing the migration.

The firmware was compiled, uploaded and tested on the Arduino Nano.

The following functionality was verified:

* Normal application startup.
* HX711 initialization.
* Initial tare.
* Weight measurement.
* Weight response when applying a known load.
* Return near zero after removing the load.
* Physical tare-button operation.
* Button debounce.
* Long-hold calibration entry.
* Very-low-level blinking indication.
* Low, medium and high level indication.
* Operation-indicator patterns.
* Periodic serial weight output.
* Persistent calibration loading after restart.
* Normal HX711 operation through the new HAL dependency chain.

No functional regression was observed during the migration.

---

## 37. Final architecture

The resulting architecture is:

```text
Application
    |
    v
Application modules
    |
    +-------------------------+
    |                         |
    v                         v
HX711 driver            Project modules
    |                         |
    v                         |
HX711 platform adapter       |
    |                         |
    +------------+------------+
                 |
                 v
          Project HAL
                 |
       +---------+----------+
       |         |          |
       v         v          v
 Arduino GPIO  Arduino time  AVR critical
    backend      backend       backend
       |         |          |
       +---------+----------+
                 |
                 v
            ATmega328P
```

The project still uses Arduino, but migrated modules no longer depend directly on Arduino GPIO or time functions.

---

## 38. Future backend replacement

A future custom AVR implementation should replace:

```text
hal_gpio_arduino.cpp
hal_time_arduino.cpp
```

with files such as:

```text
hal_gpio_avr.c
hal_time_avr.c
```

Ideally, the following files should remain unchanged:

```text
hal_gpio.h
hal_time.h
hal_critical.h
hx711_driver.c
hx711_platform.c
button.cpp
indicator_leds.cpp
level_indicator.cpp
operation_indicator.cpp
scale.cpp
```

Later branches will also need to address:

```text
Serial communication
persistent storage
Arduino startup and runtime
```

Only after those dependencies have been replaced can:

```ini
framework = arduino
```

be removed safely.

---

## 39. Definition of done

* [x] The HAL architecture is documented.
* [x] GPIO HAL interfaces exist.
* [x] The Arduino GPIO backend exists.
* [x] Time HAL interfaces exist.
* [x] The Arduino time backend exists.
* [x] Critical-section HAL interfaces exist.
* [x] The AVR critical-section backend exists.
* [x] Public HAL headers are compatible with C and C++.
* [x] Public HAL headers contain no Arduino dependencies.
* [x] The HX711 platform adapter uses the project HAL.
* [x] The HX711 protocol implementation remains independent of Arduino.
* [x] The physical LED module uses the GPIO HAL.
* [x] The button module uses the GPIO and time HAL.
* [x] The level indicator uses the time HAL.
* [x] The operation indicator uses the time HAL.
* [x] Application periodic timing uses the time HAL.
* [x] Weight measurement still works.
* [x] Initial and physical tare still work.
* [x] Persistent calibration still works.
* [x] LED level indication still works.
* [x] Very-low-level blinking still works.
* [x] Button debounce still works.
* [x] Long-hold detection still works.
* [x] The serial calibration workflow still works.
* [x] The project builds and uploads successfully.
* [x] Remaining direct Arduino dependencies are documented.
* [x] Final physical validation is documented.
* [ ] Custom AVR GPIO backend.
* [ ] Custom AVR time backend.
* [ ] Serial HAL.
* [ ] Persistent-storage HAL.
* [ ] Removal of the Arduino runtime.
