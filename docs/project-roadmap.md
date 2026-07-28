# Project Roadmap and Continuation Guide

## Purpose

This document is the active continuation plan for the project after:

```text
v1.0-functional-prototype
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
v1.0-functional-prototype
```

The release contains:

- Direct AVR production entry point.
- No Arduino Core in the production execution path.
- Project-owned HX711 driver.
- Project-owned GPIO, Timer1, delay, EEPROM and USART0 backends.
- Persistent calibration factor.
- Automatic and manual tare.
- Physical and serial calibration.
- Four logical level states represented by three LEDs.
- 177 native tests with zero failures.
- Complete project README.
- One-command native regression script.
- English project documentation.

Production memory usage at the stable baseline:

```text
Static SRAM: 194 bytes
Flash:       11356 bytes
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

Create a separate repository that explains the project progressively and in depth without adding educational comments or duplicate teaching material to the production firmware.

Repository:

```text
load-cell-level-indicator-study
```

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

# Next firmware milestone

## v1.1: safe startup tare

Planned tag:

```text
v1.1-safe-startup-tare
```

Planned branch:

```text
feature/safe-startup-tare
```

This is the highest-priority firmware improvement after `v1.0`.

## Problem

The current startup sequence performs an automatic tare.

If power is lost while the container is partially filled:

```text
partially filled container
    |
    v
power failure
    |
    v
restart
    |
    v
automatic tare with the current load
    |
    v
current load becomes zero
    |
    v
incorrect empty-level indication
```

The same issue can occur after:

- Manual reset.
- Brown-out reset.
- Watchdog reset.
- USB reconnection.
- Firmware upload.
- Any unexpected restart.

This is acceptable for the first functional prototype because it can be recovered manually, but it is not suitable for unattended field operation.

## Fundamental measurement limitation

The measured raw value is approximately:

```text
raw = tare_offset + weight × calibration_factor
```

After a restart, one raw reading cannot uniquely determine both:

```text
tare_offset
weight
```

The firmware cannot reliably infer whether a changed raw value represents:

- A different zero offset.
- An existing load.
- A mixture of both.

Therefore, the problem should not be solved with an arbitrary startup threshold or an automatic guess.

## Selected policy

Persist the tare offset in EEPROM and restore it during startup.

The intended behaviour is:

```text
startup
    |
    v
load calibration factor
    |
    v
load stored tare offset
    |
    +--> valid tare: apply it and enter normal operation
    |
    +--> no valid tare: enter TARE_REQUIRED
```

A new automatic tare must not occur during every restart.

## Operational zero

For the deployed system, the preferred zero reference is:

```text
platform + empty permanent container
```

This makes the displayed weight represent mainly the container contents.

A new tare is required when:

- The container is replaced.
- The mechanical installation changes.
- The load cell or mounting changes.
- The empty-container zero has drifted enough to require correction.

The operating instructions must state this clearly.

## Persistent tare record

Calibration and tare should remain separate records because they represent different physical properties.

Calibration:

```text
counts per gram
```

Tare:

```text
raw zero offset for the current mechanical installation
```

Approximate tare record:

```cpp
struct TareStorageRecord
{
    uint32_t magic;
    uint8_t version;
    int32_t tare_offset;
    uint16_t crc;
};
```

The final field layout should follow the established EEPROM-storage conventions already used by calibration storage.

The record should provide:

- Identifier or magic value.
- Version.
- Stored offset.
- CRC.
- Validation before use.
- Invalid-record fallback.
- Explicit invalidation support if required.

## Tare-required state

When no valid stored tare exists, the application should not report a normal level as though the measurement were trustworthy.

It should enter an explicit state:

```text
TARE_REQUIRED
```

Possible behaviour:

- Suppress normal weight-level output.
- Show a distinct LED pattern.
- Print a clear console instruction.
- Wait for deliberate user action.

Suggested console message:

```text
No valid tare offset is stored.
Place the empty container on the platform and perform tare.
```

The final LED pattern must be different from:

- Normal `VERY_LOW`.
- Calibration wait.
- Tare in progress.
- Success.
- Error.

## Deliberate manual tare

For field use, a short accidental button press should not immediately redefine zero.

Preferred normal-operation control:

```text
hold tare button for approximately 3 seconds
    |
    v
