# Remove Arduino Delay Notes

## 1. Purpose

The purpose of this milestone is to remove the remaining direct
Arduino timing dependency from `app.cpp`.

The application currently includes:

```cpp
#include <Arduino.h>
```

only because it still calls:

```cpp
delay(1000);
delay(3000);
```

The target is to replace those calls with a project-owned
millisecond delay API.

The milestone tag will be:

```text
v0.15-remove-arduino-delay
```

---

## 2. Current architecture

The active millisecond time source is already project-owned:

```text
ATmega328P Timer1
        |
        v
TIMER1_COMPA_vect
        |
        v
elapsed_milliseconds
        |
        v
hal_time_millis()
```

`app.cpp` already uses `hal_time_millis()` indirectly through the
button and indicator modules.

However, its remaining blocking delays still use Arduino Timer0
through `delay()`.

---

## 3. Target architecture

The target delay path is:

```text
app.cpp
    |
    v
hal_time_delay_ms()
    |
    v
hal_time_millis()
    |
    v
hal_time_avr.c
    |
    v
ATmega328P Timer1
```

After the migration, `app.cpp` will no longer include `Arduino.h`.

The Arduino Core will still provide:

```text
startup
setup()
loop()
```

through `main.cpp`.

Removing Arduino startup is outside this milestone.

---

## 4. Public API

The public time HAL will gain:

```c
void hal_time_delay_ms(
    uint32_t milliseconds
);
```

The existing API remains:

```c
void hal_time_init(void);

uint32_t hal_time_millis(void);

void hal_time_delay_us(
    uint16_t microseconds
);
```

No existing caller needs to change except the three direct Arduino
`delay()` calls in `app.cpp`.

---

## 5. Common implementation

The millisecond delay will be implemented in:

```text
src/hal_time_delay.c
```

rather than inside either hardware backend.

The algorithm only requires the public millisecond time source, so
it is independent from the physical timer implementation.

Conceptually:

```c
start_time = hal_time_millis();

while (
    (uint32_t)(
        hal_time_millis() - start_time
    ) < milliseconds
)
{
    /* Busy wait. */
}
```

This allows the same implementation to work with:

```text
hal_time_avr.c
hal_time_arduino.cpp
native fake time backends
```

---

## 6. Why unsigned subtraction is used

The project millisecond counter is a `uint32_t` and eventually wraps
from:

```text
4294967295
```

to:

```text
0
```

Elapsed time must therefore be calculated using unsigned modular
subtraction:

```c
(uint32_t)(current_time - start_time)
```

Example:

```text
start time:   4294967294
current time:          1
elapsed time:          3
```

The delay remains correct across the counter overflow.

---

## 7. Zero-duration behaviour

A request of:

```c
hal_time_delay_ms(0U);
```

will return immediately.

It will not wait for a timer tick.

The implementation may avoid reading the time source entirely for
this case.

---

## 8. Blocking semantics

`hal_time_delay_ms()` will remain a blocking busy-wait.

It does not:

- Put the CPU to sleep.
- Process application state.
- Poll physical buttons.
- Run `app_update()`.
- Make scale operations non-blocking.

The milestone preserves the current application behaviour while
removing the Arduino API dependency.

A later milestone may replace long blocking waits with explicit
application states and deadlines.

---

## 9. Interrupt behaviour

The delay function will not disable global interrupts.

While the main code is waiting:

- Timer1 continues updating the millisecond counter.
- USART reception continues filling its receive buffer.
- Other enabled interrupt handlers continue running.

This is required because the delay itself depends on the Timer1
compare-match interrupt.

---

## 10. Usage restrictions

A non-zero `hal_time_delay_ms()` must not be called:

- Before `hal_time_init()`.
- From an interrupt service routine.
- While global interrupts are disabled.
- From a critical section that prevents Timer1 interrupts.

Doing so would prevent the millisecond counter from advancing and
would make the function wait indefinitely.

All current application calls occur after `hal_time_init()` and
outside critical sections.

---

## 11. Timing resolution

The time source advances once per millisecond.

Therefore the delay has one-millisecond resolution.

The actual wall-clock wait may differ by less than one timer tick
depending on where the call begins relative to the next Timer1
compare match.

This is suitable for the existing one-second and three-second
application waits.

It is not intended for precise microsecond timing.

Precise short delays continue using:

```c
hal_time_delay_us();
```

---

## 12. Remaining application delays

The current direct Arduino calls are:

### Fatal HX711 initialization state

```cpp
while (true)
{
    delay(1000);
}
```

### Fatal calibration-factor state

```cpp
while (true)
{
    delay(1000);
}
```

### Automatic startup tare countdown

```cpp
delay(3000);
```

They will become:

```cpp
while (true)
{
    hal_time_delay_ms(1000UL);
}
```

and:

```cpp
hal_time_delay_ms(3000UL);
```

---

## 13. Fatal-loop behaviour

The two fatal loops intentionally remain infinite.

The one-second delay prevents a tight CPU loop while preserving the
existing behaviour.

The application does not currently attempt automatic recovery from:

- HX711 initialization failure.
- An invalid active calibration factor.

Changing the failure policy is outside this milestone.

---

## 14. Startup input behaviour

The three-second startup delay remains blocking.

USART input may still be received by interrupt during this interval.

The following automatic tare ends by discarding pending console
input, so commands received during the startup wait or tare are not
executed afterwards.

Physical buttons are not polled during the startup delay.

This behaviour remains consistent with the busy-operation input
policy established in `v0.14`.

---

## 15. Arduino Timer0

The Arduino Core configures Timer0 during startup.

After `app.cpp` stops using `delay()`, the application will no longer
depend on Arduino Timer0 for its own timing.

However, Timer0 may still be configured and its interrupt may still
run because the firmware continues using the Arduino framework and
its startup code.

Disabling Timer0 or removing the Arduino Core is outside this
milestone.

---

## 16. Native tests

A new native environment will be added:

```text
native_time_delay
```

It will compile:

```text
src/hal_time_delay.c
test/test_time_delay/fake_hal_time.c
test/test_time_delay/test_main.c
```

The fake time backend will provide controlled values through:

```c
uint32_t hal_time_millis(void);
```

No AVR registers or Arduino components will be required.

---

## 17. Planned test coverage

The native delay tests will cover:

- Zero milliseconds returns immediately.
- A one-millisecond wait.
- An ordinary multi-millisecond wait.
- Correct comparison at the exact deadline.
- A time source that advances by more than one millisecond per read.
- Correct operation across `uint32_t` overflow.

The tests will also verify the number of fake time reads where that
is useful for defining behaviour.

---

## 18. Production validation

After replacing the application calls, validation must confirm:

- `app.cpp` compiles without `Arduino.h`.
- No direct `delay()` call remains in `app.cpp`.
- The Nano firmware compiles.
- Startup still waits approximately three seconds.
- Automatic tare still begins normally.
- Fatal loops compile and retain their intended behaviour.
- Timer1 timing remains operational.
- Console input and output remain operational.
- HX711, buttons, LEDs, calibration and EEPROM remain operational.
- All previous native tests pass.
- The new delay tests pass.

---

## 19. Architectural boundary after the milestone

After this milestone:

```text
app.cpp
```

will be independent from the Arduino framework.

The remaining direct Arduino-facing production entry point will be:

```text
src/main.cpp
```

which still contains:

```cpp
#include <Arduino.h>

void setup(void);
void loop(void);
```

Reference backends that are excluded from the production build may
also continue to include Arduino headers.

---

## 20. Non-objectives

This milestone will not:

- Make startup non-blocking.
- Make taring non-blocking.
- Make HX711 sample collection non-blocking.
- Add a scheduler.
- Add sleep modes.
- Change Timer1 configuration.
- Change `hal_time_millis()`.
- Change `hal_time_delay_us()`.
- Disable Arduino Timer0.
- Replace `setup()` or `loop()`.
- Replace the Arduino startup code.
- Remove the Arduino framework from `platformio.ini`.
- Change application messages.
- Change console commands.
- Change button behaviour.

---

## 21. Planned commits

### Design

```text
docs: define Arduino delay removal strategy
```

### Common delay API

```text
feat: add project millisecond delay
```

### Native validation

```text
test: cover project millisecond delay
```

### Application migration

```text
refactor: replace Arduino delays in application
```

### Final validation

```text
docs: record Arduino delay removal validation
```

---

## 22. Definition of done

This milestone will be complete when:

- [ ] The delay-removal strategy is documented.
- [ ] `hal_time_delay_ms()` is declared publicly.
- [ ] A common platform-independent implementation exists.
- [ ] Zero-duration delay returns immediately.
- [ ] Ordinary millisecond delays work.
- [ ] Counter overflow is handled correctly.
- [ ] The delay does not disable interrupts.
- [ ] Usage restrictions are documented.
- [ ] A native fake time backend exists.
- [ ] Native delay tests pass.
- [ ] Every direct Arduino `delay()` call is removed from `app.cpp`.
- [ ] `app.cpp` no longer includes `Arduino.h`.
- [ ] The startup three-second wait remains functional.
- [ ] Fatal loops retain their existing behaviour.
- [ ] All previous native tests pass.
- [ ] The Nano firmware compiles.
- [ ] Physical behaviour is validated.
- [ ] SRAM and flash usage are recorded.
- [ ] Final documentation is complete.
