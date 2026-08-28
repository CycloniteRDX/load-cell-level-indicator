# Project Roadmap and Continuation Guide

## Purpose

This document is the active continuation plan for the project after:

```text
v1.3-fault-recovery-and-watchdog
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
v1.3-fault-recovery-and-watchdog
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
- Cooperative startup, tare and calibration states.
- At most one ready HX711 conversion per application update.
- Explicit busy-state and result-state input policies.
- Latched cooperative startup fault instead of a permanent wait loop.
- Stable fault codes with explicit recoverable or terminal policy.
- Runtime HX711 readiness supervision and bounded cooperative recovery.
- Three recovery attempts with real HX711 power cycling.
- Distinct recovery and terminal LED patterns.
- Persistent record loads that distinguish absent, corrupt and access failure.
- Two-second AVR hardware watchdog fed only after complete iterations.
- Dedicated watchdog validation builds for both entry points.
- Deterministic DOUT fault detection through an external 10 kΩ pull-up.
- Four logical level states represented by three LEDs.
- 307 native tests across 13 suites with zero failures.
- Complete release and physical-validation documentation.

Production memory usage at the stable baseline:

```text
Static SRAM: 230 bytes
Flash:       17738 bytes
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
- The two measurement backends remain small compile-time adapters.
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

The repository is complete through Lesson 19 and documents the full historical
progression from the initial PlatformIO project through
`v1.1-safe-startup-tare`.

## Current structure

```text
load-cell-level-indicator-study/
â”œâ”€â”€ README.md
â”œâ”€â”€ lessons/
â”‚   â”œâ”€â”€ 00-project-overview/
â”‚   â”œâ”€â”€ 01-initial-platformio-project/
â”‚   â”‚   â”œâ”€â”€ README.md
â”‚   â”‚   â”œâ”€â”€ annotated-source/
â”‚   â”‚   â”œâ”€â”€ exercises.md
â”‚   â”‚   â””â”€â”€ solutions.md
â”‚   â”œâ”€â”€ ...
â”‚   â””â”€â”€ 19-safe-startup-tare/
â”‚       â”œâ”€â”€ README.md
â”‚       â”œâ”€â”€ annotated-source/
â”‚       â”œâ”€â”€ exercises.md
â”‚       â””â”€â”€ solutions.md
â””â”€â”€ reference/
    â””â”€â”€ README.md
```

Exercises and solutions belong to the lesson that provides their context.
`reference/README.md` is a transversal index that points to the detailed
explanations inside the lessons.

The production milestones available to the educational track are:

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

Do not create a lesson for planned firmware. Add the next lesson only after its
production milestone has been implemented, tested, integrated and tagged. The
next educational addition is Lesson 20, based on the now-stable
`v1.2-non-blocking-application` release.

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

## v1.2: non-blocking application operations

Completed tag:

```text
v1.2-non-blocking-application
```

Development branch:

```text
feature/non-blocking-application
```

The milestone introduced:

- Cooperative HX711 startup readiness polling.
- A latched fault state after a 2000 ms startup timeout.
- An incremental scale collector with at most one ready read per update.
- Non-blocking 20-sample operational tare.
- Non-blocking zero and reference-mass calibration phases.
- Cancellation before the next sample from UART or D4.
- A 5000 ms total timeout for multi-sample operations.
- Explicit result-pattern ownership and input suppression until release.
- Immediate state-specific responses for rejected UART commands.
- Transactional tare and calibration persistence guarantees preserved.
- 269 passing native tests across 11 suites.
- Ten passing physical validation scenarios at the real 10 SPS rate.

Validated memory usage:

```text
Direct AVR production:
SRAM:  217 bytes
Flash: 16898 bytes

Arduino reference:
SRAM:  226 bytes
Flash: 17194 bytes
```

Detailed records:

```text
docs/non-blocking-application-notes.md
docs/v1.2-non-blocking-application-validation.md
docs/v1.2-release-notes.md
```

## v1.3: fault recovery and watchdog

Completed tag:

```text
v1.3-fault-recovery-and-watchdog
```

Development branch:

```text
feature/fault-recovery-watchdog
```

The milestone introduced:

- Stable application-visible fault codes `01` through `09`.
- Explicit recover-sensor and terminal policies.
- Scale read results that distinguish value, no data and read error.
- A 2000 ms runtime no-conversion health deadline.
- Cooperative HX711 power cycling with 500 ms backoff, 2000 ms attempt
  deadline and three attempts.
- Cancellation of incomplete work and return only to safe boundary states.
- D4/D8 suppression and UART discard across recovery transitions.
- LOW/HIGH alternating recovery indication and persistent HIGH terminal fault.
- Valid, absent, corrupt and access-failure persistent-load classification.
- Two-second AVR hardware watchdog fed after complete application iterations.
- Early application-visible reset-cause capture.
- Dedicated direct-AVR and Arduino-reference watchdog validation builds.
- External 10 kΩ DOUT pull-up for deterministic disconnection detection.
- 307 passing native tests across 13 suites.
- Complete watchdog, PD_SCK, retry, interruption and input-boundary physical
  validation.

Validated memory usage:

```text
Direct AVR production:
SRAM:  230 bytes
Flash: 17738 bytes

Arduino reference:
SRAM:  239 bytes
Flash: 18032 bytes
```

Detailed records:

```text
docs/fault-recovery-watchdog-notes.md
docs/v1.3-fault-recovery-and-watchdog-validation.md
docs/v1.3-release-notes.md
```

---

# Next and later firmware milestones

The names and ordering below are provisional. They should remain smaller than a single large redesign.

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

### Evidence-driven pause for ADS1232 comparison

The HX711 capture phase found a strong LED-current return coupling mechanism and
an unresolved broadband component in the unsoldered breadboard assembly. The
raw data and conclusions are preserved in:

```text
logs/
docs/hx711-prototype-characterization.md
```

An ADS1232 module became available before the filtering policy was selected.
The preferred sequence and current status are:

1. **Complete:** preserve the HX711 diagnostic branch and datasets.
2. **Complete:** add a project-owned ADS1232 driver on `feature/ads1232-support`.
3. **Complete in code:** introduce the minimum compile-time boundary needed to
   keep `scale` independent of one converter.
4. **Complete in code:** assign distinct calibration and tare records to each
   converter.
5. **Next:** pass native and production builds, then repeat comparable physical
   captures with the same cell and mechanics.
6. Resume filter selection using evidence from both backends.

This is not a broad source-layout refactor. Any later directory reorganization
remains a separate change.

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
 â”œâ”€â”€ scale
 â”œâ”€â”€ indicators
 â”œâ”€â”€ console
 â””â”€â”€ radio
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
docs/v1.3-release-notes.md
docs/v1.3-fault-recovery-and-watchdog-validation.md
```

Then read the notes for the most recent completed milestone.

## Validate the baseline

```powershell
pio run -e nanoatmega328new
.\scripts\run-native-tests.ps1
```

Expected baseline at `v1.3-fault-recovery-and-watchdog`:

```text
production build: SUCCESS
native suites:    13/13
tests:            307
failures:         0
```

Validated production memory:

```text
SRAM:  230 bytes
Flash: 17738 bytes
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
1. Add Lesson 20 to the study repository for stable v1.2
2. Add the later study lesson for stable v1.3
3. Preserve and document the HX711 physical characterization
4. Implement and evaluate the available ADS1232 backend
5. Resume measurement robustness using comparable raw data
6. Validate 24 V power and output hardware
7. Build the final mechanical installation
8. Design a custom PCB
9. Add LoRa
10. Reorganize the source tree only when growth justifies it
```

The educational and production tracks may progres
