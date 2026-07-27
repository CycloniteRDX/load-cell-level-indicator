# Direct AVR EEPROM Backend Notes

## 1. Purpose

The purpose of this milestone is to replace the Arduino EEPROM storage backend with a direct ATmega328P EEPROM implementation.

The current calibration architecture is:

```text
calibration_storage.cpp
        |
        +--> calibration_record.cpp
        |
        +--> hal_storage.h
                 |
                 v
        hal_storage_arduino.cpp
                 |
                 v
        Arduino EEPROM library
```

The target architecture is:

```text
calibration_storage.cpp
        |
        +--> calibration_record.cpp
        |
        +--> hal_storage.h
                 |
                 v
          hal_storage_avr.c
                 |
                 v
        ATmega328P EEPROM registers
```

The public storage HAL will remain unchanged:

```c
size_t hal_storage_capacity(void);

bool hal_storage_read(
    size_t address,
    uint8_t *destination,
    size_t length
);

bool hal_storage_write(
    size_t address,
    const uint8_t *source,
    size_t length
);
```

No change is planned for the calibration-storage or calibration-record public behaviour.

---

## 2. Current backend

The current production backend is:

```text
src/hal_storage_arduino.cpp
```

It uses:

```text
EEPROM.length()
EEPROM.read()
EEPROM.update()
```

The backend already provides:

* Capacity reporting.
* Range validation.
* Zero-length operations.
* Null-pointer validation.
* Byte-range reads.
* Update-style writes.
* Avoidance of unnecessary physical writes.

The direct AVR backend must preserve these semantics.

The Arduino implementation will remain in the repository as a readable reference but will be excluded from the Nano production build.

---

## 3. New backend

The direct backend will be:

```text
src/hal_storage_avr.c
```

It will include:

```c
#include <avr/io.h>
```

and access the EEPROM through:

```text
EEAR
EEDR
EECR
SPMCSR
```

The backend will not include:

```text
Arduino.h
EEPROM.h
avr-libc EEPROM helper functions
```

The implementation will directly control the ATmega328P registers.

---

## 4. EEPROM capacity

The ATmega328P provides 1024 addressable EEPROM bytes.

The last valid EEPROM address is exposed by the AVR device header as:

```c
E2END
```

The HAL capacity will therefore be calculated as:

```c
(size_t)E2END + 1U
```

For the ATmega328P:

```text
E2END:    1023
Capacity: 1024 bytes
```

The implementation will derive the capacity from the selected AVR device header instead of duplicating the numeric value in application code.

---

## 5. Range validation

The direct backend will preserve the existing subtraction-based range check.

A requested range is valid when:

```text
address <= capacity
length  <= capacity - address
```

This avoids possible overflow in:

```c
address + length
```

Examples for a 1024-byte EEPROM:

```text
Address  Length  Result
0        12      valid
1012     12      valid
1013     12      invalid
1024     0       valid
1024     1       invalid
```

Zero-length operations will succeed without accessing EEPROM.

A null source or destination will only be rejected when the requested length is non-zero.

---

## 6. EEPROM busy state

The EEPROM controller exposes its write-in-progress state through:

```text
EECR.EEPE
```

Before reading or starting another write, the backend must wait until:

```c
(EECR & _BV(EEPE)) == 0U
```

The waiting operation will not disable interrupts.

Timer1, Serial and other interrupt-driven project functions may therefore continue operating while the EEPROM controller completes a physical write.

---

## 7. Byte-read sequence

A direct EEPROM byte read will use this sequence:

```text
Wait until EEPE is clear
        |
        v
Load the address into EEAR
        |
        v
Set EERE in EECR
        |
        v
Read the byte from EEDR
```

The address-trigger-read sequence will be protected by the critical-section HAL.

The potentially long wait for an earlier write to finish will occur before entering the critical section.

The critical section will therefore last only a small number of CPU instructions.

---

## 8. Byte-write sequence

A direct EEPROM byte write will use this sequence:

```text
Wait until EEPE is clear
        |
        v
Wait until SPMEN is clear
        |
        v
Load address into EEAR
        |
        v
Load data into EEDR
        |
        v
Select atomic erase-and-write mode
        |
        v
Enter critical section
        |
        v
Set EEMPE
        |
        v
Set EEPE within four CPU cycles
        |
        v
Restore previous interrupt state
        |
        v
Wait until the physical write completes
```

Only the timed `EEMPE` to `EEPE` sequence requires interrupts to be disabled.

The backend will use:

```text
hal_critical_enter()
hal_critical_exit()
```

instead of calling `cli()` directly.

This preserves the previous interrupt state and keeps interrupt management behind the existing project HAL.

---

## 9. Flash self-programming check

EEPROM programming cannot begin while the CPU is performing Flash self-programming.

The backend will wait until:

```c
(SPMCSR & _BV(SPMEN)) == 0U
```

before starting an EEPROM write.

The application does not normally perform Flash self-programming after startup, but keeping this check follows the complete hardware programming sequence and protects the backend from future bootloader-related changes.

The wait will occur with interrupts enabled.

---

## 10. Critical-section scope

The EEPROM controller needs a protected timed sequence:

```text
EEMPE -> EEPE
```

The complete physical EEPROM programming time must not run inside a critical section.

Incorrect approach:

```text
disable interrupts
wait several milliseconds
restore interrupts
```

Planned approach:

```text
wait until ready with interrupts enabled
prepare address and data
disable interrupts briefly
set EEMPE
set EEPE
restore interrupts
wait for completion with interrupts enabled
```

This keeps Timer1 and Serial interruption latency low.

---

## 11. Update-style writes

The Arduino backend currently uses:

```text
EEPROM.update()
```

The direct backend will preserve that behaviour.

Before programming a byte, it will:

```text
Read the existing value
        |
        v
Compare it with the requested value
        |
        +--> equal: skip physical write
        |
        +--> different: perform EEPROM write
```

This avoids unnecessary EEPROM erase-and-write cycles.

`hal_storage_write()` will still report success when every requested byte already contains the requested value.

---

## 12. EEPROM programming mode

The direct backend will use the default atomic programming mode:

```text
EEPM1 = 0
EEPM0 = 0
```

This performs:

```text
erase old byte
+
write new byte
```

as one EEPROM programming operation.

The backend will explicitly clear both programming-mode bits before starting a write.

It will not use:

* Erase-only mode.
* Write-only mode.
* EEPROM-ready interrupts.
* Asynchronous queued writes.

---

## 13. Blocking behaviour

The public HAL remains synchronous.

When:

```c
hal_storage_write(...)
```

returns `true`, all changed bytes will have completed their physical EEPROM programming operation.

Unchanged bytes will be skipped.

This preserves a simple contract for:

```text
calibration_storage_save()
calibration_storage_clear()
```

Both higher-level operations already perform read-back verification after writing.

---

## 14. Interrupt and concurrency model

The project will not use the EEPROM-ready interrupt.

EEPROM access will only be initiated from normal application context.

The backend is not intended to be called from an interrupt service routine.

The brief critical sections prevent interruption during register sequences that must remain coherent.

The physical programming wait keeps global interrupts enabled.

---

## 15. Compatibility

This milestone changes only the physical storage backend.

It does not change:

```text
Storage address
Record size
Magic value
Format version
Float representation
Byte order
CRC algorithm
Clear behaviour
```

The calibration record remains:

```text
12 bytes at EEPROM address zero
```

A calibration written by `hal_storage_arduino.cpp` must remain readable after switching to `hal_storage_avr.c`.

A calibration written by the AVR backend must also remain compatible with the existing record codec.

---

## 16. Backend selection

During development, both implementations will exist:

```text
src/hal_storage_arduino.cpp
src/hal_storage_avr.c
```

Only one backend may be linked into the Nano firmware because both define:

```text
hal_storage_capacity()
hal_storage_read()
hal_storage_write()
```

The development sequence will initially keep:

```text
hal_storage_arduino.cpp active
hal_storage_avr.c excluded
```

After isolated compilation and review, the build filter will change to:

```text
hal_storage_arduino.cpp excluded
hal_storage_avr.c active
```

Native tests will continue using:

```text
fake_hal_storage.cpp
```

and will not compile either production backend.

---

## 17. Automated validation

The direct AVR file will first be compiled in isolation with AVR-GCC.

The production firmware will then be compiled with:

```text
pio run -e nanoatmega328new
```

The complete native regression will remain:

```text
native_button:                       10
native_hx711:                        18
native_level_indicator:              14
native_operation_indicator:          14
native_scale:                        32
native_calibration_storage:          40

Total:                              128
```

The existing native calibration-storage suite will continue exercising:

* Record encoding.
* Record validation.
* Successful load.
* Successful save.
* Read-back verification.
* Clear operations.
* Capacity failures.
* Read failures.
* Write failures.
* Corrupted storage.
* Caller-output preservation.

The native tests validate the layers above the physical AVR register backend.

---

## 18. Physical validation

Before activating the direct backend:

1. Store a known calibration using the Arduino EEPROM backend.
2. Record its calibration-factor value.
3. Do not clear EEPROM.

After activating the direct backend:

1. Upload the new firmware.
2. Restart the Nano.
3. Confirm that the existing factor loads unchanged.
4. Perform a normal tare.
5. Perform a new calibration.
6. Restart the Nano.
7. Confirm that the new factor persists.
8. Clear persistent calibration.
9. Restart the Nano.
10. Confirm that the default factor is selected.
11. Perform another calibration.
12. Remove and restore power.
13. Confirm that the factor survives a full power cycle.

The following behaviour must also remain functional:

* HX711 readings.
* Buttons.
* Indicator patterns.
* Serial output.
* Timer1 timekeeping.
* Startup tare.

---

## 19. Memory comparison

The production build before selecting the AVR backend will be recorded.

The production build after selecting the AVR backend will also be recorded.

The comparison will include:

```text
Static SRAM
Flash usage
```

Replacing the Arduino EEPROM wrapper may reduce flash usage, but no target reduction is required for the milestone.

Correctness and architectural separation are the primary objectives.

---

## 20. Non-objectives

This milestone will not:

* Change the storage HAL interface.
* Change the calibration record.
* Change the record version.
* Change the CRC.
* Add wear levelling.
* Add redundant records.
* Add asynchronous EEPROM writes.
* Use the EEPROM-ready interrupt.
* Add multiple calibration profiles.
* Move the record from address zero.
* Remove the Arduino Core.
* Modify Serial communication.
* Modify the calibration state machine.
* Change application behaviour.

---

## 21. Planned commits

### Design documentation

```text
docs: define direct AVR EEPROM strategy
```

### Development isolation

```text
build: isolate AVR EEPROM backend during development
```

### Capacity and byte reads

```text
feat: add AVR EEPROM byte reads
```

### Update-style byte writes

```text
feat: add AVR EEPROM update writes
```

### Production selection

```text
build: select AVR EEPROM backend
```

### Final validation

```text
docs: record direct AVR EEPROM validation
```

---

## 22. Definition of done

This milestone will be complete when:

* [ ] The direct AVR EEPROM design is documented.
* [ ] `hal_storage_avr.c` exists.
* [ ] Capacity is derived from `E2END`.
* [ ] Range validation is overflow-safe.
* [ ] Zero-length operations behave correctly.
* [ ] Invalid null pointers are rejected.
* [ ] EEPROM busy state is respected.
* [ ] Direct byte reads use `EEAR`, `EERE` and `EEDR`.
* [ ] Direct byte writes use `EEAR`, `EEDR`, `EEMPE` and `EEPE`.
* [ ] Flash self-programming state is checked.
* [ ] Programming mode is explicitly selected.
* [ ] The timed write sequence is protected.
* [ ] The previous interrupt state is restored.
* [ ] Interrupts remain enabled during physical programming waits.
* [ ] Unchanged bytes are not physically rewritten.
* [ ] The write operation is synchronous.
* [ ] The Arduino storage backend remains as a reference.
* [ ] The AVR backend is active in the Nano build.
* [ ] All 128 native tests pass.
* [ ] The Nano firmware compiles.
* [ ] Existing stored calibration remains readable.
* [ ] A new calibration can be saved.
* [ ] A new calibration survives restart.
* [ ] A new calibration survives a power cycle.
* [ ] Persistent calibration can be cleared.
* [ ] Default calibration is selected after clearing.
* [ ] No application regression is detected.
* [ ] SRAM and flash usage are recorded.
* [ ] Final validation is documented.

## 23. Final architecture

The direct AVR EEPROM backend has been implemented and selected successfully for the Arduino Nano production build.

The final calibration-storage architecture is:

```text
Application
    |
    v
calibration_storage.cpp
    |
    +--> calibration_record.cpp
    |
    +--> hal_storage.h
             |
             v
       hal_storage_avr.c
             |
             v
      ATmega328P EEPROM
```

The previous Arduino backend remains available as a reference:

```text
src/hal_storage_arduino.cpp
```

but is excluded from the Arduino Nano production build.

The active production backend is:

```text
src/hal_storage_avr.c
```

No changes were required in:

```text
src/calibration_storage.cpp
src/calibration_record.cpp
include/hal_storage.h
```

This confirms that the storage HAL successfully isolates the higher-level calibration logic from the physical EEPROM implementation.

---

## 24. Production backend selection

The Nano production build now excludes the temporary Arduino implementations:

```ini
build_src_filter =
    +<*>
    -<hal_gpio_arduino.cpp>
    -<hal_time_arduino.cpp>
    -<hal_storage_arduino.cpp>
```

The active direct AVR backends are:

```text
src/hal_gpio_avr.c
src/hal_time_avr.c
src/hal_storage_avr.c
src/hal_critical_avr.c
```

The Arduino Core remains temporarily responsible for:

```text
startup
setup() and loop()
Serial
remaining direct delay() calls
```

The Arduino EEPROM library is no longer part of the active calibration-storage path.

---

## 25. EEPROM capacity

The backend derives the EEPROM capacity from:

```c
E2END
```

The capacity calculation is:

```c
(size_t)E2END + 1U
```

For the ATmega328P:

```text
Last valid address: 1023
Total capacity:     1024 bytes
```

The capacity is therefore obtained from the selected device header instead of being duplicated as a project constant.

---

## 26. Range validation

The backend validates byte ranges using:

```text
address <= capacity
length  <= capacity - address
```

This avoids an overflow-prone expression such as:

```c
address + length
```

The final behaviour includes:

```text
Address  Length  Result
0        12      valid
1012     12      valid
1013     12      invalid
1023     1       valid
1024     0       valid
1024     1       invalid
```

A zero-length operation succeeds without accessing EEPROM.

A null source or destination is accepted only when the requested length is zero.

---

## 27. Direct EEPROM reads

Byte reads use the ATmega328P EEPROM registers directly:

```text
EEAR
EECR
EEDR
```

The sequence is:

```text
Wait until EEPE is clear
        |
        v
Enter a critical section
        |
        v
Load the EEPROM address into EEAR
        |
        v
Set EERE
        |
        v
Read EEDR
        |
        v
Restore the previous interrupt state
```

The potentially long wait for an earlier EEPROM write occurs before entering the critical section.

The critical section therefore contains only the register sequence that must remain coherent.

---

## 28. Direct EEPROM writes

Byte writes use:

```text
EEAR
EEDR
EECR
SPMCSR
```

The sequence is:

