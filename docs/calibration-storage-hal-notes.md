# Calibration Storage HAL Notes

## 1. Purpose

The purpose of this milestone is to separate calibration-record logic from physical non-volatile storage access.

The current calibration-storage module depends directly on:

```text
Arduino.h
EEPROM.h
Arduino EEPROM.get()
Arduino EEPROM.put()
Arduino EEPROM.length()
```

This prevents the module from being compiled and tested natively without emulating the Arduino EEPROM library.

The new architecture will introduce:

* An explicit calibration-record format.
* A storage hardware-abstraction layer.
* An Arduino EEPROM storage backend.
* A fake storage backend for native tests.
* Native tests for record validation and storage operations.

The public application interface will remain unchanged:

```c
bool calibration_storage_load(
    float *calibration_factor
);

bool calibration_storage_save(
    float calibration_factor
);

bool calibration_storage_clear(void);
```

---

## 2. Current implementation

The current file:

```text
src/calibration_storage.cpp
```

owns all of the following responsibilities:

* Calibration-factor validation.
* Calibration-record structure definition.
* Format magic and version.
* CRC-16 calculation.
* EEPROM-capacity validation.
* EEPROM reads.
* EEPROM writes.
* Write verification.
* Record invalidation.

Its current dependency structure is:

```text
app.cpp
    |
    v
calibration_storage.cpp
    |
    v
Arduino EEPROM library
    |
    v
ATmega328P EEPROM
```

This implementation works on the Arduino Nano, but record logic cannot be tested independently from Arduino.

---

## 3. Problems to solve

### Direct Arduino dependency

`calibration_storage.cpp` includes:

```cpp
#include <Arduino.h>
#include <EEPROM.h>
```

This prevents ordinary native compilation.

### Implicit structure layout

The stored record is currently written using:

```cpp
EEPROM.put(
    CALIBRATION_EEPROM_ADDRESS,
    record
);
```

This writes the in-memory representation of a C++ structure.

That representation may depend on:

* Processor endianness.
* Compiler alignment rules.
* Structure padding.
* Floating-point representation.
* Target ABI.

The ATmega328P and the development computer may therefore produce different in-memory layouts for the same structure.

Native tests must not rely on the host compiler accidentally matching the AVR layout.

### Mixed responsibilities

CRC calculation, factor validation, binary formatting and EEPROM access currently live in one file.

A failure in any layer is difficult to simulate independently.

---

## 4. Target architecture

The planned architecture is:

```text
Application
    |
    v
calibration_storage.h
    |
    v
calibration_storage.cpp
    |
    +---------------------------+
    |                           |
    v                           v
calibration_record.cpp      hal_storage.h
    |                           |
    |                           +--> hal_storage_arduino.cpp
    |                           |
    |                           +--> fake_hal_storage.cpp
    |                           |
    |                           +--> future hal_storage_avr.c
    |
    v
Explicit 12-byte record
```

Each layer will have one primary responsibility.

### `calibration_record`

Responsible for:

* Calibration-factor validation.
* Record encoding.
* Record decoding.
* Magic validation.
* Version validation.
* CRC generation.
* CRC validation.
* Explicit byte ordering.

### `calibration_storage`

Responsible for:

* Checking available storage capacity.
* Requesting record encoding.
* Reading bytes through the storage HAL.
* Writing bytes through the storage HAL.
* Verifying saved records.
* Invalidating stored records.
* Preserving caller outputs after failure.

### Storage HAL

Responsible for:

* Reporting storage capacity.
* Reading byte ranges.
* Writing byte ranges.
* Reporting low-level failures.

The storage HAL does not understand calibration factors, record versions or checksums.

---

## 5. Storage HAL interface

The planned public HAL header is:

```text
include/hal_storage.h
```

The intended interface is:

```c
#ifndef HAL_STORAGE_H
#define HAL_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
```

The interface is byte-oriented deliberately.

It does not expose C or C++ structures to the backend.

This prevents the physical storage implementation from depending on structure layout.

---

## 6. Storage HAL behaviour

### Capacity

```c
size_t hal_storage_capacity(void);
```

returns the number of addressable bytes.

