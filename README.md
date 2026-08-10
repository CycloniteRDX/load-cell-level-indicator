# Load Cell Level Indicator

A bare-metal ATmega328P firmware project that measures the weight of a container through an HX711 load-cell ADC and represents the measured level with three LEDs.

The project began as a small Arduino learning exercise and was progressively evolved into a modular, tested firmware with project-owned drivers, hardware-abstraction layers, persistent calibration and tare records, native unit tests and a direct AVR entry point.

> **Project status:** validated functional prototype
> **Latest completed milestone:** `v1.2-non-blocking-application`
> **Current development milestone:** `v1.3-fault-recovery-and-watchdog`

## Features

- Load-cell measurement through a project-owned HX711 driver.
- Persistent tare offset restored after reset or power loss.
- Explicit `TARE_REQUIRED` state when no valid tare exists.
- Deliberate physical tare through a three-second button hold.
- Immediate service tare through the serial console.
- In-place calibration using a known reference mass.
- Persistent calibration factor stored in the ATmega328P EEPROM.
- Independent, versioned and CRC-protected calibration and tare records.
- Persistent loads distinguish valid, absent, corrupt and access-error outcomes.
- Four logical level states represented by three LEDs:
  - `VERY_LOW`
  - `LOW`
  - `MEDIUM`
  - `HIGH`
- Hysteresis around level thresholds to prevent rapid switching.
- Visual feedback for tare-required, tare, calibration, success and error states.
- Debounced physical buttons with short-press and hold events.
- Serial console with fixed-point number formatting.
- Cooperative application state machine for startup, tare and calibration.
- Incremental HX711 sampling with at most one ready conversion per update.
- Status-rich scale reads that distinguish no data from driver failure.
- Bounded HX711 power-cycle primitive that preserves active tare and calibration.
- Stable application fault codes with explicit recovery or terminal policy.
- Finite 2000 ms health deadline for missing HX711 conversions during normal operation.
- Cooperative HX711 recovery with 500 ms backoff, a 2000 ms ready deadline and three attempts.
- Recovery discards inputs, cancels unfinished operations and returns only to a safe boundary state.
- Distinct recovery indication with the LOW and HIGH LEDs alternating every 250 ms.
- Responsive UART and button handling during 20-sample operations.
- Direct AVR implementations for GPIO, Timer1, EEPROM and USART0.
- Project-owned bare-metal `main()`; the production build does not use Arduino Core.
- 300 native Unity tests across 12 suites.

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
| Button debounce | 40 ms |
| Physical tare hold | 3000 ms |
| Calibration-start hold | 3000 ms |
| Weight output period | 500 ms |
| HX711 startup timeout | 2000 ms |
| HX711 runtime no-data timeout | 2000 ms |
| Multi-sample operation timeout | 5000 ms |
| Recovery backoff | 500 ms |
| Recovery ready timeout | 2000 ms |
| Recovery attempts | 3 |
| Recovery LED alternation | 250 ms |
| Tare samples | 20 |
| Normal weight reads per update | At most 1 |
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
| Tare required | All LEDs blink together slowly |
| Tare in progress | All LEDs on |
| Waiting for calibration zero | Low LED blinking |
| Waiting for reference mass | Medium LED blinking |
| Successful operation | All LEDs flash twice |
| Operation error | High LED flashes three times |
| HX711 recovery | Low and high LEDs alternate every 250 ms |
| Terminal fault | High LED blinks persistently |

While `TARE_REQUIRED` is active, normal level indication is disabled because the firmware does not have a trustworthy operational zero.

## Controls

### Physical buttons

- **D4 short press during normal operation:** ignored.
- **D4 hold for approximately 3 seconds:** perform and persist tare.
- **D4 during active tare or calibration:** cancel immediately.
- **D8 hold for approximately 3 seconds:** start calibration.
- **D8 short press during calibration:** confirm the current calibration step.

A D4 press used to cancel an operation is suppressed until release, so the same
physical press cannot later be reinterpreted as a tare hold.

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
| `t` | Perform and persist tare immediately |
| `c` | Start or confirm calibration |
| `q` | Cancel an active tare or calibration operation |
| `s` | Save the active calibration factor |
| `x` | Clear only the stored calibration record |
| `z` | Clear only the stored tare record and enter `TARE_REQUIRED` |

No newline is required. When several bytes are pending, the application processes the first command and discards the remaining queued input.

Commands received while sampling or temporary result patterns are active receive
an immediate state-specific response and are discarded, so they cannot execute
later in a different state. `q` remains available to cancel active tare or
calibration sampling.

