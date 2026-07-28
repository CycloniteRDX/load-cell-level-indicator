# Safe Startup Tare Design Notes

## Milestone

Planned branch:

```text
feature/safe-startup-tare
```

Planned tag:

```text
v1.1-safe-startup-tare
```

## Purpose

Prevent an unexpected restart from redefining the current load as zero.

The `v1.0-functional-prototype` firmware performs an automatic tare during every startup. This is convenient during bench development, but it is unsafe for unattended field operation.

If power is lost while the container is partially filled, the current startup sequence performs tare with that load still present. The partially filled container is then reported as empty.

This milestone replaces automatic startup tare with a persistent, explicitly established tare offset.

---

## Existing behaviour

The current startup flow is approximately:

```text
reset
    |
    v
initialize scale
    |
    v
load calibration factor
    |
    v
wait three seconds
    |
    v
automatic tare
    |
    v
normal measurement
```

The resulting weight calculation is approximately:

```text
weight = (raw_reading - tare_offset) / calibration_factor
```

After an unexpected restart, the firmware cannot infer both the previous tare offset and the current weight from one raw reading.

A startup threshold or heuristic cannot reliably distinguish between:

- A changed zero offset.
- A real load already present.
- A combination of both.

The firmware must therefore restore a previously validated tare offset or require deliberate user action.

---

## Selected startup policy

The new startup flow will be:

```text
reset
    |
    v
initialize scale
    |
    v
load calibration factor
    |
    v
load stored tare offset
    |
    +--> valid tare
    |        |
    |        v
    |    apply tare offset
    |        |
    |        v
    |    normal measurement
    |
    +--> missing or invalid tare
             |
             v
         TARE_REQUIRED
```

The firmware will no longer perform automatic tare during every startup.

The three-second startup delay existed primarily to allow the scale to settle before automatic tare. It will be removed from the normal startup path when automatic startup tare is removed.

---

## Operational zero reference

For the deployed system, the intended zero reference is:

```text
platform + empty permanent container
```

The displayed weight will therefore represent mainly the contents of the container.

A new tare is required when:

- The container is replaced.
- The mounting is changed.
- The load cell is replaced.
- The mechanical preload changes.
- The empty-container reading drifts enough to require correction.

The firmware cannot verify that the container is truly empty. The operator remains responsible for establishing the correct physical zero condition.

---

## Persistent tare record

The tare offset will be stored separately from the calibration factor.

Calibration represents:

```text
ADC counts per gram
```

Tare represents:

```text
raw ADC count corresponding to the operational zero
```

They change for different reasons and should not share one logical record.

The tare record will contain:

```text
magic identifier
format version
signed 32-bit tare offset
CRC-16/CCITT
```

Expected encoded size:

```text
4-byte magic
2-byte version
4-byte signed offset
2-byte CRC

Total: 12 bytes
```

The record codec must use explicit byte offsets and little-endian encoding, following the existing calibration-record design.

The record must reject:

- Incorrect magic.
- Unsupported version.
- Incorrect CRC.
- Null pointers.
- Buffers smaller than the fixed record size.

Every signed 32-bit offset is a valid representable tare value. Unlike a calibration factor, the offset does not require a non-zero or finite-number check.

---

## EEPROM layout

The existing calibration record occupies the first bytes of storage.

The tare record will occupy a separate non-overlapping region immediately after the calibration record.

Conceptual layout:

```text
EEPROM address 0
    |
    +--> calibration record
    |
    +--> tare record
    |
    +--> unused EEPROM
```

The exact addresses must be defined in one shared storage-layout location rather than duplicated independently in several modules.

The implementation must verify that each complete record fits inside `hal_storage_capacity()`.

Saving calibration must not modify the tare record.

Saving tare must not modify the calibration record.

---

## Scale API changes

The current scale module owns the active runtime offset.

The milestone requires explicit restoration of a stored offset.

Planned API direction:

```cpp
bool scale_tare(void);

void scale_set_offset(
    int32_t tare_offset
);

int32_t scale_get_offset(void);
```

### `scale_tare()`

The current function returns `void` and silently leaves the previous offset unchanged when sample collection fails.

It should return:

```text
true  → a new offset was measured and applied
false → sample collection failed and the previous offset remains active
```

### `scale_set_offset()`

Applies a previously validated persistent offset without reading the HX711.

This function is required during startup restoration and rollback after a storage failure.

### `scale_get_offset()`

Should use the explicit `int32_t` type instead of `long` so its width remains clear in both AVR and native builds.

---

## Safe manual tare transaction

A deliberate manual tare must not be reported as persistent unless the EEPROM write succeeds.

The application will preserve the previous runtime offset:

```text
previous offset
    |
    v
collect new tare samples
    |
    +--> sample failure
    |        |
    |        v
    |    keep previous offset
    |    report failure
    |
    +--> sample success
             |
             v
         apply candidate offset
             |
             v
         save and verify EEPROM
             |
             +--> save success
             |        |
             |        v
             |    mark tare valid
             |    resume normal operation
             |
             +--> save failure
                      |
                      v
                  restore previous offset
                  restore previous validity state
                  report failure
```