For an ATmega328P EEPROM backend, this is expected to correspond to the device EEPROM capacity.

### Read

```c
bool hal_storage_read(
    size_t address,
    uint8_t *destination,
    size_t length
);
```

must:

* Reject a null destination when length is non-zero.
* Reject ranges outside the available storage.
* Copy exactly the requested bytes.
* Return false when the operation cannot be completed.

A zero-length read may succeed without accessing storage.

### Write

```c
bool hal_storage_write(
    size_t address,
    const uint8_t *source,
    size_t length
);
```

must:

* Reject a null source when length is non-zero.
* Reject ranges outside the available storage.
* Write exactly the requested bytes.
* Return false when the operation cannot be completed.

The Arduino backend should use update-style EEPROM writes so unchanged bytes are not physically rewritten unnecessarily.

A zero-length write may succeed without accessing storage.

---

## 7. Arduino storage backend

The initial active backend will be:

```text
src/hal_storage_arduino.cpp
```

It will wrap the Arduino EEPROM library.

The backend may use:

```cpp
EEPROM.length()
EEPROM.read()
EEPROM.update()
```

The higher-level calibration module will no longer include `EEPROM.h`.

This backend will preserve current firmware behaviour while creating a clean boundary for future replacement.

The later direct AVR backend will implement the same interface without Arduino.

---

## 8. Explicit calibration-record format

The new record will have a fixed size:

```text
12 bytes
```

The byte layout will be:

```text
Offset  Size  Field
------  ----  --------------------------
0       4     Magic value
4       2     Format version
6       4     Calibration-factor bits
10      2     CRC-16/CCITT
```

Symbolically:

```text
+0  magic[0]
+1  magic[1]
+2  magic[2]
+3  magic[3]

+4  version[0]
+5  version[1]

+6  calibration_factor_bits[0]
+7  calibration_factor_bits[1]
+8  calibration_factor_bits[2]
+9  calibration_factor_bits[3]

+10 checksum[0]
+11 checksum[1]
```

All multibyte values will use an explicitly defined little-endian representation.

The record will no longer depend on:

```text
sizeof(calibration_record_t)
compiler padding
native host alignment
native host endianness
```

---

## 9. Existing format values

The current format constants will be preserved:

```text
Magic:   0x4C43414C
Version: 1
```

The CRC algorithm will also remain:

```text
CRC-16/CCITT
Polynomial:    0x1021
Initial value: 0xFFFF
```

The CRC covers:

```text
magic
version
calibration-factor binary representation
```

It does not include the stored checksum itself.

Preserving these values is intended to keep the explicit format compatible with calibration records already written by the ATmega328P implementation.

Compatibility must be verified physically before closing the milestone:

1. Save a calibration using the current firmware.
2. Install the refactored firmware without clearing EEPROM.
3. Confirm that the previous calibration is loaded.
4. Confirm that its numerical value is unchanged.

---

## 10. Floating-point representation

The stored calibration factor uses the binary representation of a 32-bit `float`.

The implementation will require:

```c
sizeof(float) == sizeof(uint32_t)
```

A compile-time assertion will reject unsupported platforms.

`memcpy()` will be used when converting between:

```text
float
uint32_t
```

This avoids violating strict-aliasing rules.

The record codec will never cast a `float *` directly to a `uint32_t *`.

---

## 11. Calibration-factor validation

A valid calibration factor must:

* Not be NaN.
* Not be positive infinity.
* Not be negative infinity.
* Have an absolute magnitude of at least `0.000001F`.

Positive and negative factors remain valid.

The exact boundary values:

```text
0.000001F
-0.000001F
```

will be accepted.

Invalid factors must never be encoded or returned to the application.

---

## 12. Record codec interface

The internal record module will be:

```text
src/calibration_record.h
src/calibration_record.cpp
```

Its planned interface is:

```c
#define CALIBRATION_RECORD_SIZE 12U

bool calibration_record_factor_is_valid(
    float calibration_factor
);

bool calibration_record_encode(
    float calibration_factor,
    uint8_t *record_bytes,
    size_t record_size
);

bool calibration_record_decode(
    const uint8_t *record_bytes,
    size_t record_size,
    float *calibration_factor
);
```

