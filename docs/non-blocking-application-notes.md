# Non-Blocking Application Design Notes

## Milestone

Development branch:

```text
feature/non-blocking-application
```

Planned tag:

```text
v1.2-non-blocking-application
```

## Purpose

Convert the remaining multi-conversion waits and permanent startup loops into
explicit cooperative application states.

The firmware already executes most recurring work without long application
delays:

- Button debounce and hold detection use elapsed time.
- USART reception uses an interrupt-driven ring buffer.
- Normal measurement starts only when the HX711 already has a conversion.
- Level and operation-indicator blinking use elapsed time.
- Success and error patterns are generated incrementally.

The remaining problem is that tare and calibration collect all requested HX711
samples inside one function call. At the current 10 samples per second, a
20-sample operation can occupy the application for approximately two seconds.
During that time the receive interrupt can queue serial bytes, but
`app_update()` cannot process buttons, commands, indicators or future
communications work.

This milestone will make those operations cooperative. One call to
`app_update()` will perform at most one HX711 conversion read and will then
return to the main loop.

---

## Baseline

The design starts from:

```text
v1.1-safe-startup-tare
```

plus the documentation-only commit:

```text
docs: synchronize roadmap and validation status
```

The baseline provides:

- Project-owned AVR GPIO, Timer1, EEPROM and USART backends.
- A project-owned HX711 driver and platform adapter.
- Persistent calibration and tare records with CRC verification.
- Safe startup behaviour when no valid tare exists.
- Long-press protection for physical tare and calibration entry.
- Non-blocking operation and level-indicator patterns.
- Native tests for the existing isolated modules.
- 225 passing native tests at the `v1.1` release.

The external behaviour validated for `v1.1` remains the reference unless this
document explicitly defines a change.

---

## `v1.1` baseline blocking paths

| Path | Current behaviour | Approximate consequence |
| --- | --- | --- |
| `scale_init()` | Calls `hx711_wait_ready()` with a 2000 ms timeout | Startup may remain inside one call for up to two seconds |
| `scale_tare()` | Reads and averages `TARE_SAMPLES` inside a loop | With 20 samples at 10 SPS, the application is paused for about two seconds |
| `scale_read_net_counts()` | Reads all requested calibration samples inside a loop | Calibration pauses the application for about two seconds |
| HX711 startup failure | `app_init()` enters a permanent delay loop | `app_update()` is never reached |
| Invalid startup calibration factor | `app_init()` enters a permanent delay loop | `app_update()` is never reached |
| Temporary result pattern | The indicator itself is incremental, but `app_update()` returns early | Input is deliberately discarded until the pattern finishes |

Normal weight acquisition is already almost suitable for the target model.
`scale_read_weight()` first checks `hx711_is_ready()` and `WEIGHT_SAMPLES` is
currently one. The new API will make that single-sample guarantee explicit so a
future configuration change cannot accidentally reintroduce a blocking loop.

---

## Meaning of non-blocking in this project

This is a cooperative bare-metal superloop, not a preemptive operating system.

For this milestone, non-blocking means:

- No application function waits for the next HX711 conversion.
- No application function collects several HX711 conversions in one call.
- Long operations retain progress between calls to `app_update()`.
- Every pass through the main loop can update time-driven indicators.
- Cancellation and serial commands can be examined between samples.
- A missing HX711 conversion is handled by returning to the main loop.
- Startup and runtime timeouts are evaluated from timestamps, not busy waits.

It does not mean that every instruction or peripheral transaction becomes
asynchronous.

The following bounded synchronous operations remain acceptable:

- Clocking one already-ready 24-bit HX711 sample.
- The short critical section used for the HX711 clock sequence.
- EEPROM record read, write and verification.
- Formatting and transmitting short console messages.
- Reading and updating one application state.

These operations can still take microseconds or, for EEPROM and longer console
messages, several milliseconds. Removing every bounded synchronous peripheral
operation would require separate asynchronous EEPROM and transmit-buffer
milestones and is not required to eliminate the current multi-second stalls.

---

## Scope

The milestone includes:

- Non-blocking HX711 startup readiness detection.
- An incremental scale sample collector.
- Non-blocking operational tare.
- Non-blocking calibration-zero sample collection.
- Non-blocking calibration-reference sample collection.
- Explicit total timeouts for startup and sample collection.
- Explicit cancellation while a long operation is active.
- An application state for temporary result patterns.
- A minimal persistent fault state instead of permanent loops in `app_init()`.
- An explicit input policy for every busy state.
- Native tests for the incremental scale API and application transitions.
- Physical validation on the Arduino Nano and real HX711.

