# Native Scale Test Notes

## 1. Purpose

The purpose of this branch is to add native host-side unit tests for the project scale module:

```text
src/scale.cpp
src/scale.h
```

The tests will run on the development computer using:

```text
PlatformIO Native
GCC / G++
Unity
```

The real HX711 driver will be replaced by a fake implementation of its public API.

The intended architecture is:

```text
scale.cpp
    |
    v
hx711_driver.h
    |
    v
Fake HX711 driver
    |
    v
Controlled readings, readiness and error statuses
```

These tests will validate the scale logic without requiring:

* An Arduino Nano.
* A physical HX711.
* A load cell.
* GPIO simulation.
* Timer simulation.
* Real ADC conversions.

---

## 2. Current scale responsibilities

The scale module currently owns the following responsibilities:

* Initialize the HX711 device.
* Wait for the HX711 to become ready during startup.
* Maintain the tare offset.
* Maintain the runtime calibration factor.
* Reject invalid calibration factors.
* Average one or more raw HX711 readings.
* Subtract the tare offset.
* Convert raw counts into grams.
* Expose net raw counts for calibration.
* Preserve previous state when operations fail.

The module contains these private state variables:

```c
static hx711_t hx711_device;

static int32_t tare_offset = 0;

static float current_calibration_factor = 1.0F;
```

The tests must account for this persistent module state.

---

## 3. Public interface under test

The public scale interface is:

```c
bool scale_init(void);

bool scale_set_calibration_factor(
    float calibration_factor
);

void scale_tare(void);

bool scale_read_weight(
    float *weight_grams
);

bool scale_read_net_counts(
    float *net_counts,
    uint8_t samples
);

long scale_get_offset(void);

float scale_get_calibration_factor(void);
```

No public scale API changes are planned for this branch.

---

## 4. Test boundary

The tests will compile the real:

```text
src/scale.cpp
```

but will not compile:

```text
src/hx711_driver.c
src/hx711_platform.c
```

Instead, the test suite will implement the functions declared by:

```text
include/hx711_driver.h
```

The fake driver will provide:

```c
hx711_init()
hx711_is_ready()
hx711_wait_ready()
hx711_read_raw()
hx711_set_gain()
hx711_power_down()
hx711_power_up()
```

Only the functions currently called by `scale.cpp` require behavioural simulation:

```text
hx711_init()
hx711_wait_ready()
hx711_is_ready()
hx711_read_raw()
```

The remaining functions may return safe default results so the fake satisfies the complete public driver interface.

---

## 5. Why the HX711 API is faked

The existing HX711 driver already has its own native unit-test suite.

Those tests validate:

* HX711 initialization.
* Readiness.
* Timeout handling.
* 24-bit reconstruction.
* Sign extension.
* Gain pulses.
* Critical sections.
* Power-down and power-up.

The scale tests should not repeat the HX711 protocol tests.

Their purpose is to answer different questions:

```text
Given a sequence of raw readings,
does scale.cpp calculate the correct result?

Given an HX711 error,
does scale.cpp preserve its previous state?

Given a calibration factor,
does scale.cpp convert counts correctly?
```

This separation keeps each suite focused on one abstraction layer.

---

## 6. Fake HX711 capabilities

The fake HX711 driver should support:

* Selecting the result returned by `hx711_init()`.
* Selecting the result returned by `hx711_wait_ready()`.
* Selecting whether `hx711_is_ready()` returns true or false.
* Loading a sequence of raw readings.
* Loading a sequence of read statuses.
* Returning an error at a selected sample.
* Recording the pins passed to `hx711_init()`.
* Recording the timeout passed to `hx711_wait_ready()`.
* Recording how many times each driver function was called.
* Recording how many raw readings were consumed.
* Detecting attempts to read beyond the prepared sequence.
* Resetting all fake state before each test.

The fake must not contain real waits or hardware access.

---

## 7. Scale initialization tests

The initialization tests should cover:

### Successful initialization

Verify that:

* `scale_init()` returns true.
* `hx711_init()` is called once.
* The configured DOUT pin is used.
* The configured PD_SCK pin is used.
* `hx711_wait_ready()` is called once.
* The startup timeout is 2000 ms.
* The tare offset is reset to zero.
* The calibration factor is reset to `1.0F`.

### HX711 initialization failure

Verify that:

* `scale_init()` returns false.
* `hx711_wait_ready()` is not called.
* Existing tare and calibration state are not reset.

### HX711 readiness timeout

Verify that:

* `scale_init()` returns false.
* The initialization function was called.
* The readiness function was called.
* Existing tare and calibration state are not reset.

The current implementation only resets scale state after both HX711 operations succeed.

---

## 8. Calibration-factor tests

The tests should verify that valid positive and negative factors are accepted.

Examples:

```text
45.5
-45.5
1.0
-1.0
```

Negative factors are valid because the sign depends on the load-cell wiring direction.

The following values must be rejected:

* Positive zero.
* Negative zero.
* NaN.
* Positive infinity.
* Negative infinity.
* Positive values whose magnitude is below `0.000001F`.
* Negative values whose magnitude is below `0.000001F`.

A rejected factor must not replace the previously valid factor.

The implementation uses:

```c
fabsf(calibration_factor) < 0.000001F
```

Therefore, a factor exactly at the boundary is intended to be accepted, while values strictly below it are rejected.

---

## 9. Tare tests

`scale_tare()` uses:

```text
TARE_SAMPLES = 20
```

The tests should cover:

### Successful tare

Load twenty controlled raw values and verify that:

* All twenty readings are consumed.
* Their integer average becomes the new tare offset.
* Positive and negative raw values are handled.
* Integer division behaviour is respected.

### Failed tare

Introduce an HX711 error during the sample sequence and verify that:

* `scale_tare()` returns no status because its API is `void`.
* Sampling stops at the first failed reading.
* The previous tare offset remains unchanged.
* Later prepared readings are not consumed.

### Repeated tare

Verify that a second successful tare replaces the previous offset.

---

## 10. Net-count tests

`scale_read_net_counts()` should be tested with:

* One sample.
* Multiple samples.
* Positive readings.
* Negative readings.
* A positive tare offset.
* A negative tare offset.
* Fractional averages caused by integer truncation.
* Null output pointer.
* Zero samples.
* Failure on the first reading.
* Failure in the middle of a sequence.
* Failure on the final reading.

The result should be:

```text
integer average of raw readings
minus
current tare offset
```

The output value must not be modified when the operation fails.

---

## 11. Weight-reading tests

`scale_read_weight()` should be tested with:

### HX711 not ready

Verify that:

* The function returns false.
* No raw reading is requested.
* The output value remains unchanged.

### Successful positive conversion

Example:

```text
Tare offset:        -170000 counts
Average raw value:  -100000 counts
Net counts:           70000 counts
Calibration factor:      46.5 counts/g
Expected weight:       1505.376... g
```

### Successful negative-factor conversion

Verify that changing the calibration-factor sign changes the resulting weight sign as expected.

### Negative net counts

Verify that readings below the tare offset produce a negative weight.

### Null output pointer

Verify that:

* The function returns false.
* `hx711_is_ready()` is not called.
* No raw reading is consumed.

### Read error after readiness

Verify that:

* The function first checks readiness.
* It returns false if `hx711_read_raw()` fails.
* The output value remains unchanged.

The current configuration uses:

```text
WEIGHT_SAMPLES = 1
```

so each successful weight measurement currently consumes one raw HX711 reading.

---

## 12. Getter tests

The tests should verify:

```c
scale_get_offset()
```

after:

* Successful initialization.
* Successful tare.
* Failed tare.
* Repeated tare.

The tests should verify:

```c
scale_get_calibration_factor()
```

after:

* Successful initialization.
* Valid positive factor.
* Valid negative factor.
* Rejected factor.
* Failed reinitialization.
* Successful reinitialization.

---

## 13. State-preservation tests

State preservation is an important part of this suite.

The tests should explicitly prove that:

* A failed `scale_init()` does not reset the previous tare offset.
* A failed `scale_init()` does not reset the previous calibration factor.
* A failed tare does not replace the previous tare offset.
* A rejected calibration factor does not replace the previous valid factor.
* A failed weight read does not modify the caller's output variable.
* A failed net-count read does not modify the caller's output variable.

This avoids silently corrupting a previously usable scale state after a transient HX711 failure.

---

## 14. Arithmetic range analysis

HX711 raw values are signed 24-bit values:

```text
Minimum: -8388608
Maximum:  8388607
```

The sample-count parameter is an unsigned 8-bit value:

```text
Maximum samples: 255
```

The largest positive sum is:

```text
8388607 * 255 = 2139094785
```

The largest negative-magnitude sum is:

```text
-8388608 * 255 = -2139095040
```

Both values fit inside a signed 32-bit integer:

```text
INT32_MIN = -2147483648
INT32_MAX =  2147483647
```

