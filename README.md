# Load Cell Level Indicator

A bare-metal ATmega328P firmware project that measures the weight of a container through an HX711 load-cell ADC and represents the measured level with three LEDs.

The project began as a small Arduino learning exercise and was progressively evolved into a modular, tested firmware with project-owned drivers, hardware-abstraction layers, persistent calibration, native unit tests and a direct AVR entry point.

> **Project status:** functional prototype / `v1.0` release candidate  
> **Latest completed milestone:** `v0.16-direct-avr-entrypoint`

## Features

- Load-cell measurement through a project-owned HX711 driver.
- Automatic tare during startup.
- Manual tare through a physical button or serial command.
- In-place calibration using a known reference mass.
- Persistent calibration factor stored in the ATmega328P EEPROM.
- Four logical level states represented by three LEDs:
  - `VERY_LOW`
  - `LOW`
  - `MEDIUM`
  - `HIGH`
- Hysteresis around level thresholds to prevent rapid switching.
- Visual feedback for tare, calibration, success and error states.
- Debounced physical buttons.
- Serial console with fixed-point number formatting.
- Direct AVR implementations for GPIO, Timer1, EEPROM and USART0.
- Project-owned bare-metal `main()`; the production build does not use Arduino Core.
- 177 native Unity tests.

## Hardware

The current prototype uses:

- Arduino Nano with ATmega328P at 16 MHz.
- HX711 module.
- Load cell connected to the HX711.
- Two momentary push buttons.
- Three indicator LEDs with suitable current-limiting resistors.
- USB connection for programming and the 115200-baud console.

The pin assignments are defined in [`src/config.h`](src/config.h).

## Pinout

| Nano pin | Function | Connection notes |
|---|---|---|
| D0 / RX | USART0 receive | USB-to-serial console |
| D1 / TX | USART0 transmit | USB-to-serial console |
| D2 | HX711 `DOUT` | HX711 data output |
| D3 | HX711 `SCK` | HX711 clock input |
| D4 | Tare/cancel button | Connect button between D4 and GND |
| D5 | Low-level LED | Active-high output |
| D6 | Medium-level LED | Active-high output |
| D7 | High-level LED | Active-high output |
| D8 | Calibration button | Connect button between D8 and GND |

The two buttons use the ATmega328P internal pull-up resistors:

```text
released = HIGH
pressed  = LOW
```

The LED outputs are active-high. Ordinary indicator LEDs must use current-limiting resistors.

> A 24 V tower light must **not** be connected directly to the Nano pins. Use suitable transistor or logic-level MOSFET driver stages, shared grounding where appropriate, and protection designed for the selected loads.

## Default measurement configuration

The current values are provisional and can be changed in [`src/config.h`](src/config.h).

| Setting | Current value |
|---|---:|
| Startup tare delay | 3000 ms |
| Button debounce | 40 ms |
| Weight output period | 500 ms |
| Tare samples | 20 |
| Weight samples | 1 |
| Calibration samples | 20 |
| Reference calibration mass | 1500 g |
| Default calibration factor | 45.589332 counts/g |
| Level hysteresis | 20 g |

### Level thresholds

| Level | Initial selection |
|---|---|
| `VERY_LOW` | Below 100 g |
| `LOW` | 100 g to below 500 g |
| `MEDIUM` | 500 g to below 1000 g |
| `HIGH` | 1000 g and above |

The state machine applies 20 g of hysteresis around these boundaries after the initial level has been selected.

## LED behaviour

### Normal level display

| State | LED behaviour |
|---|---|
| `VERY_LOW` | Low LED blinking |
| `LOW` | Low LED steady |
| `MEDIUM` | Medium LED steady |
| `HIGH` | High LED steady |

### Operation display

| Operation | LED behaviour |
|---|---|
| Tare in progress | All LEDs on |
| Waiting for calibration zero | Low LED blinking |
| Waiting for reference mass | Medium LED blinking |
| Successful calibration | All LEDs flash twice |
| Calibration error | High LED flashes three times |

## Controls

### Physical buttons

- **D4 short press:** perform tare during normal operation.
- **D4 during calibration:** cancel calibration.
- **D8 hold for 3 seconds:** start calibration.
- **D8 short press during calibration:** confirm the current calibration step.

### Serial console

Open the console at:

```text
115200 baud
8 data bits
no parity
1 stop bit
```