This prevents the active runtime offset and the persistent offset from silently diverging after a failed save.

---

## TARE_REQUIRED application state

When no valid tare record exists, normal weight-level reporting is not trustworthy.

The application will enter an explicit state:

```text
TARE_REQUIRED
```

In this state:

- Normal weight measurements are not presented as valid levels.
- The normal level indicator is reset.
- A distinct LED pattern is shown.
- The serial console explains what the operator must do.
- Calibration may still be started.
- A deliberate tare may establish the operational zero.

Suggested console text:

```text
No valid tare offset is stored.
Place the empty container on the platform.
Hold the TARE button to establish zero.
```

The state must not be represented as `VERY_LOW`, because that would falsely imply that a valid empty measurement exists.

---

## TARE_REQUIRED LED pattern

The pattern must be distinct from all existing states.

Selected pattern:

```text
all three LEDs blink together slowly
```

This differs from:

- `VERY_LOW`: low LED blinks quickly.
- Calibration zero: low LED blinks slowly.
- Calibration mass: medium LED blinks slowly.
- Tare in progress: all LEDs remain steadily on.
- Success and error: temporary fast flash sequences.

A new operation-indicator mode will represent `TARE_REQUIRED`.

---

## Physical tare control

During normal operation and `TARE_REQUIRED`, a short accidental press must not redefine zero.

Selected policy:

```text
hold TARE for approximately 3 seconds
```

The existing button module already supports debounced hold events and millisecond-counter overflow-safe timing.

A new configuration constant should describe this duration explicitly, for example:

```cpp
static const uint32_t TARE_START_HOLD_MS =
    3000UL;
```

Contextual behaviour:

```text
normal operation:
    short TARE press → ignored
    long TARE hold   → perform and persist tare

TARE_REQUIRED:
    short TARE press → ignored
    long TARE hold   → perform and persist tare

active calibration:
    short TARE press → cancel calibration immediately
```

Calibration cancellation remains immediate because the button has a different contextual purpose during calibration.

---

## Serial tare policy

For `v1.1`, the serial console is treated as a service and development interface.

Selected initial policy:

```text
t → perform and persist tare immediately
```

This keeps the console simple while the physical field control becomes resistant to accidental presses.

The documentation must clearly state that the operator must first place the empty container on the platform.

A later field-configuration milestone may add:

- Command confirmation.
- A service mode.
- Longer textual commands.
- Restricted production commands.

That additional console policy is not required to solve the power-loss problem.

---

## Calibration interaction

Calibration zero confirmation is also a deliberate zero-reference operation.

When the user confirms the calibration zero condition:

```text
perform tare
    |
    v
save and verify tare offset
    |
    +--> success
    |        |
    |        v
    |    continue to reference-mass stage
    |
    +--> failure
             |
             v
         remain at zero-confirmation stage
         report error
```

The tare offset is valid independently of whether the later calibration-factor step succeeds or is cancelled.

This is intentional:

- The operator explicitly confirmed the empty-container condition.
- Tare and calibration are separate physical properties.
- A valid zero should not be discarded merely because calibration was cancelled later.

If tare sample collection or tare storage fails, calibration must not advance to the reference-mass stage.

---

## Calibration storage independence

The existing calibration factor behaviour remains:

```text
valid stored factor → load it
invalid or absent factor → use compiled fallback
```

The tare record does not depend on the presence of a stored calibration record.

Possible combinations:

```text
valid calibration + valid tare
    → normal operation

fallback calibration + valid tare
    → normal operation using fallback factor

valid calibration + missing tare
    → TARE_REQUIRED

fallback calibration + missing tare
    → TARE_REQUIRED
```

A missing calibration record does not make the zero reference unknowable because the compiled fallback factor still permits measurement.

A missing tare record does make the zero reference unknowable and therefore blocks normal level reporting.

---

## EEPROM clear behaviour

The existing `x` console command continues to clear only the stored calibration record.

No new user-facing tare-clear command is required for this milestone because a new deliberate tare can overwrite the previous stored offset.

The tare-storage module may still provide a `clear()` operation for:

- Native tests.
- Manufacturing or service procedures.
- Future configuration commands.
- Simulating first boot.

Adding a public console command to invalidate tare can be reconsidered later.

---

## Startup messages

### Valid stored tare

Suggested output:

```text
Stored tare offset loaded: <offset>
Normal measurement started.
```

### Missing or invalid stored tare

Suggested output:

```text
No valid tare offset is stored.
Normal level indication is disabled.
Place the empty container on the platform.
Hold TARE for 3 seconds or send 't'.
```

The exact final wording should remain concise because string literals consume Flash.

---

## Blocking behaviour

This milestone does not convert tare into a non-blocking operation.

Tare sample collection remains blocking while the configured number of HX711 samples is collected.

The existing USART receive interrupt may continue buffering input during the operation, and pending commands must still be discarded after the blocking operation.

Non-blocking tare and calibration are deferred to a later milestone.

---

## Failure behaviour

### HX711 tare failure