## Non-goals

The milestone does not include:

- Watchdog activation.
- Detailed fault categories or automatic recovery policies.
- Filtering, median calculation, outlier rejection or stability detection.
- Changes to the calibration mass or provisional thresholds.
- Asynchronous EEPROM programming.
- Interrupt-driven or buffered USART transmission.
- LoRa communication.
- Alternative ADC backends.
- RTOS tasks, dynamic allocation, classes or templates.
- A rewrite of the HX711 bit-clock transaction.
- Physical power-down and power-up validation.

Detailed recoverable faults and watchdog behaviour remain part of the planned
`v1.3` milestone. Measurement filtering and stability detection remain part of
the planned `v1.4` milestone.

---

## Execution model

The main loop continues to call:

```cpp
while (true)
{
    app_update();
}
```

Each `app_update()` iteration will follow this order:

1. Update the operation indicator.
2. Sample and interpret the inputs allowed in the current state.
3. Apply cancellation before collecting another sample.
4. Execute at most one state-machine step.
5. Read at most one ready HX711 conversion.
6. Update normal level effects and periodic output when normal operation owns
   the LEDs.
7. Return to the main loop.

Cancellation is checked before sample acquisition so a queued `q` command or a
new D4 cancel press wins over the next conversion.

The application must not use delay calls or a loop that waits for a state,
conversion count, timeout, button or command to change.

---

## Application states

The existing calibration-only state variable will be replaced by one explicit
application state.

Planned state type:

```cpp
typedef enum
{
    APP_STATE_STARTUP_WAIT_FOR_SCALE,
    APP_STATE_STARTUP_LOAD_CONFIGURATION,
    APP_STATE_TARE_REQUIRED,
    APP_STATE_NORMAL_OPERATION,
    APP_STATE_TARE_SAMPLING,
    APP_STATE_CALIBRATION_WAITING_FOR_ZERO,
    APP_STATE_CALIBRATION_ZERO_SAMPLING,
    APP_STATE_CALIBRATION_WAITING_FOR_MASS,
    APP_STATE_CALIBRATION_MASS_SAMPLING,
    APP_STATE_RESULT_PATTERN,
    APP_STATE_FAULT
} app_state_t;
```

`tare_available` remains a separate fact because it is needed while calibration
or a result pattern temporarily owns the application state.

`state_after_result` records the state to restore when a finite success or error
pattern finishes.

`operation_started_ms` records the start of startup waiting or sample
collection. Elapsed time is always calculated with unsigned subtraction so the
logic remains correct across `hal_time_millis()` overflow.

### State responsibilities

| State | Main responsibility | Normal measurement | Long-operation input |
| --- | --- | --- | --- |
| `STARTUP_WAIT_FOR_SCALE` | Poll HX711 readiness until ready or timed out | Disabled | Inputs are sampled and rejected |
| `STARTUP_LOAD_CONFIGURATION` | Load and apply calibration and tare records once | Disabled | Inputs are sampled and rejected |
| `TARE_REQUIRED` | Wait for deliberate tare or calibration entry | Disabled | Tare and calibration entry allowed |
| `NORMAL_OPERATION` | Read one ready weight sample and update level output | Enabled | Tare and calibration entry allowed |
| `TARE_SAMPLING` | Collect one tare sample whenever one is ready | Disabled | Cancel allowed |
| `CALIBRATION_WAITING_FOR_ZERO` | Wait for empty-platform confirmation | Disabled | Confirm or cancel allowed |
| `CALIBRATION_ZERO_SAMPLING` | Collect one zero sample whenever one is ready | Disabled | Cancel allowed |
| `CALIBRATION_WAITING_FOR_MASS` | Wait for reference-mass confirmation | Disabled | Confirm or cancel allowed |
| `CALIBRATION_MASS_SAMPLING` | Collect one reference sample whenever one is ready | Disabled | Cancel allowed |
| `RESULT_PATTERN` | Keep updating a finite success or error pattern | Disabled | Inputs are sampled but do not start work |
| `FAULT` | Preserve a safe, diagnosable latched state | Disabled | Inputs are sampled and rejected |

### Main transitions

