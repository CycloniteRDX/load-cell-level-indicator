# Project Roadmap and Continuation Guide

## Purpose

This document is the active continuation plan for the project after:

```text
v1.1-safe-startup-tare
```

It is intentionally separate from [`project-seed.md`](project-seed.md).

The two documents have different responsibilities:

- `project-seed.md` records the history, architecture and completed milestones.
- `project-roadmap.md` records future decisions, priorities and the next work to perform.

This separation prevents completed facts from being mixed with plans that may later change.

When development is resumed after a long pause, read:

1. [`../README.md`](../README.md)
2. [`project-seed.md`](project-seed.md)
3. This roadmap
4. The notes for the most recent completed milestone
5. The current Git status, log, branches and tags

---

# Stable baseline

The current stable release is:

```text
v1.1-safe-startup-tare
```

The release contains:

- Direct AVR production entry point.
- No Arduino Core in the production execution path.
- Project-owned HX711 driver.
- Project-owned GPIO, Timer1, delay, EEPROM and USART0 backends.
- Persistent calibration factor.
- Persistent tare offset restored after reset and power loss.
- Explicit `TARE_REQUIRED` state when no valid tare exists.
- Deliberate three-second physical tare.
- Immediate serial service tare.
- Independent calibration and tare clear commands.
- Physical and serial calibration.
- Four logical level states represented by three LEDs.
- 225 native tests with zero failures.
- Complete release and physical-validation documentation.

Production memory usage at the stable baseline:

```text
Static SRAM: 195 bytes
Flash:       13566 bytes
```

The stable tag must never be moved or recreated.

Future work begins after this release and does not alter the historical contents of the tag.

---

# Current repository policy

The main firmware repository remains:

```text
load-cell-level-indicator
```

Its documentation and code should remain in English.

The separate educational repository will be:

```text
load-cell-level-indicator-study
```

Its explanations may be written in Spanish because its primary purpose is detailed learning.

## Branch workflow

For each significant milestone:

```text
main
  |
  +--> feature/<milestone>
             |
             +--> small reviewed commits
             |
             +--> merge into main
             |
             +--> annotated tag when appropriate
             |
             +--> delete the merged branch
```

Use `-d`, not `-D`, when deleting local merged branches.

Do not keep completed `feature/...` or `release/...` branches permanently.

Tags preserve important historical milestones.

## Source-layout policy

Do not reorganize `src/` merely for appearance.

The current flat layout remains appropriate while:

- The number of modules is manageable.
- There is only one production microcontroller target.
- There is only one measurement backend.
- PlatformIO source filters remain easy to understand.

Consider restructuring only when real growth requires it, for example:

- Multiple ADC backends.
- LoRa and other communication modules.
- Multiple microcontroller platforms.
- Clearly separated application, driver and platform families.

Any future layout change should use a dedicated branch such as:

```text
refactor/source-layout
```

and must not be combined with a functional change.

---

# Parallel educational track

## Objective

Maintain the separate repository that explains the project progressively and in depth without adding educational comments or duplicate teaching material to the production firmware.

Repository:

```text
load-cell-level-indicator-study
```

The repository skeleton and its first commit have been created. The next educational work is the detailed project overview followed by the `v0.1` and `v0.2` lessons.

## Initial structure

```text
load-cell-level-indicator-study/
├── README.md
├── lessons/
│   ├── 00-project-overview/
│   ├── 01-minimal-functional/
│   ├── 02-modular-architecture/
│   ├── 03-persistent-calibration/
│   ├── 04-custom-hx711-driver/
│   ├── 05-hardware-abstraction/
│   ├── 06-native-testing/
│   ├── 07-direct-avr-peripherals/
│   └── 08-bare-metal-entrypoint/
├── reference/
│   ├── c-cpp-glossary.md
│   ├── avr-registers.md
│   └── execution-flow.md
└── exercises/
```

Do not copy the complete final firmware into every lesson at once.

