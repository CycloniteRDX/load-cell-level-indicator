# Load Cell Level Indicator

A bare-metal ATmega328P firmware project that measures the weight of a container through an HX711 load-cell ADC and represents the measured level with three LEDs.

The project began as a small Arduino learning exercise and was progressively evolved into a modular, tested firmware with project-owned drivers, hardware-abstraction layers, persistent calibration and tare records, native unit tests and a direct AVR entry point.

> **Project status:** validated functional prototype
> **Latest completed milestone:** `v1.3-fault-recovery-and-watchdog`

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
- Deterministic HX711 `DOUT` disconnection detection through an external 10 kΩ pull-up.
- Two-second AVR hardware watchdog fed only after complete application iterations.
- Early application-visible reset-cause capture and startup diagnostics.
- Watchdog reset timing and safe restart physically validated in both firmware environments.
- Responsive UART and button handling during 20-sample operations.
- Direct AVR implementations for GPIO, Timer1, EEPROM, USART0 and watchdog control.
- Project-owned bare-metal `main()`; the production build does not use Arduino Core.
- Dedicated direct-AVR and Arduino-reference watchdog hardware-validation builds.
- 318 native Unity tests across 13 suites.

## Hardware

The current prototype uses:

- Arduino Nano with ATmega328P at 16 MHz.
- HX711 module.
- Load cell connected to the HX711.
- 10 kΩ pull-up resistor between Nano D2 and the shared logic supply.
- Two momentary push buttons.
- Three indicator LEDs with suitable current-limiting resistors.
- USB connection for programming and the 115200-baud console.

The pin assignments are defined in [`src/config.h`](src/config.h).

## Pinout

| Nano pin | Function | Connection notes |
|---|---|---|
| D0 / RX | USART0 receive | USB-to-serial console |
| D1 / TX | USART0 transmit | USB-to-serial console |
| D2 | HX711 `DOUT` | HX711 data output; external 10 kΩ pull-up to the shared logic supply |
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

### HX711 DOUT fail-safe pull-up

The prototype keeps an external 10 kΩ pull-up on the Nano side of the HX711
`DOUT` connection:

```text
Nano +5 V ---- 10 kΩ ----+
                          +---- Nano D2
HX711 DOUT ---------------+
```

The resistor must connect to the shared logic rail: 5 V in the current
prototype, or 3.3 V if both devices use 3.3 V logic. It should remain on the
microcontroller side of the cable or connector so a broken `DOUT` conductor
leaves D2 at a defined HIGH level. Do not connect D2 directly to the supply.

The HX711 protocol uses HIGH for "conversion not ready" and LOW for
"conversion ready". A disconnected `DOUT` therefore produces a deterministic
timeout instead of allowing a floating D2 input to generate false readiness
events and false 24-bit readings. When the HX711 drives LOW, the 10 kΩ resistor
draws approximately 0.5 mA from a 5 V rail.

The ATmega328P internal pull-up is a valid software-controlled alternative. In
a simple Arduino program it is enabled with:

```cpp
pinMode(LOADCELL_DOUT_PIN, INPUT_PULLUP);
```

In this project's HAL-based architecture, the equivalent change would be to
make `hx711_platform_configure_input()` call:

```c
hal_gpio_configure_input_pullup((hal_gpio_pin_t)pin);
```

That same HAL call maps to `pinMode(pin, INPUT_PULLUP)` in the Arduino-reference
backend. In the direct AVR backend it keeps D2/PD2 as an input and sets its
`PORTD` latch bit:

```c
DDRD  &= (uint8_t)~(1U << DDD2);
PORTD |= (uint8_t)(1U << PORTD2);
```

The internal pull-up only exists after firmware configures the pin and its
resistance is less tightly controlled than a selected external resistor. A
reset, bootloader execution or later input reconfiguration may temporarily
disable it. The external 10 kΩ resistor is therefore the selected production
prototype solution. The firmware leaves the internal D2 pull-up disabled; both
pull-ups are not required simultaneously.

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
| Hardware watchdog period | 2 s |
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
| `d` | Start or stop raw diagnostic data capture |

No newline is required. When several bytes are pending, the application processes the first command and discards the remaining queued input.