- Preserve the previous runtime offset.
- Preserve the previous tare-valid state.
- Report an error.
- Return to the previous application state.
- Do not write EEPROM.

### EEPROM tare save failure

- Restore the previous runtime offset.
- Restore the previous tare-valid state.
- Report that the new tare was not applied persistently.
- Do not claim success.

### Invalid or corrupted stored record

- Do not apply its offset.
- Enter `TARE_REQUIRED`.
- Permit the operator to establish a new tare.

### EEPROM capacity failure

- Treat it as a storage failure.
- Do not read or write beyond the available capacity.
- Preserve the previous runtime state.

---

## Native test plan

### Tare record codec

Tests should cover:

- Correct fixed record size.
- Valid positive offset.
- Valid negative offset.
- Zero offset.
- `INT32_MIN`.
- `INT32_MAX`.
- Exact little-endian byte layout.
- Correct magic.
- Correct version.
- Correct CRC.
- Null encode buffer.
- Small encode buffer.
- Null decode buffer.
- Null output pointer.
- Small decode buffer.
- Invalid magic.
- Unsupported version.
- Corrupted payload.
- Corrupted CRC.
- Output value unchanged after failed decode.

### Tare storage

Tests should cover:

- Load valid record.
- Load empty EEPROM.
- Load corrupt record.
- Save and verify.
- Positive offset.
- Negative offset.
- Boundary offsets.
- Read failure.
- Write failure.
- Verification read failure.
- Verification mismatch.
- Insufficient EEPROM capacity.
- Clear and verify.
- Clear write failure.
- Clear verification failure.
- Calibration region remains unchanged.
- Tare writes begin at the expected address.

### Scale

Tests should cover:

- Initial offset is zero.
- Restore positive offset.
- Restore negative offset.
- `scale_get_offset()` returns the restored value.
- Successful tare returns true and applies the measured offset.
- Failed tare returns false and preserves the previous offset.
- Weight conversion uses a restored offset.
- Net-count conversion uses a restored offset.

### Buttons and indicators

Existing button hold tests should remain valid.

Operation-indicator tests should add:

- `TARE_REQUIRED` starts with the intended LED state.
- All LEDs change together.
- The slow blink period is respected.
- Clearing the mode restores normal indicator ownership.

---

## Physical validation plan

Validate on the real Nano:

1. Clear or invalidate the tare record.
2. Power-cycle the board.
3. Confirm `TARE_REQUIRED`.
4. Confirm that no normal level is reported.
5. Place the empty permanent container on the platform.
6. Hold TARE for three seconds.
7. Confirm that tare completes and is stored.
8. Add a known load.
9. Confirm that the weight and level are correct.
10. Power off while the load remains present.
11. Power on again without removing the load.
12. Confirm that the previous tare is restored.
13. Confirm that the existing load is still reported.
14. Press TARE briefly and confirm that zero does not change.
15. Hold TARE with the empty container and confirm a new offset is stored.
16. Start calibration.
17. Confirm zero with the empty container.
18. Cancel after the zero stage.
19. Power-cycle and confirm that the newly confirmed tare remains stored.
20. Repeat the upload sequence and confirm that the bootloader remains functional.

---

## Planned commits

The milestone should remain incremental.

Proposed sequence:

```text
docs: define safe startup tare behavior
test: add tare record codec tests
feat: add tare record codec
test: add persistent tare storage tests
feat: add persistent tare storage
test: add restorable scale offset tests
feat: support restoring the scale offset
test: add tare-required indicator tests
feat: add tare-required indicator mode
feat: restore persistent tare during startup
feat: require deliberate physical tare
feat: persist calibration zero tare
docs: record safe startup tare validation
```

The sequence may be adjusted if compilation dependencies require a different split, but each commit should have one clear responsibility.

---

## Out of scope

This milestone does not include:

- Non-blocking tare.
- Non-blocking calibration.
- Watchdog configuration.
- Brown-out reset diagnosis.
- Stable-weight detection.
- Advanced filtering.
- EEPROM wear levelling.
- Multiple tare-record slots.
- Field-editable thresholds.
- LoRa.
- Alternative ADC backends.
- Source-tree reorganization.

---

## Definition of done

The milestone is complete when:

- Automatic startup tare has been removed.
- A valid tare offset is restored after reset and power loss.
- A partially filled container remains correctly measured after restart.
- Missing, invalid or corrupted tare data produces `TARE_REQUIRED`.
- Normal level indication is suppressed until a valid tare exists.
- The physical tare control requires a deliberate hold.
- Serial tare remains available as a documented service operation.
- Successful manual tare is verified in EEPROM.
- Failed persistence restores the previous runtime offset.
- Calibration zero confirmation persists the tare before advancing.
- Calibration and tare records remain independent.
- Existing calibration fallback behaviour remains valid.
- Existing HX711, level, console and EEPROM functionality remains valid.
- All native tests pass.
- Physical reset and power-cycle scenarios pass.
- Memory usage is recorded.
- Documentation is updated.
- The branch is merged into `main`.
- The annotated `v1.1-safe-startup-tare` tag is published.
