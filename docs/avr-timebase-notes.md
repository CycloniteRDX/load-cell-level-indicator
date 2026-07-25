# AVR Timebase Notes

## 1. Purpose

The purpose of this branch is to replace the Arduino implementation of the project time HAL with a direct ATmega328P implementation.

The current backend is:

```text
src/hal_time_arduino.cpp
```

It currently delegates to:

```text
millis()
delayMicroseconds()
```

The new backend will be:

```text
src/hal_time_avr.c
```

The intended architecture is:

```text
Before this branch:

Application modules
        |
        v
Project time HAL
        |
        v
Arduino millis() and delayMicroseconds()
        |
        v
Arduino Timer0 implementation
```

```text
After this branch:

Application modules
        |
        v
Project time HAL
        |
        +--> Timer1 CTC millisecond counter
        |
        +--> cycle-counted microsecond delay
        |
        v
ATmega328P hardware
```

---

## 2. Main objectives

This branch will:

* Add explicit initialization to the time HAL.
* Configure Timer1 directly through AVR registers.
* Generate one Timer1 compare interrupt every millisecond.
* Maintain a project-owned 32-bit millisecond counter.
* Read the millisecond counter atomically.
* Implement microsecond delays without `delayMicroseconds()`.
* Preserve the existing application timing behaviour.
* Preserve unsigned-overflow-safe time comparisons.
* Keep the Arduino time backend as a reference.
* Keep all 56 native tests passing.
* Validate timing and application behaviour on the physical Nano.

---

## 3. Non-objectives

This branch will not:

* Remove the Arduino framework.
* Replace Arduino startup code.
* Replace `setup()` or `loop()`.
* Replace Serial communication.
* Replace EEPROM access.
* Replace every direct call to Arduino `delay()`.
* Implement a general-purpose scheduler.
* Implement sleep modes.
* Implement low-power timekeeping.
* Implement RTC functionality.
* Implement `micros()`.
* Implement PWM.
* Support arbitrary CPU frequencies initially.
* Add STM32 support.
* Change button debounce periods.
* Change LED blink periods.
* Change HX711 protocol timing requirements.

The direct `delay()` calls that still exist in `app.cpp` will continue using Arduino Timer0 during this transitional milestone.

---

## 4. Existing public interface

The current public interface is declared in:

```text
include/hal_time.h
```

It currently provides:

```c
uint32_t hal_time_millis(void);

void hal_time_delay_us(
    uint16_t microseconds
);
```

This branch will extend the interface with explicit initialization:

```c
void hal_time_init(void);
```

The final interface will be:

```c
void hal_time_init(void);

uint32_t hal_time_millis(void);

void hal_time_delay_us(
    uint16_t microseconds
);
```

---

## 5. Initialization order

Timer1 must be configured before any project module uses:

```text
hal_time_millis()
```

The initialization call will therefore be placed at the beginning of:

```text
app_init()
```

The intended order is:

```c
void app_init(void)
{
    hal_time_init();

    Serial.begin(115200);

    /* Remaining application initialization. */
}
```

The Arduino runtime initializes the microcontroller before calling `setup()`.

The project time initialization will therefore run after the Arduino Core initialization and intentionally replace the Arduino Timer1 configuration.

---

## 6. Timer selection

### Timer0

Timer0 will not be modified in this branch.

Arduino currently uses Timer0 for:

```text
millis()
micros()
delay()
```

Keeping Timer0 unchanged allows the remaining direct Arduino `delay()` calls in `app.cpp` to continue working during the transition.

### Timer1

Timer1 will be reserved for the project time HAL.

Timer1 is a 16-bit timer and will operate in Clear Timer on Compare Match mode.

The project does not currently use:

```text
PWM on D9
PWM on D10
Servo
other Timer1-dependent libraries
```

Once the AVR time backend is active, those Timer1-dependent functions must be considered unavailable.

### Timer2

Timer2 will remain untouched.

It may be useful for future PWM, tone generation or other timing functions.