| Current state | Event | Next state |
| --- | --- | --- |
| `STARTUP_WAIT_FOR_SCALE` | HX711 ready | `STARTUP_LOAD_CONFIGURATION` |
| `STARTUP_WAIT_FOR_SCALE` | Startup timeout | `FAULT` |
| `STARTUP_LOAD_CONFIGURATION` | Valid stored tare loaded | `NORMAL_OPERATION` |
| `STARTUP_LOAD_CONFIGURATION` | No valid stored tare | `TARE_REQUIRED` |
| `STARTUP_LOAD_CONFIGURATION` | Invalid active calibration factor | `FAULT` |
| `TARE_REQUIRED` or `NORMAL_OPERATION` | Tare requested | `TARE_SAMPLING` |
| `TARE_REQUIRED` or `NORMAL_OPERATION` | Calibration requested | `CALIBRATION_WAITING_FOR_ZERO` |
| `TARE_SAMPLING` | Collection and persistent save succeed | `NORMAL_OPERATION` |
| `TARE_SAMPLING` | Cancelled | Previous idle state |
| `TARE_SAMPLING` | Read, timeout or save error | Error pattern, then previous idle state |
| `CALIBRATION_WAITING_FOR_ZERO` | Confirmed | `CALIBRATION_ZERO_SAMPLING` |
| `CALIBRATION_ZERO_SAMPLING` | Collection and tare save succeed | `CALIBRATION_WAITING_FOR_MASS` |
| `CALIBRATION_ZERO_SAMPLING` | Read, timeout or save error | Error pattern, then `CALIBRATION_WAITING_FOR_ZERO` |
| `CALIBRATION_WAITING_FOR_MASS` | Confirmed | `CALIBRATION_MASS_SAMPLING` |
| `CALIBRATION_MASS_SAMPLING` | Valid factor saved | Success pattern, then `NORMAL_OPERATION` |
| `CALIBRATION_MASS_SAMPLING` | Retryable measurement error | Error pattern, then `CALIBRATION_WAITING_FOR_MASS` |
| Any calibration state | Cancelled | Idle state selected from `tare_available` |
| `RESULT_PATTERN` | Pattern complete | `state_after_result` |
| Any state | Internal invariant failure | `FAULT` |

The idle state selected from `tare_available` is:

```text
tare available     -> NORMAL_OPERATION
tare unavailable   -> TARE_REQUIRED
```

---

## Scale API direction

### Initialization

`scale_init()` will configure the HX711 pins and internal scale state, but it
will no longer call `hx711_wait_ready()`.

The application will poll a new readiness query:

```cpp
bool scale_is_ready(void);
```

This separates two different operations:

```text
configure the device     -> immediate initialization result
wait for first conversion -> application state plus timeout
```

The HX711 driver's existing `hx711_wait_ready()` remains available for driver
operations that require it, but the application-facing scale startup will not
use it.

### Normal weight reading

The normal-reading API will make its try-once behaviour explicit:

```cpp
bool scale_try_read_weight(
    float *weight_grams
);
```

It will:

1. Reject a null output pointer.
2. Return immediately when the HX711 is not ready.
3. Clock exactly one ready conversion.
4. Apply the current tare offset and calibration factor.
5. Return `true` only when a new weight value was produced.

The `WEIGHT_SAMPLES` configuration constant will no longer control a hidden
loop. Filtering and multi-sample normal measurement belong to the later
measurement-robustness milestone.

### Incremental sample collection

Current public status:

```cpp
typedef enum
{
    SCALE_SAMPLE_COLLECTION_IDLE,
    SCALE_SAMPLE_COLLECTION_IN_PROGRESS,
    SCALE_SAMPLE_COLLECTION_COMPLETE,
    SCALE_SAMPLE_COLLECTION_ERROR
} scale_sample_collection_status_t;
```

Current public operations:

```cpp
bool scale_start_sample_collection(
    uint8_t sample_count
);

scale_sample_collection_status_t
scale_update_sample_collection(void);

bool scale_take_sample_average(
    int32_t *average_raw
);

void scale_cancel_sample_collection(void);
```

The calibration migration removed the old blocking public functions:

```cpp
bool scale_tare(void);

bool scale_read_net_counts(
    float *net_counts,
    uint8_t samples
);
```

Their removal prevents a future caller from accidentally reintroducing the old
multi-conversion blocking behaviour.

### Collector invariants

The scale module owns:

