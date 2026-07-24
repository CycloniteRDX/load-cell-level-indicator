# Native Unit Test Notes

## 1. Purpose

The purpose of this branch is to introduce automated native unit tests for project modules that can run without the Arduino Nano or physical hardware.

The tests will run on the development computer using:

```text
PlatformIO Native
GCC
Unity
```

The initial objective is to create a reliable test infrastructure before replacing additional Arduino functionality or implementing direct AVR hardware access.

---

## 2. Why native tests are useful

Physical testing remains necessary for:

* Electrical connections.
* Real HX711 timing.
* Load-cell noise.
* Mechanical behaviour.
* LED wiring.
* Button wiring.
* EEPROM persistence on the board.

However, many software behaviours can be tested without hardware:

* Button debounce.
* Long-press detection.
* Millisecond-counter overflow.
* Level thresholds.
* Level hysteresis.
* LED pattern logic.
* HX711 bit reconstruction.
* HX711 sign extension.
* HX711 timeout handling.
* HX711 channel and gain pulse counts.
* Invalid argument handling.

Native tests allow these behaviours to be checked quickly and repeatedly.

---

## 3. Test architecture

The intended test architecture is:

```text
Module under test
        |
        v
Project HAL interfaces
        |
        v
Fake HAL used by tests
```

For example:

```text
button.cpp
    |
    +--> hal_gpio_read()
    |
    +--> hal_time_millis()
              |
              v
        simulated values
```

The module under test should not need to know whether it is using:

```text
real Arduino hardware
or
a simulated native backend
```

---

## 4. Test framework

The project will use Unity as the initial testing framework.

Unity was selected because:

* It supports C and C++.
* It runs on the native development computer.
* It can also run on constrained embedded targets.
* It has low overhead.
* Its assertions are suitable for fixed-width integer types.
* It does not require C++ classes.

The test code may be compiled as C++ when the module under test is currently implemented in C++.

---

## 5. Native environment

A new PlatformIO environment will be added:

```ini
[env:native]
platform = native
test_framework = unity
```

The native environment uses the GCC toolchain installed on the development computer.

It must not compile Arduino-specific files such as:

```text
main.cpp
app.cpp
calibration_storage.cpp
hal_gpio_arduino.cpp
hal_time_arduino.cpp
hal_critical_avr.c
```

Only the source files required by each native test should be included.

---

## 6. Fake HAL

Tests will provide fake implementations of the public HAL interfaces.

The fake GPIO implementation should support:

* Configuring inputs.
* Configuring pull-up inputs.
* Configuring outputs.
* Setting simulated input levels.
* Reading output levels.
* Recording GPIO operations.

The fake time implementation should support:

* Setting the current simulated time.
* Advancing time by a chosen number of milliseconds.
* Simulating millisecond-counter overflow.
* Recording microsecond delays where required.

The fake critical-section implementation should support:

* Recording entry into critical sections.
* Recording restoration of the previous state.
* Checking that critical sections are balanced.

---

## 7. Initial test order

Tests will be introduced incrementally.

### Button tests

The first tested module will be the button module.

Planned cases include:

* Null pointer handling.
* Pull-up configuration during initialization.
* Initial released state.
* Initial pressed state.
* Press before debounce completion.
* Press after debounce completion.
* Contact bounce.
* Only one event per physical press.
* Release handling.
* Long-press detection.
* Long press reported only once.
* New long press after release.
* Millisecond-counter overflow.

### HX711 driver tests

Planned cases include:

* Invalid arguments.
* Initialization.
* Readiness.
* Timeout.
* Positive 24-bit values.
* Negative 24-bit values.
* Sign extension.
* Most-significant-bit-first reconstruction.
* Gain pulse counts.
* Critical-section restoration.
* Power-down.
* Power-up.

### Level-indicator tests

Planned cases include:

* Very-low level.
* Low level.
* Medium level.
* High level.
* Threshold boundaries.
* Hysteresis.
* Direct transitions between distant levels.
* Very-low warning blink timing.
* Millisecond-counter overflow.

### Operation-indicator tests

Planned cases include:

* Idle state.
* Tare indication.
* Calibration-zero indication.
* Calibration-mass indication.
* Success pattern.
* Error pattern.
* Return to the previous indication.
* Correct blink counts and periods.

---

## 8. Test isolation

Each test should:

* Start from a known fake-HAL state.
* Avoid depending on the result of a previous test.
* Initialize its own module state.
* Control simulated time explicitly.
* Use explicit expected values.
* Avoid real delays.
* Avoid access to physical hardware.
* Avoid dynamic memory allocation.

The Unity `setUp()` function will reset shared fake state before each test.

---

## 9. Source-code strategy

The project source code currently remains in `src/`.

For this branch, PlatformIO may temporarily use:

```ini
test_build_src = yes
```

together with source filters so that only the module required by a particular native test is compiled.

A future structural branch may move reusable modules into private components under `lib/`.

That reorganization is outside the scope of this branch.

---

## 10. Non-objectives

This branch will not:

* Replace Arduino backends with AVR backends.
* Remove the Arduino framework.
* Add direct register access.
* Rewrite production modules solely to simplify tests.
* Test physical electrical behaviour.
* Test real load-cell accuracy.
* Simulate EEPROM initially.
* Simulate serial communication initially.
* Add continuous integration initially.
* Convert all C++ modules to C.
* Reorganize the complete source tree.

Production-code changes should only be made when a test reveals a real defect or an unavoidable testability problem.

---

## 11. Planned file structure

The expected initial structure is:

```text
test/
├── support/
│   ├── fake_hal.h
│   └── fake_hal.cpp
│
├── test_button/
│   └── test_main.cpp
│
├── test_hx711_driver/
│   └── test_main.c
│
├── test_level_indicator/
│   └── test_main.cpp
│
└── test_operation_indicator/
    └── test_main.cpp
```

The exact structure may be adjusted as the fake HAL develops.

---

## 12. Planned commits

```text
docs: define native unit test strategy
```

```text
test: add native Unity environment
```

```text
test: add fake GPIO and time HAL
```

```text
test: cover button debounce and hold detection
```

```text
test: cover HX711 driver protocol
```

```text
test: cover level indicator behaviour
```

```text
test: cover operation indicator patterns
```

```text
docs: record native unit test coverage
```

---

## 13. Validation commands

Native tests will be executed using:

```text
pio test -e native
```

A specific suite may be executed using:

```text
pio test -e native --filter test_button
```

The existing Arduino firmware must continue to compile separately:

```text
pio run -e nanoatmega328new
```

Native tests must not interfere with normal firmware compilation or upload.

---

## 14. Definition of done

This branch will be complete when:

* [ ] The native PlatformIO environment exists.
* [ ] Unity runs successfully on the development computer.
* [ ] Fake GPIO support exists.
* [ ] Fake time support exists.
* [ ] Fake critical-section support exists where required.
* [ ] Button debounce is tested.
* [ ] Button long-hold detection is tested.
* [ ] HX711 raw data reconstruction is tested.
* [ ] HX711 sign extension is tested.
* [ ] HX711 timeout handling is tested.
* [ ] HX711 gain pulse generation is tested.
* [ ] Level thresholds are tested.
* [ ] Level hysteresis is tested.
* [ ] Very-low blinking is tested.
* [ ] Operation-indicator patterns are tested.
* [ ] Millisecond-counter overflow is tested.
* [ ] Native tests pass.
* [ ] Arduino firmware still builds.
* [ ] Physical firmware behaviour remains unchanged.
* [ ] Final test coverage is documented.