Build the educational repository progressively from the stable tags:

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
```

## Educational method

Each lesson should explain:

- What problem existed.
- What changed.
- Why the selected solution was used.
- Which C or C++ concepts appear.
- What happens in memory.
- Which hardware registers are involved.
- Which tests validate the behaviour.
- Which alternative designs were possible.
- Which exercises can reproduce the concept independently.

Important reusable topics include:

- Variables and storage duration.
- `static`.
- `volatile`.
- Pointers.
- Arrays.
- Structures and enumerations.
- Headers and translation units.
- Internal and external linkage.
- C and C++ interoperability.
- Bit masks and register access.
- Interrupt service routines.
- Atomic access.
- Flash, SRAM and EEPROM.
- CRC.
- Debouncing.
- Hysteresis.
- State machines.
- Ring buffers.
- Driver, HAL and application boundaries.

The study repository should not be merged into the production repository.

---

# Completed firmware milestone

## v1.1: safe startup tare

Completed tag:

```text
v1.1-safe-startup-tare
```

Development branch:

```text
feature/safe-startup-tare
```

The milestone removed automatic startup tare and introduced:

- A versioned, CRC-protected persistent tare record.
- A shared non-overlapping EEPROM layout.
- Explicit runtime restoration through `scale_set_offset()`.
- `TARE_REQUIRED` when the persistent offset is absent or invalid.
- Slow all-LED blinking while tare is required.
- Three-second physical tare hold.
- Immediate serial `t` service command.
- Serial `z` command to invalidate only the tare record.
- Transactional rollback after tare-storage failure.
- Persistent calibration-zero tare.
- Protection against reinterpreting a calibration-cancel press as tare.
- 225 passing native tests.
- Physical power-loss validation with a load left in place.

Validated memory usage:

```text
Direct AVR production:
SRAM:  195 bytes
Flash: 13566 bytes

Arduino reference:
SRAM:  204 bytes
Flash: 13862 bytes
```

Detailed records:

```text
docs/safe-startup-tare-notes.md
docs/v1.1-safe-startup-tare-validation.md
docs/v1.1-release-notes.md
```

---

# Next and later firmware milestones

The names and ordering below are provisional. They should remain smaller than a single large redesign.

## v1.2: non-blocking application operations

Convert blocking operations into explicit application states.

Candidates:

- Startup wait.
- Startup decision flow.
- Tare sample collection.
- Calibration sample collection.
- Temporary success and error patterns.
- Some permanent error loops.

Possible state structure:

```text
STARTUP
    |
    +--> LOAD_CONFIGURATION
    +--> TARE_REQUIRED
    +--> NORMAL_OPERATION
    +--> TARING
    +--> CALIBRATION_ZERO
    +--> CALIBRATION_REFERENCE
    +--> RESULT_PATTERN
    +--> FAULT
```

Objectives:

- Buttons remain responsive.
- UART input policy is explicit.
- Indicators continue updating.
- LoRa can later coexist without being starved.
- Long operations can time out or be cancelled cleanly.
- No hidden commands remain queued for later execution.

Do not combine this milestone with watchdog or filtering changes.

## v1.3: fault handling and watchdog

Add explicit runtime fault categories.

Candidates:

- HX711 not ready.
- HX711 timeout.
- Implausible raw reading.
- Saturated ADC.
- Invalid calibration.
- Invalid persistent configuration.
- Internal state inconsistency.

Define:

- Recoverable faults.
- Non-recoverable faults.
- User-visible LED patterns.
- Console diagnostic messages.
- Retry policy.
- Safe output policy.
- Watchdog reset policy.
- Startup reason reporting when feasible.

Only enable the watchdog after the application has a clear fault and recovery model.

## v1.4: measurement robustness

Improve field measurement quality.

Candidates:

- Stable-weight detector.
- Median filter.
- Trimmed mean.
- Outlier rejection.
- Configurable sample window.
- Drift diagnostics.
- Minimum state residence time.
- Mechanical-settling detection.
- Disconnected or saturated load-cell detection.

Begin with the simplest method that solves a measured problem.

Do not add filtering merely because it is theoretically available.

Record raw data from the real installation before selecting the final filter.

## v1.5: field configuration

Move deployment-specific values out of hard-coded development assumptions.

Candidates:

- Level thresholds.
- Hysteresis.
- Reference calibration mass.
- Output period.
- Long-press duration.
- Filter settings.

Possible storage:

- EEPROM configuration record.
- Compile-time defaults plus validated persistent overrides.
- Service-console commands.

This milestone should follow the persistent-tare work so that EEPROM record ownership is designed consistently.

---

# Hardware roadmap

## 24 V power architecture

Design and validate a field power system for:

- 24 V input.
- Arduino Nano or future MCU supply.
- HX711, NAU7802 or ADS1232 analog supply.
- Load-cell excitation.
- 24 V tower-light outputs.
- Future LoRa current peaks.

Topics:

- Input fuse or resettable protection.
- Reverse-polarity protection.
- TVS protection.
- Buck conversion.
- Analog filtering.
- Ground strategy.
- Decoupling.
- MOSFET or transistor output stages.
- Flyback protection for inductive loads.
- Connector selection.
- Current and thermal budget.

Validate the power subsystem separately before connecting the complete firmware.

## Mechanical validation

The final calibration factor and level thresholds should not be considered final until the real mechanical installation exists.

Validate:

- Load-cell mounting.
- Force direction.
- Mechanical preload.
- Cable strain relief.
- Corner or off-axis loading.
- Repeatability.
- Hysteresis.
- Creep.
- Temperature drift.
- Empty-container repeatability.
- Full-container safety margin.

## Custom PCB

Create a custom PCB only after:

- The 24 V architecture is validated.
- The preferred ADC is selected.
- The mechanical wiring is known.
- Output currents are known.
- Connector types are known.
- Protection requirements are known.
- The prototype has been tested under realistic conditions.

The first PCB should prioritize observability and test points over minimum size.

---

# Alternative measurement backends

The current scale architecture should eventually support other converters.

Candidates:

```text
HX711
NAU7802
ADS1232
```

Before adding a backend, define a stable low-level measurement interface.

Possible direction:

```text
scale
    |
    v