---

## 7. Timer1 millisecond configuration

The Arduino Nano operates at:

```text
F_CPU = 16 000 000 Hz
```

The Timer1 prescaler will be:

```text
64
```

The timer frequency will therefore be:

```text
16 000 000 / 64 = 250 000 Hz
```

One timer count lasts:

```text
4 microseconds
```

A one-millisecond interval requires:

```text
1 000 us / 4 us = 250 counts
```

Because the counter starts at zero, the compare value will be:

```text
OCR1A = 249
```

The intended register configuration is conceptually:

```c
TCCR1A = 0U;
TCCR1B = 0U;
TCNT1 = 0U;

OCR1A = 249U;

TIFR1 = _BV(OCF1A);

TCCR1B =
    _BV(WGM12) |
    _BV(CS11) |
    _BV(CS10);

TIMSK1 |=
    _BV(OCIE1A);
```

This selects:

```text
Timer1 mode: CTC
TOP:         OCR1A
Prescaler:   64
Interrupt:   TIMER1_COMPA
Period:      1 ms
```

Initialization will be performed inside a critical section.

---

## 8. Millisecond counter

The backend will contain a project-owned counter:

```c
static volatile uint32_t elapsed_milliseconds =
    0U;
```

The Timer1 compare ISR will increment it:

```c
ISR(TIMER1_COMPA_vect)
{
    ++elapsed_milliseconds;
}
```

The counter is `volatile` because it is modified asynchronously by an interrupt.

The ISR must remain short and deterministic.

It will not:

* Call application functions.
* Write to Serial.
* Modify GPIO.
* Perform floating-point calculations.
* Allocate memory.
* Perform blocking operations.

---

## 9. Atomic millisecond reads

The ATmega328P is an 8-bit microcontroller.

Reading a 32-bit value requires multiple machine operations.

An interrupt between those operations could otherwise produce a value containing bytes from two different counter states.

`hal_time_millis()` will therefore use the existing critical-section HAL:

```c
uint32_t hal_time_millis(void)
{
    const hal_critical_state_t previous_state =
        hal_critical_enter();

    const uint32_t current_time =
        elapsed_milliseconds;

    hal_critical_exit(previous_state);

    return current_time;
}
```

The previous interrupt state must be restored exactly.

---

## 10. Counter overflow

The millisecond counter will naturally wrap from:

```text
4 294 967 295
```

to:

```text
0
```

after approximately 49.7 days.

Project timing comparisons will continue using unsigned subtraction:

```c
if ((uint32_t)(now - previous) >= interval)
{
    /* Interval elapsed. */
}
```

The existing button and indicator tests already verify this behaviour at the module level.

The AVR backend will not attempt to prevent or reset the overflow.

---

## 11. Microsecond delay

`hal_time_delay_us()` is currently used by the HX711 platform adapter.

The AVR implementation will use a CPU-cycle-counted busy loop specialized for:

```text
F_CPU = 16 MHz
```

It will not:

* Use Timer0.
* Use Timer1.
* Enable or disable interrupts itself.
* Call Arduino `delayMicroseconds()`.
* Allocate memory.
* Perform floating-point operations.

The implementation must support the short delays required by the HX711 clock protocol.

The first implementation will be intentionally specific to the Arduino Nano clock frequency.

A compile-time guard should reject unsupported frequencies:

```c
#if F_CPU != 16000000UL
#error "hal_time_avr.c currently requires F_CPU = 16 MHz"
#endif
```

Future ports may provide separate calibrated implementations for other CPU frequencies.

---

## 12. Interrupt interaction with the HX711

The HX711 driver protects its 24-bit transfer with:

```text
hal_critical_enter()
hal_critical_exit()
```

While the transfer is protected, the Timer1 compare ISR may be delayed briefly.

After interrupts are restored, the pending compare interrupt will execute.

The HX711 critical section is short enough that losing substantial time is not expected.

Physical validation must confirm that:

* HX711 reads remain functional.
* No repeated timeouts occur.
* Millisecond-based application behaviour remains acceptable.
* LED and button timing remains unchanged to the user.

---

## 13. Temporary coexistence with Arduino time

During this milestone, two timing systems will coexist:

```text
Project HAL:
Timer1 -> hal_time_millis()

Arduino Core:
Timer0 -> millis(), micros() and delay()
```

Project modules already using the HAL will use Timer1.

The remaining direct `delay()` calls in `app.cpp` will continue using Timer0.

This coexistence is temporary and intentional.

A future milestone may remove or replace the remaining Arduino time calls before the Arduino Core itself is removed.

---

## 14. Timer1 ownership consequences

Activating this backend reserves Timer1.

The following features must not be used simultaneously without redesign:

* PWM on Arduino pins D9 and D10.
* Servo implementations using Timer1.
* Libraries that reconfigure Timer1.
* Input capture through Timer1.
* Other Timer1 compare interrupts.

The project currently uses none of these features.

If a future requirement needs Timer1, the timebase architecture must be reconsidered.

Possible alternatives would include:

* Moving the timebase to Timer2.
* Sharing Timer1 compare channels carefully.
* Replacing Timer0 ownership when the Arduino Core is removed.
* Using a scheduler or RTC peripheral on a future microcontroller.

---

## 15. Backend files

Both backends will remain in the repository:

```text
src/hal_time_arduino.cpp
src/hal_time_avr.c
```

Only one backend may be included in the Nano firmware at a time because both implement the same public HAL functions.

During development:

```ini
build_src_filter =
    +<*>
    -<hal_gpio_arduino.cpp>
    -<hal_time_avr.c>
```

After activation:

```ini
build_src_filter =
    +<*>
    -<hal_gpio_arduino.cpp>
    -<hal_time_arduino.cpp>
```

The native test environments already compile only their selected production modules and fake dependencies.

---

## 16. Planned implementation order

### Stage 1: design documentation

Create this document.

Commit:

```text
docs: define AVR timebase strategy
```

### Stage 2: explicit HAL initialization

Add:

```c
void hal_time_init(void);
```

to `hal_time.h`.

Add a no-operation implementation to the Arduino backend.

Call it at the beginning of `app_init()`.

This commit must not change runtime behaviour.

Commit:

```text
feat: add time HAL initialization
```

### Stage 3: isolate the AVR backend

Create the initial:

```text
src/hal_time_avr.c
```

Exclude it from the Nano build while it is incomplete.

Commit:

```text
build: isolate AVR time backend during development
```

### Stage 4: Timer1 millisecond timebase

Implement:

* Timer1 CTC initialization.
* One-millisecond compare interrupt.
* Volatile 32-bit counter.
* Atomic `hal_time_millis()`.

Commit:

```text
feat: add AVR Timer1 millisecond timebase
```

### Stage 5: microsecond delay

Implement the 16 MHz cycle-counted:

```c
hal_time_delay_us()
```

Commit:

```text
feat: add AVR microsecond delay
```

### Stage 6: backend activation

Exclude:

```text
hal_time_arduino.cpp
```

and include:

```text
hal_time_avr.c
```

Compile, upload and validate physically.

Commit:

```text
build: select AVR time backend
```

### Stage 7: final documentation

Record:

* Automated test result.
* Physical validation.
* Timer accuracy observations.
* Flash and SRAM usage.
* Any changed implementation decisions.

Commit:

```text
docs: record AVR timebase validation
```

---

## 17. Automated validation

The normal firmware must compile:

```text
pio run -e nanoatmega328new
```

All existing native tests must continue passing:

```text
pio test -e native_button
pio test -e native_hx711
pio test -e native_level_indicator
pio test -e native_operation_indicator
```

Expected result:

```text
56 native tests passed
Nano firmware build passed
```

The native tests do not execute AVR registers or the Timer1 interrupt.

They validate that the application modules using the time HAL retain their expected behaviour.