Commands received while sampling or temporary result patterns are active receive
an immediate state-specific response and are discarded, so they cannot execute
later in a different state. `q` remains available to cancel active tare or
calibration sampling.

### Diagnostic data capture

Diagnostic capture is an opt-in service mode for measurement analysis. It can
start only during normal measurement with a valid tare and is disabled after
every reset.

Send:

```text
d
```

to start a session. The console prints one header followed by one row for every
successful HX711 conversion:

```text
DATA,sequence,timestamp_ms,raw_counts,tare_offset,net_counts,weight_grams
DATA,0,123456,-100000,-170000,70000,1505.500000
```

The values in each row describe the same conversion. The ordinary periodic
`Weight: ... | Level: ...` output is suppressed while capture is active, but
level calculation, LEDs, HX711 supervision, recovery and the watchdog continue
normally.

Send `d` again to stop the session. Starting tare or calibration, clearing the
stored tare, entering fault handling or resetting the MCU also stops capture.
It never restarts automatically and is not stored in EEPROM.

## Startup sequence

After reset, the production firmware:

1. Runs an early `.init3` hook that captures the reset flags still visible in `MCUSR`, clears them and disables an inherited watchdog.
2. Enters the project-owned AVR `main()` and enables global interrupts.
3. Initializes Timer1, USART0, buttons and LEDs.
4. Reports the application-visible reset cause and initializes the HX711.
5. Enables the two-second watchdog only after `app_init()` has established the complete safe application state.
6. Calls `app_update()` repeatedly and feeds the watchdog only after each complete iteration returns.
7. Polls HX711 readiness cooperatively from `app_update()`.
8. Records fault `02` and starts bounded recovery if the first conversion is not ready within 2000 ms.
9. Power-cycles the HX711 after each 500 ms backoff and waits cooperatively for a conversion for at most 2000 ms.
10. Retries at most three times, then enters a terminal reset-required fault if the sensor does not recover.
11. Loads and classifies the persistent calibration record after a successful startup recovery.
12. Uses a valid stored calibration, or the compiled default when the record is absent or corrupt.
13. Loads and classifies the persistent tare record.
14. Applies the stored offset and enters normal measurement when the tare is valid.
15. Enters `TARE_REQUIRED` when the tare record is absent or corrupt.
16. Enters terminal `FAULT 09` when either record cannot be read from persistent storage.

The Nano bootloader may alter `MCUSR` before the application starts. The
firmware therefore reports only the flags visible to the application and uses
`unknown` when no supported flag survives that boot path.

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
    +--> hal_watchdog_avr.c
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

The two intentional-stall validation builds are:

```powershell
pio run -e nanoatmega328new_watchdog_validation
pio run -e nanoatmega328new_arduino_watchdog_validation
```

These environments define `WATCHDOG_HARDWARE_VALIDATION` and include the
otherwise excluded `watchdog_validation.c` module. The two production
environments do neither.

After uploading a validation build, first release D4 and D8. Press both buttons
together to stop the main execution path before its next watchdog kick. The
two-second watchdog must reset the MCU. The trigger starts disarmed after every
reset and cannot fire again until both buttons have been observed released, so
keeping both pressed through the reset does not create a repeated-reset loop.

Physical testing on the real Nano produced watchdog resets after `2.255 s` in
the direct AVR environment and `2.260 s` in the Arduino-reference environment.
Both restarted safely, restored the stored tare and resumed normal measurement.
The Nano bootloader path left no supported `MCUSR` flag visible to the
application, so post-watchdog startup reported `unknown` as designed.

Do not use a validation environment for normal operation. Re-upload
`nanoatmega328new` after the physical test.

### HX711 power-down pulse measurement

The `PD_SCK` power-down pulse generated by the production direct-AVR firmware
was captured on the real Nano and HX711 during the `v1.3` physical validation.
The oscilloscope used a ×10 probe, 20 MHz bandwidth limit, 50 µs/div timebase
and a positive-pulse trigger at 2.5 V.

![HX711 PD_SCK power-down pulse captured during recovery](docs/images/hx711-pd-sck-power-down-pulse.png)

| Measurement | Result |
|---|---:|
| Automatic positive width | `82.49783 µs` |
| HX711 power-down requirement | `>60 µs` |
| Stable HIGH level (`Top`) | `4.91458 V` |
| Stable LOW level (`Base`) | `-2.083 mV` |
| Timing and logic levels | **PASS** |