Commands are case-insensitive:

| Command | Action |
|---|---|
| `t` | Perform tare |
| `c` | Start or confirm calibration |
| `q` | Cancel calibration |
| `s` | Save the active calibration factor |
| `x` | Clear the stored calibration record |

No newline is required. When several bytes are pending, the application processes the first command and discards the remaining queued input.

Commands received while blocking operations or temporary result patterns are active are discarded so they are not unexpectedly executed later.

## Startup sequence

After reset, the production firmware:

1. Enters the project-owned AVR `main()`.
2. Enables global interrupts.
3. Initializes Timer1, USART0, buttons and LEDs.
4. Initializes the HX711.
5. Loads a valid calibration factor from EEPROM, or uses the default factor.
6. Waits approximately three seconds.
7. Performs an automatic tare.
8. Enters normal measurement operation.

Leave the scale unloaded, or leave only the empty platform/container in place, during startup.

## Calibration procedure

The current calibration uses a reference mass of **1500 g**. Change `CALIBRATION_MASS_GRAMS` before calibrating with a different mass.

### Using the physical controls

1. Hold the D8 calibration button for three seconds.
2. Remove the measured load and leave only the empty platform or container.
3. Wait for mechanical stability.
4. Press D8 to confirm zero.
5. Place the configured reference mass on the scale.
6. Wait for stability.
7. Press D8 again.
8. The firmware calculates, validates, applies and stores the new factor in EEPROM.

Press D4 at any calibration stage to cancel.

### Using the serial console

The same sequence can be completed with:

```text
c → start calibration
c → confirm zero
c → calculate and save using the reference mass
q → cancel
```

A successful calibration is saved automatically.

The `s` command stores the currently active factor manually.

The `x` command invalidates the stored calibration. The active factor remains in use until the next restart, when the firmware falls back to the default factor unless another valid record has been saved.

## Firmware architecture

```text
Nano bootloader
    |
    v
AVR-LibC startup
    |
    v
main_avr.cpp
    |
    v
app.cpp
    |
    +--> button.cpp
    +--> scale.cpp
    |       |
    |       v
    |    hx711_driver.c
    |
    +--> level_indicator.cpp
    +--> operation_indicator.cpp
    +--> calibration_storage.cpp
    +--> console.c
    |
    +--> hal_gpio_avr.c
    +--> hal_time_avr.c
    +--> hal_time_delay.c
    +--> hal_storage_avr.c
    +--> hal_serial_avr.c
    +--> hal_critical_avr.c
```

The production environment does not declare `framework = arduino`.

An Arduino entry-point environment remains available only as a controlled reference comparison.

## Repository layout

```text
.
├── docs/       Detailed milestone and architecture notes
├── include/    Public C-compatible driver and HAL headers
├── src/        Application, modules, drivers and HAL backends
├── test/       Native Unity test suites and fake backends
└── platformio.ini
```

The flat `src/` layout is intentional for the current project size. It keeps the first project easy to navigate and avoids restructuring without a functional need.

## Requirements

### Production build and upload

- VS Code with the PlatformIO extension, or PlatformIO Core.
- PlatformIO Atmel AVR platform.
- Arduino Nano with the new ATmega328P bootloader configuration.

The production firmware uses AVR-GCC and AVR-LibC but not Arduino Core.

### Native tests

The native test environments require a host C/C++ compiler available in `PATH`.

On Windows, MSYS2 UCRT64 is a suitable option. Typical paths are:

```text
C:\msys64\ucrt64\bin
C:\Users\<user>\.platformio\penv\Scripts
```

After changing the Windows `PATH`, completely restart VS Code so its integrated terminal inherits the new environment.

## Build

The default environment is the direct AVR production build:

```powershell
pio run
```

Equivalent explicit command:

```powershell
pio run -e nanoatmega328new
```

The Arduino entry-point reference build is:

```powershell
pio run -e nanoatmega328new_arduino
```

## Upload

Close any active serial monitor before uploading, because only one process can own the COM port.

```powershell
pio run -t upload
```

Or explicitly:

```powershell
pio run -e nanoatmega328new -t upload
```

## Serial monitor

```powershell
pio device monitor -e nanoatmega328new
```

Stop the monitor with `Ctrl+C` before the next upload.

## Native tests

Run every suite:

```powershell
pio test -e native_button
pio test -e native_hx711
pio test -e native_level_indicator
pio test -e native_operation_indicator
pio test -e native_scale
pio test -e native_calibration_storage
pio test -e native_console
pio test -e native_time_delay
```

Current test inventory:

| Environment | Tests |
|---|---:|
| `native_button` | 10 |
| `native_hx711` | 18 |
| `native_level_indicator` | 14 |
| `native_operation_indicator` | 14 |
| `native_scale` | 32 |
| `native_calibration_storage` | 40 |
| `native_console` | 43 |
| `native_time_delay` | 6 |
| **Total** | **177** |

## Production memory usage

For the direct AVR production environment at `v0.16`:

```text
Static SRAM: 194 bytes
Flash:       11356 bytes
```

For the controlled Arduino entry-point reference environment:

```text
Static SRAM: 203 bytes
Flash:       11664 bytes
```

## Project history

The development was preserved through tagged milestones:

```text
v0.1-minimal-functional
v0.2-structured-bogde
v0.3-persistent-calibration
v0.4-very-low-warning
v0.5-custom-hx711-driver
v0.6-project-hal
v0.7-native-unit-tests
v0.8-avr-gpio-backend
v0.9-avr-timebase
v0.10-native-scale-tests
v0.11-calibration-storage-hal
v0.12-direct-avr-eeprom
v0.13-console-abstraction
v0.14-direct-avr-uart
v0.15-remove-arduino-delay
v0.16-direct-avr-entrypoint
```

These tags preserve the progressive learning path from an Arduino/Bogde prototype to a direct AVR firmware.

## Detailed documentation

Start with:

- [`docs/project-seed.md`](docs/project-seed.md) — complete project history and learning plan.
- [`docs/hx711-driver-notes.md`](docs/hx711-driver-notes.md) — custom HX711 driver.
- [`docs/hal-design-notes.md`](docs/hal-design-notes.md) — hardware-abstraction design.
- [`docs/native-unit-test-notes.md`](docs/native-unit-test-notes.md) — native test strategy.
- [`docs/avr-gpio-backend-notes.md`](docs/avr-gpio-backend-notes.md) — direct GPIO backend.
- [`docs/avr-timebase-notes.md`](docs/avr-timebase-notes.md) — Timer1 timebase.
- [`docs/avr-eeprom-backend-notes.md`](docs/avr-eeprom-backend-notes.md) — direct EEPROM access.
- [`docs/console_abstraction-notes.md`](docs/console_abstraction-notes.md) — console abstraction.
- [`docs/avr-uart-backend-notes.md`](docs/avr-uart-backend-notes.md) — direct USART0 backend.
- [`docs/remove-arduino-delay-notes.md`](docs/remove-arduino-delay-notes.md) — project-owned delay.
- [`docs/avr-entrypoint-notes.md`](docs/avr-entrypoint-notes.md) — direct AVR entry point.

## Current limitations

This is a functional educational prototype, not yet an industrial product.

Current limitations include:

- Tare and calibration sample collection are blocking operations.
- Thresholds and the reference calibration mass are compile-time constants.
- No watchdog or complete fault-recovery strategy.
- No stable-weight detector or advanced outlier rejection.
- No configuration interface beyond the existing commands.
- No 24 V output-driver hardware is included in this repository.
- The current hardware target is only the ATmega328P Nano.
- The current measurement backend is only the HX711.

## Roadmap after `v1.0`

Possible next phases:

1. Convert startup, tare and calibration into non-blocking application states.
2. Add explicit fault states, diagnostics and watchdog handling.
3. Improve measurement stability and outlier rejection.
4. Design the 24 V power and tower-light driver hardware.
5. Create a first custom PCB.
6. Add alternative scale backends such as NAU7802 or ADS1232.
7. Add LoRa communication after the local system is stable.
8. Build a separate educational repository with heavily annotated versions of each milestone.

## Safety and validation

Before connecting the project to higher-voltage supplies, inductive loads or industrial tower lights:

- Verify the power architecture independently.
- Use suitable regulation and transient protection.
- Use transistor or MOSFET drivers rather than MCU pins.
- Include flyback protection for inductive loads.
- Confirm grounding and isolation requirements.
- Validate the assembly with current-limited bench supplies.

The firmware alone does not make a 24 V installation electrically safe.