```text
collector status
requested sample count
collected sample count
raw sum
completed average
```

The collector must obey these rules:

- A zero sample count is rejected.
- A new collection is rejected unless the collector is idle.
- An update while the HX711 is not ready performs no read and remains in
  progress.
- One update performs at most one `hx711_read_raw()` call.
- A read failure changes the collector to a sticky error state.
- Completion changes the collector to a sticky complete state.
- Complete and error states perform no further reads.
- Taking a completed average copies the result and returns the collector to
  idle.
- Cancellation discards partial progress and returns the collector to idle.
- Cancellation and failed collection never change the active tare offset or
  calibration factor.
- Initialization resets any previous collector progress.

The raw sum remains a signed 32-bit value. A valid signed 24-bit HX711 result
multiplied by the maximum `uint8_t` sample count still fits inside `int32_t`.

The application owns elapsed-time policy. The scale collector reports progress
or an I/O error; it does not read the system clock or decide when an operation
has taken too long.

---

## Why one ready HX711 read remains synchronous

The HX711 holds DOUT low after a conversion is ready and waits for the
microcontroller to clock out the result.

The scale collector will first call `hx711_is_ready()`. Only when DOUT is
already low will it call `hx711_read_raw()`.

`hx711_read_raw()` will continue to:

- Enter the existing critical section.
- Generate the 24 data-clock pulses.
- Generate the gain-selection pulse.
- Sign-extend the 24-bit result.
- Restore the previous interrupt state.

This short, timing-sensitive transfer should not be split across application
iterations. The long delay is the interval between conversions, not the few
microseconds used to transfer one already-ready conversion.

---

## Startup flow

`app_init()` will initialize the time, console, buttons, LEDs, indicators and
HX711 pin configuration. It will not wait for DOUT and will not enter a
permanent loop.

After successful immediate initialization it will:

1. Record `operation_started_ms`.
2. Enter `APP_STATE_STARTUP_WAIT_FOR_SCALE`.
3. Print the startup banner and one readiness message.
4. Return to the bare-metal entrypoint.

Repeated calls to `app_update()` will check:

```text
scale ready                  -> load configuration
elapsed time below timeout   -> remain in startup wait
elapsed time reached timeout -> enter fault
```

Configuration loading remains a separate state step. It will preserve the
`v1.1` policy:

- Use the stored calibration factor when its record is valid.
- Otherwise use the default calibration factor.
- Load and apply a valid stored tare offset.
- Enter `TARE_REQUIRED` when no valid stored tare exists.
- Never perform automatic startup tare.

---

## Non-blocking operational tare

Starting an operational tare will:

1. Remember whether the return idle state is `NORMAL_OPERATION` or
   `TARE_REQUIRED`.
2. Invalidate the last displayed measurement.
3. Reset the level indicator.
4. Start a `TARE_SAMPLES` collection.
5. Record `operation_started_ms`.
6. Select the all-LED tare indication.
7. Enter `APP_STATE_TARE_SAMPLING`.

Every update in `TARE_SAMPLING` will first process cancellation and then call
`scale_update_sample_collection()` once.

The collected average is a candidate offset. It will not replace the active
offset during partial collection.

After successful collection:

1. Take the completed average.
2. Save and verify the candidate offset in EEPROM.
3. Apply it with `scale_set_offset()` only after the save succeeds.
4. Set `tare_available` to true.
5. Clear the operation indicator.
6. Enter `NORMAL_OPERATION`.

This ordering preserves the previous active runtime offset if collection or
persistent saving fails and avoids a temporary RAM rollback.

If EEPROM saving fails after modifying some physical EEPROM bytes, the previous
runtime offset remains active for the current boot. As in `v1.1`, the record may
be rejected at the next startup if the interrupted or failed write damaged it.
Multi-slot power-fail-safe persistence is a separate future improvement.

---

## Non-blocking calibration

### Waiting for zero

Calibration entry remains deliberate:

- Hold D8 for three seconds during an idle state, or
- Send `c` from the console.

The application enters `CALIBRATION_WAITING_FOR_ZERO`, disables normal
measurement and waits for a new confirmation.

### Zero collection

Confirmation starts a `TARE_SAMPLES` collection and enters
`CALIBRATION_ZERO_SAMPLING`.

The candidate zero offset is not applied during partial collection. After all
samples are available, it is saved and verified first and then applied to the
scale. Successful zero collection enters
`CALIBRATION_WAITING_FOR_MASS`.