Therefore, the current `int32_t raw_sum` is safe for every valid HX711 reading and every possible `uint8_t` sample count.

The tests do not need to change the production accumulator type.

Boundary tests may nevertheless verify averaging near the positive and negative 24-bit limits.

---

## 15. Test isolation

Each test should:

* Reset the fake HX711 state.
* Establish a known scale state.
* Prepare all expected raw readings explicitly.
* Prepare all expected statuses explicitly.
* Initialize output variables with sentinel values.
* Verify both return values and side effects.
* Verify fake-driver call counts.
* Avoid relying on test execution order.

Because `scale.cpp` stores private static state, most tests should begin with a successful:

```c
scale_init();
```

to reset the offset and factor.

Tests specifically checking failed initialization must first establish a deliberate previous state before causing the failure.

---

## 16. Native PlatformIO environment

A new environment will be added:

```ini
[env:native_scale]
platform = native
test_framework = unity

test_filter = test_scale
test_build_src = yes

build_src_filter =
    -<*>
    +<scale.cpp>

build_flags =
    -Isrc
```

This environment will compile:

```text
src/scale.cpp
test/test_scale/fake_hx711_driver.c
test/test_scale/test_main.cpp
```

It will not compile the real HX711 driver or any AVR-specific source files.

---

## 17. Planned file structure

The initial structure will be:

```text
test/
└── test_scale/
    ├── fake_hx711_driver.h
    ├── fake_hx711_driver.c
    └── test_main.cpp
```

The test entry point will be written in C++ because `scale.cpp` and `scale.h` currently use C++ linkage.

The fake HX711 implementation may be written in C because `hx711_driver.h` exposes a C-compatible API.

---

## 18. Non-objectives

This branch will not:

* Test GPIO registers.
* Test Timer1.
* Test the HX711 serial protocol.
* Test HX711 pulse timing.
* Test electrical noise.
* Test real load-cell accuracy.
* Test mechanical installation.
* Test calibration storage.
* Change the scale public API.
* Add dynamic memory allocation.
* Replace floating-point arithmetic.
* Add filtering algorithms.
* Change sample counts.
* Change the calibration workflow.
* Modify application behaviour.
* Add continuous integration.

Physical validation remains necessary for the complete measurement chain.

---

## 19. Planned commits

### Design documentation

```text
docs: define native scale test strategy
```

### Native environment and fake driver

```text
test: add native scale test foundation
```

### Initialization and calibration-factor coverage

```text
test: cover scale initialization and calibration factors
```

### Tare and raw-count coverage

```text
test: cover scale tare and net counts
```

### Weight conversion and failure coverage

```text
test: cover scale weight conversion and errors
```

### Final documentation

```text
docs: record native scale test coverage
```

---

## 20. Validation commands

The new suite will run with:

```text
pio test -e native_scale
```

The existing regression must continue passing:

```text
pio test -e native_button
pio test -e native_hx711
pio test -e native_level_indicator
pio test -e native_operation_indicator
```

The production firmware must continue compiling:

```text
pio run -e nanoatmega328new
```

---

## 21. Definition of done

This branch will be complete when:

* [ ] The scale-test strategy is documented.
* [ ] A native scale environment exists.
* [ ] A controllable fake HX711 driver exists.
* [ ] HX711 initialization success is tested.
* [ ] HX711 initialization failure is tested.
* [ ] HX711 startup timeout is tested.
* [ ] Successful initialization resets scale state.
* [ ] Failed initialization preserves scale state.
* [ ] Positive calibration factors are tested.
* [ ] Negative calibration factors are tested.
* [ ] Zero, NaN and infinity are rejected.
* [ ] Very small calibration factors are rejected.
* [ ] Rejected factors preserve the previous factor.
* [ ] Successful tare is tested.
* [ ] Failed tare preserves the previous offset.
* [ ] Repeated tare is tested.
* [ ] Single-sample net counts are tested.
* [ ] Multi-sample net counts are tested.
* [ ] Positive and negative raw values are tested.
* [ ] Invalid net-count arguments are tested.
* [ ] Net-count read failures preserve the output value.
* [ ] HX711-not-ready weight behaviour is tested.
* [ ] Positive and negative weight conversion is tested.
* [ ] Weight-read errors preserve the output value.
* [ ] Getter behaviour is tested.
* [ ] Arithmetic boundary cases are tested.
* [ ] All native scale tests pass.
* [ ] The previous 56 native tests still pass.
* [ ] The Arduino Nano firmware still compiles.
* [ ] Final coverage is documented.