---

## 18. Physical validation

After activating the AVR backend, verify:

* [ ] Firmware starts normally.
* [ ] Serial output remains functional.
* [ ] Automatic startup tare still occurs.
* [ ] Weight readings continue updating.
* [ ] HX711 communication does not time out.
* [ ] Button debounce remains correct.
* [ ] Short button presses work.
* [ ] Long button presses work.
* [ ] Very-low LED blinking remains correct.
* [ ] Calibration LED blinking remains correct.
* [ ] Success and error patterns retain their expected timing.
* [ ] Periodic serial weight output remains approximately correct.
* [ ] Calibration storage still loads.
* [ ] Complete calibration flow still works.
* [ ] No unexpected PWM or Timer1 functionality is required.

Where suitable, an oscilloscope or logic analyzer may be used to observe:

* Timer-derived LED periods.
* HX711 PD_SCK high and low times.
* Long-term timebase drift.

---

## 19. Production-code constraints

The following modules should not require behavioural changes:

```text
button.cpp
indicator_leds.cpp
level_indicator.cpp
operation_indicator.cpp
hx711_driver.c
hx711_platform.c
scale.cpp
calibration_storage.cpp
```

Expected production changes are limited primarily to:

```text
include/hal_time.h
src/hal_time_arduino.cpp
src/hal_time_avr.c
src/app.cpp
platformio.ini
documentation
```

Changes to timing constants should not be necessary.

---

## 20. Definition of done

This branch will be complete when:

* [ ] The AVR timebase design is documented.
* [ ] The time HAL has explicit initialization.
* [ ] Timer1 is configured in CTC mode.
* [ ] Timer1 generates a compare interrupt every millisecond.
* [ ] A project-owned volatile millisecond counter exists.
* [ ] The millisecond counter is read atomically.
* [ ] Natural `uint32_t` overflow is preserved.
* [ ] Microsecond delays no longer call Arduino.
* [ ] Unsupported CPU frequencies are rejected safely.
* [ ] Timer1 ownership is documented.
* [ ] The Arduino time backend remains available as a reference.
* [ ] The AVR time backend is selected for the Nano build.
* [ ] All 56 native tests pass.
* [ ] The Nano firmware compiles.
* [ ] Serial communication remains functional.
* [ ] HX711 communication remains functional.
* [ ] Button timing remains correct.
* [ ] Indicator timing remains correct.
* [ ] Calibration behaviour remains correct.
* [ ] Flash and SRAM usage are recorded.
* [ ] Final validation is documented.

---

## 21. References

The design is based on:

* Microchip ATmega328P Timer/Counter documentation.
* Microchip Timer1 CTC mode documentation.
* Arduino AVR Core `wiring.c`.
* The existing project time and critical-section HAL interfaces.

## 22. Final architecture

The direct AVR time backend has been implemented and selected successfully for the Arduino Nano production build.

The active project time architecture is now:

```text
Application modules
        |
        v
Project time HAL
        |
        +--> hal_time_millis()
        |        |
        |        v
        |   Timer1 CTC interrupt
        |        |
        |        v
        |   Project-owned uint32_t counter
        |
        +--> hal_time_delay_us()
                 |
                 v
          CPU-cycle-counted busy loop
                 |
                 v
          ATmega328P at 16 MHz
```

The previous Arduino backend remains available as a reference:

```text
src/hal_time_arduino.cpp
```

but is excluded from the Arduino Nano production build.

The active backend is:

```text
src/hal_time_avr.c
```

---

## 23. Timer1 configuration

Timer1 is owned exclusively by the project time HAL.

The final configuration is:

```text
Timer:            Timer1
Timer width:      16 bits
Operating mode:   CTC
TOP:              OCR1A
CPU frequency:    16 MHz
Prescaler:        64
Timer frequency:  250 kHz
Timer tick:       4 us
OCR1A:            249
Interrupt rate:   1000 Hz
Interrupt period: 1 ms
Interrupt vector: TIMER1_COMPA_vect
```