perform tare
    |
    v
validate result
    |
    v
apply tare offset
    |
    v
store tare offset
```

During calibration, the tare button may continue to cancel immediately because its contextual purpose is different.

The serial command should also be reconsidered.

Possible safe policies:

1. Require two `t` commands within a confirmation window.
2. Add a separate service mode.
3. Keep direct `t` only for development builds.
4. Require a longer explicit command such as `tare`.

The selected serial policy should be documented before implementation.

## EEPROM wear

Persistent tare should be written only after a deliberate successful tare.

Do not:

- Store every weight reading.
- Store the current weight every 500 ms.
- Rewrite the tare record continuously.
- Use EEPROM as a runtime log.

Occasional manual tare writes are not expected to create a practical wear problem.

## Planned commits

The exact design may adjust after code review, but the intended sequence is:

```text
docs: define safe startup tare policy
test: add persistent tare storage tests
feat: add persistent tare storage
test: add safe startup tare application tests
feat: restore tare offset during startup
feat: add tare-required application state
feat: require deliberate manual tare
docs: record safe startup tare validation
```

Each commit should remain compilable and conceptually focused.

## Required tests

Storage tests:

- Empty EEPROM.
- Valid tare record.
- Invalid magic value.
- Unsupported record version.
- Corrupted CRC.
- Positive offset.
- Negative offset.
- Boundary values.
- Store and load round trip.
- Invalidate record.
- Calibration record remains unchanged.
- Tare record remains unchanged when calibration is stored.

Application tests:

- Startup with valid stored tare.
- Startup without stored tare.
- Startup with corrupted tare.
- Startup while the container is partially filled.
- Startup after a full power cycle.
- Deliberate manual tare.
- Accidental short press.
- Long-press threshold.
- Tare success.
- Tare failure.
- Persistent offset written only after success.
- Calibration factor is not modified by tare.
- Correct transition from `TARE_REQUIRED` to normal operation.

Physical validation:

- First boot with no stored tare.
- Tare with the empty permanent container.
- Restart with the empty container.
- Restart with a partially filled container.
- Power interruption with a partially filled container.
- Manual reset with a partially filled container.
- Replace the container and perform a new tare.
- Repeated bootloader uploads.
- EEPROM persistence across power cycles.

## Definition of done

The milestone is complete when:

- Startup no longer automatically tares over an unknown load.
- A valid tare offset survives reset and power loss.
- A partially filled container remains correctly measured after restart.
- Missing or corrupted tare data produces `TARE_REQUIRED`.
- The user must perform a deliberate tare before normal measurement.
- Calibration and tare records remain independent.
- Existing calibration behaviour remains valid.
- Existing level behaviour remains valid after a valid tare is loaded.
- Native tests pass.
- Physical power-cycle behaviour is validated.
- Documentation is updated.
- The milestone is merged and tagged.

---

# Later firmware milestones

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
docs/v1.0-release-notes.md
```

Then read the notes for the most recent milestone after `v1.0`, if one exists.

## Validate the baseline

```powershell
pio run -e nanoatmega328new
.\scripts\run-native-tests.ps1
```

Expected baseline at `v1.0-functional-prototype`:

```text
production build: SUCCESS
native suites:    8/8
tests:            177
failures:         0
```

Later milestones may increase the test total.

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
1. Create the educational repository skeleton
2. Implement v1.1-safe-startup-tare
3. Begin detailed study of v0.1 and v0.2
4. Implement non-blocking application operations
5. Add fault handling and watchdog
6. Improve measurement robustness
7. Validate 24 V power and output hardware
8. Build the final mechanical installation
9. Design a custom PCB
10. Evaluate alternative ADC backends
11. Add LoRa
12. Reorganize the source tree only when growth justifies it
```

The educational and production tracks may progress in parallel, but production changes should remain small, testable and independently tagged.