The measured width includes the requested 70 µs delay plus the execution time
of the surrounding GPIO operations. `Pk-Pk = 5.82292 V` is not used as a logic
level because it includes the brief overshoot and undershoot around the two
edges. `Top` and `Base` represent the stable levels more usefully.

<details>
<summary>Manual level-cursor verification</summary>

![HX711 PD_SCK HIGH and LOW levels checked with oscilloscope cursors](docs/images/hx711-pd-sck-level-cursors.png)

The Y cursors independently place the stable HIGH level near `4.95 V` and the
stable LOW level near `-0.03 V`, for approximately `4.98 V` between them. The X
cursors in this capture are not positioned on the pulse edges; the pulse width
is the automatic `+Width(C1)` result shown at the bottom of both captures.

</details>

This capture validates the electrical timing and levels of one real recovery
pulse. The complete physical fault matrix exercised at least 16 automatic
power-cycle attempts while checking measurement continuity, retry exhaustion,
transactional persistence and input behaviour across recovery.

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
pio test -e native_watchdog_validation
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
| `native_app` | 79 |
| `native_tare_record` | 20 |
| `native_tare_storage` | 21 |
| `native_calibration_storage` | 40 |
| `native_console` | 45 |
| `native_time_delay` | 6 |
| `native_watchdog_validation` | 6 |
| **Total** | **318** |

Validated result:

```text
Suites passed: 13/13
Tests passed:  318/318
Failures:      0
Exit code:     0
```

## Production memory usage

For the direct AVR production environment at `v1.3`:

```text
Static SRAM: 230 bytes of 2048 bytes (11.2%)
Flash:       17738 bytes of 30720 bytes (57.7%)
```

For the controlled Arduino entry-point reference environment:

```text
Static SRAM: 239 bytes of 2048 bytes (11.7%)
Flash:       18032 bytes of 30720 bytes (58.7%)
```

The Arduino reference currently uses:

```text
9 additional SRAM bytes
294 additional Flash bytes
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
v1.3-fault-recovery-and-watchdog
```

These tags preserve the progressive learning path from an Arduino/Bogde
prototype to a direct AVR firmware with restart-safe persistence and
cooperative application states.

## Detailed documentation

Start with:

- [`docs/project-seed.md`](docs/project-seed.md) — completed project history and architecture.
- [`docs/project-roadmap.md`](docs/project-roadmap.md) — active continuation plan after `v1.3`.
- [`docs/measurement-robustness-notes.md`](docs/measurement-robustness-notes.md) — evidence-driven measurement investigation and capture plan.
- [`docs/hx711-prototype-characterization.md`](docs/hx711-prototype-characterization.md) — physical HX711 wiring, coupling, drift and noise findings.
- [`docs/v1.3-release-notes.md`](docs/v1.3-release-notes.md) — fault recovery and watchdog release summary.
- [`docs/v1.3-fault-recovery-and-watchdog-validation.md`](docs/v1.3-fault-recovery-and-watchdog-validation.md) — native, build and physical validation for `v1.3`.
- [`docs/fault-recovery-watchdog-notes.md`](docs/fault-recovery-watchdog-notes.md) — `v1.3` design and incremental implementation record.
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
- The Nano bootloader path does not preserve a supported `MCUSR` cause for the application; post-watchdog startup reports `unknown`.
- Serial service commands do not require confirmation.
- EEPROM operations and console transmission remain bounded synchronous operations.
- Recovery accepts the first ready post-cycle conversion; it does not yet require several coherent measurements before declaring success.
- The DOUT pull-up detects a missing digital connection but not every possible load-cell bridge-wire fault.
- No 24 V output-driver hardware is included in this repository.
- The current hardware target is only the ATmega328P Nano.
- The current measurement backend is only the HX711.

## Roadmap after `v1.3`

The next firmware milestone is expected to improve measurement robustness
using real raw data to justify the selected stability, filtering or outlier
policy. The separate study repository can document `v1.3` after its existing
`v1.2` Lesson 20 work is complete.

Later phases include:

1. Complete Lesson 20 for `v1.2` in the educational repository.
2. Add the later educational lesson for stable `v1.3`.
3. Improve measurement stability and outlier rejection from recorded data.
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