## Startup sequence

After reset, the production firmware:

1. Enters the project-owned AVR `main()`.
2. Enables global interrupts.
3. Initializes Timer1, USART0, buttons and LEDs.
4. Initializes the HX711.
5. Returns from initialization and polls HX711 readiness from `app_update()`.
6. Records fault `02` and starts bounded cooperative recovery if the first conversion is not ready within 2000 ms.
7. Power-cycles the HX711 after each 500 ms backoff and waits cooperatively for a conversion for at most 2000 ms.
8. Retries at most three times, then enters a terminal reset-required fault if the sensor does not recover.
9. Loads and classifies the persistent calibration record after a successful startup recovery.
10. Uses a valid stored calibration, or the compiled default when the record is absent or corrupt.
11. Loads and classifies the persistent tare record.
12. Applies the stored offset and enters normal measurement when the tare is valid.
13. Enters `TARE_REQUIRED` when the tare record is absent or corrupt.
14. Enters terminal `FAULT 09` when either record cannot be read from persistent storage.

The firmware does **not** automatically tare during startup.

A restart while the container is partially filled therefore restores the previous operational zero instead of redefining the current load as zero.

### First startup after upgrading from `v1.0`

`v1.0` stored only the calibration record. Its calibration data remains in EEPROM addresses 0–11.

`v1.1` adds the tare record in addresses 12–23. On the first startup after upgrading, no valid tare record exists yet, so the firmware enters `TARE_REQUIRED`.

Place the empty permanent container on the platform and either:

```text
hold D4 for approximately 3 seconds
```

or send:

```text
t
```

The new tare is then stored and restored on later resets and power cycles.

## Tare procedure

The preferred operational zero is:

```text
platform + empty permanent container
```

To establish or replace it:

1. Place the empty permanent container on the platform.
2. Wait for the mechanical assembly to become stable.
3. Hold D4 for approximately three seconds, or send `t`.
4. Wait for the incremental HX711 sampling and EEPROM verification to complete.
5. Confirm that normal measurement resumes.

A short D4 press cannot redefine zero.

During sampling the superloop continues updating inputs and indicators. A new
D4 press or serial `q` cancels the operation before the next sample. If HX711
sampling, its total timeout, EEPROM saving or verification fails, the previous
offset remains active.

## Calibration procedure

The current calibration uses a reference mass of **1500 g**. Change `CALIBRATION_MASS_GRAMS` before calibrating with a different mass.

Calibration may be started from normal operation or from `TARE_REQUIRED`.

### Using the physical controls

1. Hold the D8 calibration button for three seconds.
2. Leave only the empty platform and permanent container in place.
3. Wait for mechanical stability.
4. Press D8 to confirm zero.
5. The new tare is measured, saved and verified before calibration advances.
6. Place the configured reference mass on the scale.
7. Wait for stability.
8. Press D8 again.
9. The firmware calculates, validates, applies and stores the new factor in EEPROM.

Press D4 at any calibration stage to cancel.

If calibration is cancelled after the zero stage, the newly confirmed tare remains stored while the previous calibration factor remains active.

### Using the serial console

The same sequence can be completed with:

```text
c → start calibration
c → confirm and persist zero
c → calculate and save using the reference mass
q → cancel
```

A successful calibration is saved automatically.

The zero and reference-mass sample sets are collected cooperatively. Each
`app_update()` reads at most one already-ready conversion, so cancellation and
state-specific UART responses remain available between samples.

The `s` command stores the currently active factor manually.

The `x` command invalidates only the stored calibration record. The active factor remains in use until the next restart, when the firmware falls back to the default factor unless another valid calibration record has been saved.

The `z` command invalidates only the stored tare record, disables normal level indication and enters `TARE_REQUIRED` immediately.

## Persistent EEPROM layout

```text
EEPROM 0–11
    calibration record

EEPROM 12–23
    tare record

EEPROM 24 onward
    unused
```

Both records use explicit fixed-size binary layouts, format versions and CRC-16/CCITT validation.

Calibration and tare are independent physical properties:

```text
calibration = ADC counts per gram
tare        = raw ADC count at operational zero
```

Clearing or replacing one record does not modify the other.

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
    +--> calibration_record.cpp
    +--> calibration_storage.cpp
    +--> tare_record.cpp
    +--> tare_storage.cpp
    +--> storage_layout.h
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
├── docs/       Detailed milestone, validation and architecture notes
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

Run the complete regression:

```powershell
.\scripts\run-native-tests.ps1
```