Encoding must fail when:

* The output pointer is null.
* The provided buffer is too small.
* The calibration factor is invalid.
* The platform does not provide a supported float format.

Decoding must fail when:

* The input pointer is null.
* The output pointer is null.
* The provided buffer is too small.
* The magic value is incorrect.
* The version is unsupported.
* The factor is invalid.
* The checksum is incorrect.

A failed decode must not modify the caller's output variable.

---

## 13. Save operation

The refactored save sequence will be:

```text
Validate factor
      |
      v
Encode 12-byte record
      |
      v
Check storage capacity
      |
      v
Write through HAL
      |
      v
Read record back through HAL
      |
      v
Decode and validate read-back record
      |
      v
Compare verified factor
```

`calibration_storage_save()` returns true only when all stages succeed.

It must return false for:

* Invalid factors.
* Insufficient capacity.
* HAL write failure.
* HAL verification-read failure.
* Corrupted verification data.
* A verified factor different from the requested factor.

---

## 14. Load operation

The refactored load sequence will be:

```text
Validate output pointer
      |
      v
Check storage capacity
      |
      v
Read 12 bytes through HAL
      |
      v
Decode and validate record
      |
      v
Publish calibration factor
```

The caller's output variable must remain unchanged when any stage fails.

---

## 15. Clear operation

Clearing the record does not require erasing all 12 bytes.

The implementation will invalidate the magic field.

The planned sequence is:

```text
Write invalid magic bytes
      |
      v
Read magic bytes back
      |
      v
Confirm that valid magic is absent
```

The currently active runtime calibration factor is not changed.

Only persistent storage is invalidated.

---

## 16. Native fake storage

The native suite will provide:

```text
test/test_calibration_storage/fake_hal_storage.cpp
test/test_calibration_storage/fake_hal_storage.h
```

The fake will contain a controllable byte array.

It should support:

* Selecting total storage capacity.
* Preloading arbitrary bytes.
* Inspecting stored bytes.
* Resetting memory before each test.
* Forcing read failures.
* Forcing write failures.
* Failing a selected read call.
* Failing a selected write call.
* Corrupting data after a write.
* Recording addresses and lengths.
* Counting read calls.
* Counting write calls.
* Detecting out-of-range accesses.

No Arduino or physical EEPROM will be required.

---

## 17. Planned record tests

The native tests should cover:

* Encoding valid positive factors.
* Encoding valid negative factors.
* Exact 12-byte output size.
* Stable little-endian field ordering.
* Correct magic bytes.
* Correct version bytes.
* Correct floating-point bytes.
* Correct checksum bytes.
* Decoding valid records.
* Round-trip encode and decode.
* Rejection of null pointers.
* Rejection of short buffers.
* Rejection of zero.
* Rejection of NaN.
* Rejection of infinity.
* Rejection of factors below the minimum magnitude.
* Acceptance of exact minimum-magnitude boundaries.
* Rejection of incorrect magic.
* Rejection of unsupported versions.
* Rejection of corrupted factor data.
* Rejection of incorrect checksums.
* Preservation of output values after decode failure.

---

## 18. Planned storage-operation tests

The native tests should cover:

### Load

* Successful load.
* Null output pointer.
* Insufficient storage capacity.
* HAL read failure.
* Empty storage.
* Incorrect magic.
* Unsupported version.
* Invalid factor.
* Incorrect checksum.
* Preservation of the output value after failure.

### Save

* Successful save.
* Positive factor.
* Negative factor.
* Invalid factor.
* Insufficient capacity.
* HAL write failure.
* Verification-read failure.
* Corrupted read-back record.
* Mismatched verified factor.
* Correct write address.
* Correct write length.
* Read-back verification.

### Clear

* Successful clear.
* Insufficient capacity.
* HAL write failure.
* Verification-read failure.
* Clear verification failure.
* Correct invalidation address.
* Only the required magic bytes being invalidated.
* Loading failing after a successful clear.

---

## 19. Native PlatformIO environment

A new environment will be added:

```ini
[env:native_calibration_storage]
platform = native
test_framework = unity

test_filter = test_calibration_storage
test_build_src = yes

build_src_filter =
    -<*>
    +<calibration_record.cpp>
    +<calibration_storage.cpp>

build_flags =
    -Isrc
```

This environment will compile:

```text
src/calibration_record.cpp
src/calibration_storage.cpp
test/test_calibration_storage/fake_hal_storage.cpp
test/test_calibration_storage/test_main.cpp
```

It will not compile:

```text
src/hal_storage_arduino.cpp
Arduino EEPROM
AVR-specific source files
```

---

## 20. Production backend selection

During this milestone, the Nano production build will use:

```text
hal_storage_arduino.cpp
```

The future direct backend:

```text
hal_storage_avr.c
```

will not be introduced until the next milestone.

The intended progression is:

```text
v0.11
calibration storage -> storage HAL -> Arduino EEPROM backend

v0.12
calibration storage -> storage HAL -> direct AVR EEPROM backend
```

This allows record and storage behaviour to be tested before changing the physical EEPROM implementation.

---

## 21. Non-objectives

This milestone will not:

* Add the direct AVR EEPROM backend.
* Change the public calibration-storage API.
* Change the calibration workflow.
* Change the active calibration factor.
* Add multiple calibration profiles.
* Add dynamic memory allocation.
* Add wear levelling.
* Add dual redundant records.
* Add power-loss recovery between partially completed writes.
* Change the format version.
* Change the CRC algorithm.
* Move the record away from address zero.
* Remove the Arduino Core.
* Modify Serial communication.

---

## 22. Planned commits

### Design

```text
docs: define calibration storage HAL strategy
```

### Storage abstraction

```text
feat: add storage HAL and Arduino backend
```

### Explicit record format

```text
feat: add explicit calibration record codec
```

### Record tests

```text
test: cover calibration record encoding and validation
```

### Storage refactor

```text
refactor: route calibration storage through storage HAL
```

### Storage-operation tests

```text
test: cover calibration storage operations
```

### Final validation

```text
docs: record calibration storage HAL validation
```

---

## 23. Validation commands

The new native suite will run with:

```text
pio test -e native_calibration_storage
```

The complete native regression will remain:

```text
pio test -e native_button
pio test -e native_hx711
pio test -e native_level_indicator
pio test -e native_operation_indicator
pio test -e native_scale
pio test -e native_calibration_storage
```

The production firmware must continue compiling:

```text
pio run -e nanoatmega328new
```

Physical validation must confirm that:

* A previously stored calibration remains readable.
* A new calibration can be saved.
* The new calibration survives restart.
* Clearing the stored calibration still works.
* The default factor is used after clearing.
* Existing calibration behaviour remains unchanged.

---

## 24. Definition of done

This milestone will be complete when:

* [ ] The storage-HAL architecture is documented.
* [ ] A C-compatible storage HAL exists.
* [ ] An Arduino EEPROM backend exists.
* [ ] Calibration storage no longer includes `EEPROM.h`.
* [ ] A fixed 12-byte record format exists.
* [ ] Record byte order is explicit.
* [ ] Record layout is independent from structure padding.
* [ ] Existing magic and version values are preserved.
* [ ] The current CRC algorithm is preserved.
* [ ] Positive calibration factors are supported.
* [ ] Negative calibration factors are supported.
* [ ] Invalid factors are rejected.
* [ ] Record encoding is tested.
* [ ] Record decoding is tested.
* [ ] Corrupted records are rejected.
* [ ] Unsupported versions are rejected.
* [ ] Failed decodes preserve caller outputs.
* [ ] A fake native storage backend exists.
* [ ] Successful loading is tested.
* [ ] Failed loading is tested.
* [ ] Successful saving is tested.
* [ ] Write verification is tested.
* [ ] Failed saving is tested.
* [ ] Successful clearing is tested.
* [ ] Failed clearing is tested.
* [ ] HAL access ranges are verified.
* [ ] All previous 88 native tests still pass.
* [ ] The Nano firmware still compiles.
* [ ] Existing EEPROM calibration remains compatible.
* [ ] Save, restart and clear work on physical hardware.
* [ ] Final validation is documented.