As in `v1.1`, a successful calibration-zero tare remains the active persistent
tare even if the user later cancels before calculating a new factor.

### Reference-mass collection

Confirmation in `CALIBRATION_WAITING_FOR_MASS` starts a
`CALIBRATION_SAMPLES` collection and enters
`CALIBRATION_MASS_SAMPLING`.

After collection:

```text
average raw counts
    - active tare offset
    = net counts

net counts
    / CALIBRATION_MASS_GRAMS
    = calibration factor in counts/g
```

The application preserves the existing checks:

- Reject a reference signal whose absolute net count is below
  `MINIMUM_CALIBRATION_SIGNAL_COUNTS`.
- Reject zero, non-finite or unrepresentable calibration factors through the
  scale API.
- Restore the previous active factor if persistent saving fails after the new
  factor was applied.
- Invalidate the previous displayed measurement after a successful change.

Successful calibration starts the existing success pattern and returns to
normal operation after the pattern completes.

---

## Cancellation policy

Cancellation is allowed during every tare or calibration sampling state.

Accepted cancellation inputs are:

- Serial `q` or `Q`.
- A new debounced D4 press.

For a tare started by holding D4, keeping the same press held does not cancel
the operation. The user must release and press D4 again. This follows naturally
from the existing one-event-per-press button logic.

Cancellation will:

1. Call `scale_cancel_sample_collection()`.
2. Suppress hold-until-release when cancellation came from D4, so the same
   physical press cannot later become a tare request.
3. Discard the partial sum and sample count.
4. Leave the active offset and factor unchanged.
5. Invalidate any stale displayed measurement.
6. Reset the level indicator.
7. Restore the idle state selected from `tare_available`.
8. Print one cancellation message.

Cancelling any calibration state cancels the complete calibration workflow.

---

## Console and button policy

Input responsiveness does not require every command to be legal in every
state. It requires the application to examine input, make a current-state
decision and avoid executing a stale command later.

| State group | Accepted commands | Other serial input | Physical-button policy |
| --- | --- | --- | --- |
| Idle (`TARE_REQUIRED`, `NORMAL_OPERATION`) | Existing `t`, `c`, `q`, `s`, `x`, `z` policy | Report unknown command | Existing hold rules apply |
| Waiting calibration states | `c` confirms, `q` cancels | Report unavailable while calibrating | D8 press confirms; D4 press cancels |
| Sampling states | `q` cancels | Report operation busy | D4 new press cancels; D8 press is consumed and suppressed |
| Startup states | None | Report startup not complete | Presses are consumed and hold is suppressed |
| Result pattern | None | Report result pattern active | Presses are consumed and hold is suppressed |
| Fault | None | Report reset required | Presses are consumed and hold is suppressed |

The console continues to process one leading command and discard the remaining
queued bytes according to the existing command framing policy. The important
change is that input is examined during sample collection instead of remaining
queued for approximately two seconds.

When a physical press is intentionally ignored in a busy state,
`button_suppress_hold_until_release()` prevents that same press from becoming a
long-press action immediately after the state changes.

No command received in one state may execute later as though it had been
received in another state.

---

## Result-pattern state

The operation-indicator module already generates success and error patterns
without delay loops. The application will keep that implementation.

The change is that pattern ownership becomes explicit through
`APP_STATE_RESULT_PATTERN` instead of being a global early-return condition in
`app_update()`.

Starting a result pattern will:

1. Select success or error in `operation_indicator`.
2. Store the intended `state_after_result`.
3. Enter `APP_STATE_RESULT_PATTERN`.

The state will continue to update the indicator and consume current input. When
`operation_indicator_is_temporary_active()` becomes false, the application will
enter `state_after_result`.

This state does not enable normal measurement while a result pattern owns the
three shared LEDs.

---

## Minimal fault state

The two permanent loops currently used by `app_init()` will be replaced by
`APP_STATE_FAULT`.

For `v1.2`, the fault state is deliberately small:

- It is latched until reset.
- Normal measurement is disabled.
- Scale sample collection is cancelled.
- Level output is reset.
- A persistent generic fault indication is shown.
- The original diagnostic reason is printed once.
- Indicators and input sampling continue to run.
- Commands are rejected with a reset-required message.
- No automatic retry is attempted.

