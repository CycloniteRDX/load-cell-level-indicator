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