The compare-match ISR increments a project-owned counter:

```c
static volatile uint32_t elapsed_milliseconds;
```

The ISR remains deliberately short:

```c
ISR(TIMER1_COMPA_vect)
{
    ++elapsed_milliseconds;
}
```

It does not call application code, access Serial, perform floating-point calculations or execute blocking operations.

---

## 24. Time HAL initialization

The public time HAL now provides explicit initialization:

```c
void hal_time_init(void);
```

The application calls it at the beginning of:

```text
app_init()
```

before buttons, indicators or other modules read the project millisecond counter.

The Arduino time backend implements `hal_time_init()` as a no-operation because Arduino initializes Timer0 before `setup()`.

The AVR backend uses `hal_time_init()` to:

* Stop Timer1.
* Clear its previous Arduino configuration.
* Disable Timer1 interrupts temporarily.
* Reset `TCNT1`.
* Reset the software millisecond counter.
* Configure `OCR1A`.
* Clear pending compare flags.
* Enable the Compare Match A interrupt.
* Start Timer1 in CTC mode with a prescaler of 64.

The configuration operation is protected by the project critical-section HAL.

---

## 25. Atomic millisecond reads

The ATmega328P is an 8-bit microcontroller, while the project counter is 32 bits.

Reading the complete value therefore requires several machine instructions.

`hal_time_millis()` protects the read using:

```text
hal_critical_enter()
hal_critical_exit()
```

This prevents the Timer1 ISR from updating the counter between individual byte reads.

The previous interrupt state is restored exactly, so the function also behaves correctly when called from an already protected section.

---

## 26. Counter overflow

The project millisecond counter retains natural unsigned 32-bit overflow behaviour.

It wraps from:

```text
4294967295
```

to:

```text
0
```

after approximately 49.7 days.

Application modules continue using unsigned subtraction:

```c
if ((uint32_t)(now - previous) >= interval)
{
    /* Interval elapsed. */
}
```

No special counter reset or overflow handler is required.

The native button and indicator tests already validate timing logic across this overflow boundary.

---

## 27. Microsecond delay architecture

The AVR backend no longer calls:

```text
delayMicroseconds()
```

The implementation uses:

```c
_delay_loop_2()
```

from AVR libc.

At 16 MHz:

```text
16 CPU cycles per microsecond
4 CPU cycles per loop iteration
4 loop iterations per microsecond
```

Requested delays are converted into loop iterations.

Large `uint16_t` delays are divided into chunks so the internal 16-bit loop counter is never exceeded.

A requested delay of zero is handled explicitly because passing zero directly to `_delay_loop_2()` would represent a complete 16-bit counter wrap rather than zero iterations.

The implementation is intentionally restricted to:

```text
F_CPU = 16000000UL
```

Unsupported CPU frequencies cause a compile-time error rather than silently producing incorrect timing.

The busy loop does not disable interrupts itself.

When exact HX711 pulse timing is required, the HX711 driver already executes the transfer inside its own critical section.

---

## 28. Coexistence with Arduino Timer0

The project currently contains two independent timing systems:

```text
Project time HAL:
Timer1 -> hal_time_millis()

Arduino Core:
Timer0 -> millis(), micros() and delay()
```

This coexistence is intentional during the transition away from Arduino.

Project modules that depend on the HAL now use Timer1.

Remaining direct Arduino `delay()` calls continue using Timer0.

Timer0 was not modified, so Arduino Serial operation, startup behaviour and remaining delay calls continue functioning normally.

---

## 29. Timer1 ownership

Timer1 is now reserved by the project time backend.

The following features must not be introduced without redesigning the time architecture:

* PWM on Arduino pins D9 and D10.
* Servo libraries that depend on Timer1.
* Timer1 input capture.
* Additional Timer1 compare interrupts.
* Libraries that reconfigure Timer1.

The current project does not require any of these resources.