The operation indicator will gain one generic persistent fault mode. A slowly
blinking HIGH LED is sufficient for this milestone. Multiple fault codes,
recovery, retry backoff, safe-output categories and watchdog integration belong
to `v1.3`.

Replacing a permanent loop with a latched cooperative state does not claim that
the underlying fault has become recoverable. It only keeps the superloop alive
and makes later fault handling possible.

---

## Timing constants

The application configuration exposes named timeout values rather than
keeping timeout policy private inside `scale.cpp`.

Current configuration:

```cpp
static const uint32_t SCALE_STARTUP_TIMEOUT_MS =
    2000UL;

static const uint32_t SCALE_SAMPLE_COLLECTION_TIMEOUT_MS =
    5000UL;
```

The two-second startup value preserves the existing readiness allowance.

The five-second collection value is provisional. It allows margin over the
approximately two seconds required for 20 samples at 10 SPS while still
detecting a disconnected, powered-down or stalled HX711.

The timeout covers the complete collection, not each individual sample. This
is appropriate for the current fixed 20-sample tare and calibration operations.
If future configurations use much larger sample counts or a selectable sample
rate, the timeout policy must be revisited.

All checks use:

```cpp
(uint32_t)(now - operation_started_ms) >= timeout_ms
```

No timeout uses an absolute future timestamp.

---

## Behavioural invariants preserved from v1.1

The implementation must preserve all of the following:

- Startup never performs automatic tare.
- A missing or invalid persistent tare disables normal measurement.
- The stored tare is restored exactly when valid.
- Physical operational tare requires a three-second hold.
- Serial `t` remains an immediate service command in an idle state.
- Physical calibration entry requires a three-second D8 hold.
- D4 cancels calibration and cannot later become a tare from the same press.
- A failed tare collection leaves the previous runtime offset active.
- A failed tare save leaves the previous runtime offset active.
- Successful calibration-zero collection persists and applies the new tare.
- A failed calibration-factor save restores the previous runtime factor.
- Calibration and tare changes invalidate the previously displayed weight.
- Normal level indication never runs while an operation indicator owns the
  LEDs.
- Negative calibration factors remain valid.
- The Arduino reference and direct-AVR production environments continue to
  build from the same application code.

---

## Native test strategy

### Scale tests

The `native_scale` suite will be updated to verify:

- `scale_init()` configures the HX711 without calling `hx711_wait_ready()`.
- `scale_is_ready()` forwards the current ready state.
- Reinitialization resets collector progress only after successful device
  initialization.
- A zero-length collection is rejected.
- A collection cannot be started while another result or error is pending.
- A not-ready update performs no raw read.
- Each ready update consumes exactly one raw reading.
- Partial progress remains in progress.
- The final sample produces the same truncated integer average as `v1.1`.
- Positive and negative 24-bit boundary values do not overflow the sum.
- A first, middle or final read error preserves the previous scale
  configuration.
- Error and complete states are sticky and perform no extra reads.
- Taking a result succeeds only after completion and returns the collector to
  idle.
- Cancellation from partial, complete and error states returns the collector
  to idle without changing offset or factor.
- `scale_try_read_weight()` returns immediately when no conversion is ready and
  reads exactly one sample when ready.

The HX711 fake will continue to count readiness queries and raw-read calls so
the one-read-per-update property is directly testable.

### Application tests

A `native_app` environment will use fake scale, console, buttons, time, storage
and indicator dependencies. Tests will focus on externally observable state
transitions rather than exact copies of every console string.

Required scenarios include:

- Startup remains responsive while the scale is not ready.
- Startup readiness loads configuration once.
- Startup timeout enters the latched fault state.
- Stored tare selects normal operation; missing tare selects tare-required.
- Operational tare collects one sample step per update.
- Operational tare can be cancelled between samples.
- Tare collection failure and save failure preserve the previous offset.
- Calibration zero can be confirmed, cancelled, retried and completed.
- Calibration mass can be cancelled between samples.
- A too-small signal returns to mass confirmation after an error pattern.
- A successful factor waits for the success pattern before normal operation.
- Input received during a sampling or result state is not executed later.
- A D4 cancellation press cannot become a later hold event.
- Millisecond overflow does not break startup or collection timeout checks.

If compiling the complete `app.cpp` test boundary proves disproportionately
large, the state coordinator may be extracted into one project-owned module and
tested there. This is an implementation fallback, not permission to omit state
transition coverage.

### Regression tests