```text
Wait until EEPE is clear
        |
        v
Wait until SPMEN is clear
        |
        v
Load EEAR and EEDR
        |
        v
Select erase-and-write mode
        |
        v
Enter a critical section
        |
        v
Set EEMPE
        |
        v
Set EEPE within the required cycle window
        |
        v
Restore the previous interrupt state
        |
        v
Wait for physical programming to finish
```

The backend explicitly selects:

```text
EEPM1 = 0
EEPM0 = 0
```

This selects the atomic erase-and-write programming mode.

---

## 29. Timed write-start sequence

The EEPROM controller requires `EEPE` to be set shortly after `EEMPE`.

The backend guarantees this using consecutive inline-assembly instructions:

```asm
sbi EECR, EEMPE
sbi EECR, EEPE
```

The sequence is executed with interrupts temporarily disabled.

This avoids relying on the compiler to generate an appropriate instruction sequence from separate C expressions.

The previous interrupt state is restored through:

```text
hal_critical_enter()
hal_critical_exit()
```

The backend does not call `cli()` or modify `SREG` directly outside the existing critical-section HAL.

---

## 30. Critical-section duration

Global interrupts are disabled only while:

```text
loading EEAR
loading EEDR
selecting the programming mode
setting EEMPE
setting EEPE
```

The physical EEPROM programming time does not execute inside the critical section.

After starting the write:

```text
interrupt state is restored
        |
        v
Timer1 and other interrupts may execute
        |
        v
the backend waits synchronously for EEPE to clear
```

This preserves the synchronous storage-HAL contract without blocking interrupts for the complete EEPROM programming duration.

---

## 31. Flash self-programming protection

Before starting an EEPROM write, the backend waits until:

```c
(SPMCSR & _BV(SPMEN)) == 0U
```

The current application does not normally perform Flash self-programming after startup.

The check is nevertheless retained to follow the complete hardware programming sequence and protect against future bootloader or self-programming interactions.

This waiting period occurs with the existing interrupt state unchanged.

---

## 32. Update-style writes

The direct backend preserves the previous `EEPROM.update()` behaviour.

For every requested byte:

```text
Read the existing EEPROM value
        |
        v
Compare it with the requested value
        |
        +--> Equal:
        |       Skip the physical write
        |
        +--> Different:
                Perform erase-and-write
```

This avoids unnecessary physical EEPROM programming cycles.

`hal_storage_write()` still returns success when all requested bytes already contain the desired values.

The higher-level calibration-storage module continues performing its own read-back verification.

---

## 33. Synchronous behaviour

The public storage HAL remains synchronous.

When:

```c
hal_storage_write(
    address,
    source,
    length
);
```

returns successfully:

* Every changed byte has completed its physical EEPROM write.
* Every unchanged byte has been verified and skipped.
* The EEPROM controller is no longer busy.
* Higher-level code may immediately perform read-back verification.

This behaviour remains compatible with:

```text
calibration_storage_save()
calibration_storage_clear()
```

---

## 34. Calibration-record compatibility

The physical storage backend was changed without modifying the stored record format.

The following remain unchanged:

```text
Storage address:       0
Record size:           12 bytes
Magic:                 0x4C43414C
Format version:        1
Byte order:            little endian
Float representation:  32-bit IEEE-754
Checksum:              CRC-16/CCITT
CRC polynomial:        0x1021
CRC initial value:     0xFFFF
```

A calibration written by the previous Arduino EEPROM backend remained readable after selecting the direct AVR backend.

This confirms byte-for-byte compatibility between both physical storage implementations.

---

## 35. Native regression

The complete native regression continues passing:

```text
native_button:                       10 tests
native_hx711:                        18 tests
native_level_indicator:              14 tests
native_operation_indicator:          14 tests
native_scale:                        32 tests
native_calibration_storage:          40 tests

Total:                              128 tests
Failures:                             0
```

Validation commands:

```text
pio test -e native_button
pio test -e native_hx711
pio test -e native_level_indicator
pio test -e native_operation_indicator
pio test -e native_scale
pio test -e native_calibration_storage
```

The native calibration-storage tests continue using:

```text
fake_hal_storage.cpp
```

They validate the unchanged storage policy and record layers independently from the physical AVR register implementation.

---

## 36. Physical validation

The direct AVR EEPROM backend was validated successfully on the physical Arduino Nano.

The following behaviour was confirmed:

* [x] Firmware starts normally.
* [x] A calibration written by the Arduino EEPROM backend remains readable.
* [x] The previously stored factor loads unchanged.
* [x] Existing record compatibility is preserved.
* [x] Startup tare remains functional.
* [x] Manual tare remains functional.
* [x] A new calibration can be completed.
* [x] A new calibration factor is written successfully.
* [x] The saved factor survives reset.
* [x] The saved factor survives a complete power cycle.
* [x] The saved factor loads correctly after restart.
* [x] Persistent calibration can be cleared.
* [x] The cleared record is rejected after restart.
* [x] The configured default factor is selected after clearing.
* [x] A new calibration can be written after clearing.
* [x] The new calibration survives another power cycle.
* [x] HX711 readings remain functional.
* [x] No repeated HX711 timeouts occur.
* [x] Buttons remain functional.
* [x] Short and long presses remain functional.
* [x] Button debounce remains correct.
* [x] Level indicators remain functional.
* [x] Operation-indicator patterns remain functional.
* [x] Timer1-based timing remains functional.
* [x] Serial output remains functional.

No application-level regression was detected.

---

## 37. Memory usage

Previous calibration-storage HAL milestone:

```text
RAM:   748 bytes
Flash: 12606 bytes
```

Direct AVR EEPROM backend:

```text
RAM:   748 bytes
Flash: 12630 bytes
```

The memory comparison is informational.

The primary objective of this milestone is replacing the physical Arduino EEPROM dependency while preserving correctness and compatibility.

---

## 38. Architectural result

The project now owns direct AVR implementations for:

```text
GPIO:
hal_gpio_avr.c
    -> DDRx / PORTx / PINx

Critical sections:
hal_critical_avr.c
    -> SREG / cli()

Time:
hal_time_avr.c
    -> Timer1 / ISR / cycle-counted delays

Non-volatile storage:
hal_storage_avr.c
    -> EEAR / EEDR / EECR / SPMCSR
```

The dependency direction is:

```text
Application logic
        |
        v
Project HAL interfaces
        |
        v
Direct AVR register backends
```

No Arduino-specific storage type or function is exposed above the storage HAL.

The Arduino EEPROM backend remains in the repository as a reference and alternative implementation.

---

## 39. Definition of done

This milestone is complete because:

* [x] The direct AVR EEPROM design is documented.
* [x] `hal_storage_avr.c` exists.
* [x] Capacity is derived from `E2END`.
* [x] Range validation is overflow-safe.
* [x] Zero-length operations behave correctly.
* [x] Invalid null pointers are rejected.
* [x] EEPROM busy state is respected.
* [x] Direct byte reads use `EEAR`, `EERE` and `EEDR`.
* [x] Direct byte writes use `EEAR`, `EEDR`, `EEMPE` and `EEPE`.
* [x] Flash self-programming state is checked.
* [x] Programming mode is explicitly selected.
* [x] The timed write sequence is protected.
* [x] The previous interrupt state is restored.
* [x] Interrupts remain enabled during physical programming waits.
* [x] Unchanged bytes are not physically rewritten.
* [x] The write operation is synchronous.
* [x] The Arduino storage backend remains as a reference.
* [x] The AVR backend is active in the Nano build.
* [x] All 128 native tests pass.
* [x] The Nano firmware compiles.
* [x] Existing stored calibration remains readable.
* [x] A new calibration can be saved.
* [x] A new calibration survives restart.
* [x] A new calibration survives a power cycle.
* [x] Persistent calibration can be cleared.
* [x] Default calibration is selected after clearing.
* [x] No application regression was detected.
* [x] SRAM and flash usage are recorded.
* [x] Final validation is documented.