The individual environments are:

```powershell
pio test -e native_button
pio test -e native_hx711
pio test -e native_level_indicator
pio test -e native_operation_indicator
pio test -e native_scale
pio test -e native_app_fault
pio test -e native_app
pio test -e native_tare_record
pio test -e native_tare_storage
pio test -e native_calibration_storage
pio test -e native_console
pio test -e native_time_delay
```

Validated test inventory:

| Environment | Tests |
|---|---:|
| `native_button` | 11 |
| `native_hx711` | 18 |
| `native_level_indicator` | 14 |
| `native_operation_indicator` | 18 |
| `native_scale` | 35 |
| `native_app_fault` | 5 |
| `native_app` | 69 |
| `native_tare_record` | 20 |
| `native_tare_storage` | 21 |
| `native_calibration_storage` | 40 |
| `native_console` | 43 |
| `native_time_delay` | 6 |
| **Total** | **300** |

Validated result:

```text
Suites passed: 12/12
Tests passed:  300/300
Failures:      0
Exit code:     0
```

## Production memory usage

For the direct AVR production environment at `v1.2`:

```text
Static SRAM: 217 bytes of 2048 bytes (10.6%)
Flash:       16898 bytes of 30720 bytes (55.0%)
```

For the controlled Arduino entry-point reference environment:

```text
Static SRAM: 226 bytes of 2048 bytes (11.0%)
Flash:       17194 bytes of 30720 bytes (56.0%)
```

The Arduino reference currently uses:

```text
9 additional SRAM bytes
296 additional Flash bytes
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
v1.0-functional-prototype
v1.1-safe-startup-tare
v1.2-non-blocking-application
```

These tags preserve the progressive learning path from an Arduino/Bogde
prototype to a direct AVR firmware with restart-safe persistence and
cooperative application states.

## Detailed documentation

Start with:

- [`docs/project-seed.md`](docs/project-seed.md) — completed project history and architecture.
- [`docs/project-roadmap.md`](docs/project-roadmap.md) — active continuation plan after `v1.2`.
- [`docs/v1.2-release-notes.md`](docs/v1.2-release-notes.md) — non-blocking application release summary.
- [`docs/v1.2-non-blocking-application-validation.md`](docs/v1.2-non-blocking-application-validation.md) — native, build and physical validation for `v1.2`.
- [`docs/non-blocking-application-notes.md`](docs/non-blocking-application-notes.md) — cooperative state-machine design and input policy.
- [`docs/v1.1-release-notes.md`](docs/v1.1-release-notes.md) — release summary and upgrade behaviour.
- [`docs/v1.1-safe-startup-tare-validation.md`](docs/v1.1-safe-startup-tare-validation.md) — native, build and physical validation.
- [`docs/safe-startup-tare-notes.md`](docs/safe-startup-tare-notes.md) — selected design and failure policy.
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

- One EEPROM slot is used per persistent record.
- No EEPROM wear levelling.
- Thresholds and the reference calibration mass are compile-time constants.
- No stable-weight detector or advanced outlier rejection.
- No hardware watchdog is active yet.
- No brown-out reset diagnosis.
- Serial service commands do not require confirmation.
- EEPROM operations and console transmission remain bounded synchronous operations.
- Physical HX711 power-down and power-up validation remains pending separately.
- No 24 V output-driver hardware is included in this repository.
- The current hardware target is only the ATmega328P Nano.
- The current measurement backend is only the HX711.

## Roadmap after `v1.2`

The next firmware milestone is expected to address fault handling and watchdog
policy. Lesson 20 in the separate study repository will first document the
completed `v1.2` transition.

Later phases include:

1. Add Lesson 20 to the separate educational repository.
2. Add explicit fault categories, diagnostics and watchdog handling.
3. Improve measurement stability and outlier rejection.
4. Design the 24 V power and tower-light driver hardware.
5. Create a first custom PCB.
6. Add alternative scale backends such as NAU7802 or ADS1232.
7. Add LoRa communication after the local system is stable.

See [`docs/project-roadmap.md`](docs/project-roadmap.md) for the active plan.

## Safety and validation

Before connecting the project to higher-voltage supplies, inductive loads or industrial tower lights:

- Verify the power architecture independently.
- Use suitable regulation and transient protection.
- Use transistor or MOSFET drivers rather than MCU pins.
- Include flyback protection for inductive loads.
- Confirm grounding and isolation requirements.
- Validate the assembly with current-limited bench supplies.

The firmware alone does not make a 24 V installation electrically safe.