load_cell_adc interface
    |
    +--> HX711 backend
    +--> NAU7802 backend
    +--> ADS1232 backend
```

Do not add all backends simultaneously.

Recommended order:

1. Preserve the working HX711 implementation.
2. Add one alternative backend in a feature branch.
3. Run the same scale-level behaviour against both.
4. Compare noise, settling, diagnostics and hardware complexity.
5. Select the production backend using measured results.

---

# LoRa roadmap

LoRa should follow local-system stability.

Prerequisites:

- Safe startup and persistent tare.
- Non-blocking application flow.
- Defined fault states.
- Stable local measurement.
- Validated power architecture.
- Clear communication requirements.

Start with a mature library to validate:

- Required range.
- Packet structure.
- Update period.
- Reliability.
- Retry policy.
- Power consumption.
- Interference behaviour.

Only implement a lower-level or bare-metal radio driver later as an educational exercise.

Possible module boundary:

```text
app
 ├── scale
 ├── indicators
 ├── console
 └── radio
```

The radio must not become a dependency of the measurement driver.

---

# Resume checklist

When returning to the project after a long pause:

## Repository status

```powershell
git switch main
git pull --ff-only
git status
git log --oneline --decorate -12
git branch -a
git tag --list
```

## Read the current state

Read:

```text
README.md
docs/project-seed.md
docs/project-roadmap.md
docs/v1.1-release-notes.md
docs/v1.1-safe-startup-tare-validation.md
```

Then read the notes for the most recent completed milestone.

## Validate the baseline

```powershell
pio run -e nanoatmega328new
.\scripts\run-native-tests.ps1
```

Expected baseline at `v1.1-safe-startup-tare`:

```text
production build: SUCCESS
native suites:    10/10
tests:            225
failures:         0
```

Validated production memory:

```text
SRAM:  195 bytes
Flash: 13566 bytes
```

## Identify the active objective

Before coding, determine:

- Latest stable tag.
- Current branch.
- Whether the working tree is clean.
- Whether an unfinished feature branch exists.
- Last completed commit.
- Next incomplete item in this roadmap.
- Any hardware change since the previous session.
- Whether stored calibration and tare data remain valid for the current installation.

## Suggested context to provide to an assistant

Provide:

- The current repository or ZIP.
- `README.md`.
- `docs/project-seed.md`.
- `docs/project-roadmap.md`.
- The latest milestone notes.
- Current `git status`.
- Current `git log --oneline --decorate -12`.
- The exact hardware configuration.
- The last command that succeeded.
- The exact error or next objective.

A useful resume instruction is:

```text
Use project-seed.md for completed history and project-roadmap.md
for future work. Verify the current repository state and continue
from the first incomplete milestone without restarting the project.
```

---

# Priority summary

Current recommended order:

```text
1. Begin the detailed educational study of v0.1 and v0.2
2. Implement v1.2 non-blocking application operations
3. Add fault handling and watchdog
4. Improve measurement robustness
5. Validate 24 V power and output hardware
6. Build the final mechanical installation
7. Design a custom PCB
8. Evaluate alternative ADC backends
9. Add LoRa
10. Reorganize the source tree only when growth justifies it
```

The educational and production tracks may progress in parallel, but production changes should remain small, testable and independently tagged.