All existing native environments continue to pass. Tests that directly called
the removed blocking APIs were retired; their arithmetic, failure and boundary
coverage now belongs to the incremental collector tests and the application
transition tests that compose the collector into tare and calibration.

---

## Build and memory validation

Every implementation stage must build both firmware environments:

```bash
pio run -e nanoatmega328new
pio run -e nanoatmega328new_arduino
```

The native test command remains:

```bash
pio test -e native_button \
         -e native_hx711 \
         -e native_level_indicator \
         -e native_operation_indicator \
         -e native_scale \
         -e native_app \
         -e native_tare_record \
         -e native_tare_storage \
         -e native_calibration_storage \
         -e native_console \
         -e native_time_delay
```

The `native_app` environment is included in the complete test command from the
cooperative startup and fault-state commit onward.

Flash and SRAM changes must be recorded for both AVR builds. The state machine
must use static storage only and must not introduce heap allocation.

---

## Physical validation plan

Native tests cannot prove electrical timing, real sample rate or human-visible
responsiveness. The completed milestone requires tests on the Arduino Nano,
HX711, load cell, buttons and LEDs.

### Startup

- Boot with a valid stored tare and verify normal measurement resumes.
- Boot without a valid stored tare and verify `TARE_REQUIRED` remains active.
- Hold DOUT high or disconnect the HX711 and verify startup enters the generic
  fault indication after the timeout without freezing the superloop.

### Operational tare

- Start tare from serial and verify indicators continue updating.
- Cancel it with D4 before 20 samples complete.
- Start tare from the physical long press and cancel after releasing and
  pressing D4 again.
- Send an unavailable command during tare and verify it is not executed later.
- Complete tare and verify the new offset survives power loss.

### Calibration

- Start and cancel while waiting for zero.
- Cancel during zero sample collection.
- Complete zero collection and cancel while waiting for the mass.
- Cancel during reference-mass sample collection.
- Complete calibration and verify the success pattern and stored factor.
- Attempt calibration without the reference mass and verify retry behaviour.

### Responsiveness evidence

At 10 SPS, confirm that:

- Indicator phases continue changing between samples.
- Cancellation is handled before all 20 samples have been collected.
- UART commands receive a current-state response instead of executing after the
  operation.
- No multi-second pause appears in the console or button handling.

Physical HX711 power-down and power-up validation remains separately pending
and is not silently marked complete by this milestone.

---

## Implementation sequence

The milestone should be divided into reviewable commits:

1. `docs: define non-blocking application design`
2. `refactor: add incremental scale sample collection`, including its updated
   scale tests
3. `refactor: add cooperative startup and fault states`, including their state
   tests
4. `refactor: make tare sample collection non-blocking`, including tare
   transition tests
5. `refactor: make calibration sample collection non-blocking`, including
   calibration transition tests
6. `refactor: make result input policy explicit`, including busy-state tests
7. `docs: record non-blocking application validation`
8. Final release documentation and `v1.2-non-blocking-application` tag

Implementation and its directly corresponding tests belong in the same commit
so every committed behaviour is covered and the branch remains buildable. Each
commit must retain a clear single purpose.

---

## Definition of done

- [x] `app_init()` contains no permanent wait or error loop.
- [x] Scale startup readiness is polled from `app_update()`.
- [x] Tare collection reads at most one ready sample per update.
- [x] Calibration collection reads at most one ready sample per update.
- [x] Blocking `scale_tare()` and `scale_read_net_counts()` are removed.
- [x] Normal weight acquisition cannot start a multi-sample loop.
- [x] Startup and sample collection use overflow-safe timeouts.
- [x] Sampling operations can be cancelled between conversions.
- [x] Buttons are sampled during long operations.
- [x] UART input received during long operations has an immediate explicit
      policy.
- [ ] No command remains queued for execution in a later state.
- [ ] Temporary result patterns are represented by an application state.
- [x] Startup failures enter a cooperative latched fault state.
- [x] Runtime tare and calibration rollback guarantees are preserved.
- [ ] All native tests pass, including scale and application transition tests.
- [ ] Direct AVR and Arduino reference firmware builds pass.
- [ ] Flash and SRAM usage are recorded.
- [ ] Physical cancellation and responsiveness are validated at 10 SPS.
- [ ] Power-loss restoration of the final tare and calibration still works.
- [ ] Validation results and any remaining limitations are documented.
- [ ] The milestone is merged and tagged only after physical validation.