Timer2 remains available for future timing, PWM or signal-generation requirements.

---

## 30. Production backend selection

The Arduino Nano build now excludes both temporary Arduino HAL backends:

```ini
build_src_filter =
    +<*>
    -<hal_gpio_arduino.cpp>
    -<hal_time_arduino.cpp>
```

The active low-level production backends are:

```text
src/hal_gpio_avr.c
src/hal_time_avr.c
src/hal_critical_avr.c
```

The project still uses the Arduino framework for:

* Startup code.
* `setup()` and `loop()`.
* Serial communication.
* EEPROM access.
* Remaining direct `delay()` calls.

---

## 31. Automated validation

The complete native regression passed successfully:

```text
pio test -e native_button
pio test -e native_hx711
pio test -e native_level_indicator
pio test -e native_operation_indicator
```

Final automated result:

```text
Button tests:              10 passed
HX711 driver tests:        18 passed
Level-indicator tests:     14 passed
Operation-indicator tests: 14 passed
Total:                     56 passed
Failures:                  0
```

The Arduino Nano firmware also compiled successfully:

```text
pio run -e nanoatmega328new
```

---

## 32. Physical validation

The direct AVR time backend was tested successfully on the physical Arduino Nano.

The following behaviour was verified:

* [x] Firmware starts normally.
* [x] Serial output remains functional.
* [x] Stored calibration loads correctly.
* [x] Startup tare completes.
* [x] HX711 initialization succeeds.
* [x] HX711 readings continue updating.
* [x] Load-cell response remains functional.
* [x] No repeated HX711 timeouts occur.
* [x] Manual tare works.
* [x] Button debounce remains correct.
* [x] Short calibration-button presses work.
* [x] Long calibration-button presses work.
* [x] Very-low warning blinking works.
* [x] Calibration blinking works.
* [x] Tare indication works.
* [x] Success indication works.
* [x] Error indication works.
* [x] Temporary patterns return to the expected state.
* [x] Periodic application behaviour remains correct.
* [x] Complete calibration flow works.
* [x] Calibration remains available after restarting.

No visible timing regression was detected during physical operation.

---

## 33. Memory usage

Previous direct AVR GPIO milestone:

```text
RAM:   744 bytes
Flash: 12450 bytes
```

Direct AVR GPIO and time backends:

```text
RAM:   748 bytes
Flash: 12620 bytes
```

The final values must be taken from the successful PlatformIO production build.

---

## 34. Architectural result

The project now owns three direct AVR hardware abstraction backends:

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
```

No behavioural changes were required in:

```text
button.cpp
indicator_leds.cpp
level_indicator.cpp
operation_indicator.cpp
hx711_driver.c
hx711_platform.c
scale.cpp
calibration_storage.cpp
```

Only the application initialization sequence required the new explicit call to:

```text
hal_time_init()
```

This confirms that the GPIO, critical-section and time abstraction boundaries are operating correctly.

---

## 35. Definition of done

This milestone is complete because:

* [x] The AVR timebase design is documented.
* [x] The time HAL has explicit initialization.
* [x] Timer1 is configured in CTC mode.
* [x] Timer1 generates a compare interrupt every millisecond.
* [x] A project-owned volatile millisecond counter exists.
* [x] The millisecond counter is read atomically.
* [x] Natural `uint32_t` overflow is preserved.
* [x] Microsecond delays no longer call Arduino.
* [x] Unsupported CPU frequencies are rejected safely.
* [x] Timer1 ownership is documented.
* [x] The Arduino time backend remains available as a reference.
* [x] The AVR time backend is selected for the Nano build.
* [x] All 56 native tests pass.
* [x] The Nano firmware compiles.
* [x] Serial communication remains functional.
* [x] HX711 communication remains functional.
* [x] Button timing remains correct.
* [x] Indicator timing remains correct.
* [x] Calibration behaviour remains correct.
* [x] Flash and SRAM usage are recorded.
* [x] Final validation is documented.
