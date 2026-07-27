# Direct AVR Entry Point Notes

## 1. Purpose

The purpose of this milestone is to remove the Arduino framework
from the production firmware entry path.

The current production entry architecture is:

```text
AVR reset
    |
    v
AVR-LibC startup
    |
    v
Arduino Core main()
    |
    v
Arduino init()
    |
    v
setup()
    |
    v
app_init()
    |
    v
loop()
    |
    v
app_update()
```

The target architecture is:

```text
AVR reset
    |
    v
AVR-LibC startup
    |
    v
project main()
    |
    v
enable global interrupts
    |
    v
app_init()
    |
    v
app_update() forever
```

The completed milestone will be tagged:

```text
v0.16-direct-avr-entrypoint
```

---

## 2. Current remaining Arduino dependency

After `v0.15`, `app.cpp` no longer includes Arduino headers or calls
Arduino APIs.

The remaining production-facing Arduino module is:

```text
src/main.cpp
```

It currently contains:

```cpp
#include <Arduino.h>

#include "app.h"

void setup(void)
{
    app_init();
}

void loop(void)
{
    app_update();
}
```

The production environment still declares:

```ini
framework = arduino
```

Therefore the Arduino Core still supplies its own `main()` function
and startup-time peripheral initialization.

---

## 3. Target project entry point

The new project-owned entry point will be:

```text
src/main_avr.cpp
```

Its intended structure is:

```cpp
#include <avr/interrupt.h>

#include "app.h"

int main(void)
{
    sei();

    app_init();

    while (true)
    {
        app_update();
    }
}
```

This remains a C++ translation unit because the existing application
interface is implemented in C++.

The entry point does not require Arduino headers.

---

## 4. Why global interrupts must be enabled explicitly

The direct firmware uses interrupt-driven services:

```text
Timer1 compare-match interrupt:
    advances hal_time_millis()

USART0 receive interrupt:
    stores received console bytes
```

The application initialization performs a blocking startup delay
through:

```c
hal_time_delay_ms(3000UL);
```

That delay depends on the Timer1 interrupt advancing the millisecond
counter.

When the Arduino Core owns `main()`, its initialization enables
global interrupts before `setup()` runs.

Once Arduino `main()` is removed, the project must explicitly enable
global interrupts.

The direct entry point will therefore execute:

```c
sei();
```

before calling:

```c
app_init();
```

Failing to enable interrupts would cause the startup millisecond
delay to wait forever.

---

## 5. Interrupt state during application initialization

`sei()` enables the AVR global interrupt flag.

Individual project interrupt sources are still configured by their
own HAL initialization functions:

```text
hal_time_init():
    configures Timer1 and enables OCIE1A

console_init():
    configures USART0 and enables RXCIE0
```

Before those peripheral interrupt-enable bits are set, enabling the
global interrupt flag alone does not create the project Timer1 or
USART interrupts.

Each HAL remains responsible for clearing pending flags and
configuring its peripheral before enabling its own interrupt source.

---

## 6. AVR-LibC startup responsibilities

Removing the Arduino framework does not mean starting execution
directly from an uninitialized processor state.

The AVR-GCC and AVR-LibC runtime startup code remains linked.

Before calling the project `main()` function, the runtime provides:

- The interrupt vector table.
- Stack-pointer initialization.
- The zero register initialization required by the AVR ABI.
- Copying initialized `.data` values from Flash to SRAM.
- Clearing the `.bss` section.
- Running C++ global constructors when present.
- Calling `main()`.

The project will not use:

```text
-nostartfiles
-nostdlib
```

Creating a completely custom reset routine and runtime startup is
outside this milestone.

---

## 7. Arduino Core responsibilities being removed

The production firmware will no longer use Arduino Core code for:

- The Arduino `main()` function.
- Calling `init()`.
- Calling `initVariant()`.
- Calling `setup()`.
- Repeatedly calling `loop()`.
- Calling `serialEventRun()`.
- Configuring Arduino Timer0 timekeeping.
- Providing `millis()`, `micros()` and `delay()`.
- Providing the global Arduino `Serial` object.
- Providing Arduino GPIO functions.

The project already owns the required replacements.

---

## 8. Timer0 after framework removal

Arduino normally configures Timer0 during `init()` and enables its
overflow interrupt for Arduino timekeeping.

The direct project entry point will not call Arduino `init()`.

Therefore the production application will no longer configure or
use Arduino Timer0 timekeeping.

The project timebase remains:

```text
Timer1 CTC
    |
    v
TIMER1_COMPA_vect
    |
    v
hal_time_millis()
```

Timer0 is not required by the current application.

The final ELF will be checked for the absence of Arduino Timer0
timekeeping symbols.

---

## 9. Bootloader is independent from the framework

The Arduino Nano bootloader remains installed in the boot section of
Flash.

Removing:

```ini
framework = arduino
```

from the application build does not erase or replace the bootloader.

The sequence remains:

```text
reset
    |
    v
bootloader
    |
    +--> accept an upload when requested
    |
    v
jump to the application reset vector
```

PlatformIO will continue using:

```ini
platform = atmelavr
board = nanoatmega328new
```

so the board's MCU, clock, Flash limit and serial upload settings
remain available.

Normal upload through the Nano USB-to-serial interface must be
validated before selecting the new environment as production.

---

## 10. Development isolation

Both entry point implementations will temporarily remain in the
repository:

```text
src/main_arduino.cpp
src/main_avr.cpp
```

The current `src/main.cpp` will first be renamed to:

```text
src/main_arduino.cpp
```

During initial development:

```text
Arduino production environment:
    main_arduino.cpp active
    main_avr.cpp excluded

Bare-metal validation environment:
    main_arduino.cpp excluded
    main_avr.cpp active
```

The two entry points must never be linked simultaneously because both
ultimately define the firmware execution entry model.

---

## 11. Bare-metal validation environment

A temporary environment will be introduced:

```text
nanoatmega328new_baremetal
```

It will use:

```ini
platform = atmelavr
board = nanoatmega328new
monitor_speed = 115200
```

and it will deliberately omit:

```ini
framework = arduino
```

The environment will exclude:

```text
main_arduino.cpp
hal_gpio_arduino.cpp
hal_time_arduino.cpp
hal_storage_arduino.cpp
hal_serial_arduino.cpp
```

It will compile:

```text
main_avr.cpp
app.cpp
console.c
project modules
direct AVR HAL backends
AVR-LibC startup
```

The existing Arduino production environment will remain unchanged
until the bare-metal firmware has been compiled, uploaded and
physically validated.

---

## 12. Final environment selection

After successful validation, the primary environment:

```text
nanoatmega328new
```

will become the bare-metal production environment.

It will no longer declare:

```ini
framework = arduino
```

and it will exclude:

```text
main_arduino.cpp
all Arduino reference HAL backends
```

An optional Arduino reference environment may remain available for
controlled comparisons, but it will not be the default environment
and will not represent production.

The default environment will remain:

```ini
[platformio]
default_envs = nanoatmega328new
```

---

## 13. C and C++ linkage

The direct entry point is implemented in C++ and includes:

```cpp
#include "app.h"
```

Therefore `app_init()` and `app_update()` retain their current C++
linkage.

No C-to-C++ bridge is required in this milestone.

A future refactor may make `app.h` explicitly C-compatible if a pure
C entry point becomes desirable.

That change is not required to remove Arduino.

---

## 14. Application loop semantics

Arduino Core currently performs the conceptual sequence:

```text
setup once
loop repeatedly
```

The direct entry point will preserve the same application behaviour:

```cpp
app_init();

while (true)
{
    app_update();
}
```

The direct loop will not call:

```text
serialEventRun()
yield()
sleep
scheduler functions
```

No such service is required because USART reception is already
interrupt-driven through the project UART backend.

---

## 15. Main must never return

There is no operating system to receive a return value from the
firmware.

The project `main()` will remain inside an infinite loop after
initialization.

No normal execution path will reach AVR-LibC `exit()`.

Fatal application states also remain inside their existing infinite
loops.

---

## 16. C++ runtime constraints

The project uses a restricted embedded C++ subset.

It does not depend on:

- The C++ standard library.
- Dynamic allocation.
- Exceptions.
- RTTI.
- Threads.
- I/O streams.

The bare-metal build must continue using size-oriented compilation
and link-time garbage collection.

The verbose build output will be inspected to confirm the effective
compiler and linker flags.

If needed, explicit flags such as these may be added:

```text
-fno-exceptions
-fno-threadsafe-statics
-ffunction-sections
-fdata-sections
-Wl,--gc-sections
```

No flag will be added merely by assumption; the actual PlatformIO
bare-metal command lines will be checked first.

---

## 17. Build validation

The bare-metal environment must compile successfully with:

```text
pio run -e nanoatmega328new_baremetal
```

The build output must not compile:

```text
framework-arduino-avr
main_arduino.cpp
Arduino core source files
Arduino reference HAL backends
```

The project must still compile all direct AVR modules and C++ modules.

Warnings will be treated as errors when the project-specific build
configuration permits it.

---

## 18. ELF validation

The optimized bare-metal ELF will be inspected with AVR tools.

It must contain a project entry point:

```text
main
```

It must contain the required project interrupt vectors, including:

```text
TIMER1_COMPA_vect implementation
USART_RX_vect implementation
```

It must not contain active Arduino symbols such as:

```text
setup()
loop()
init()
initVariant()
serialEventRun()
timer0_millis
timer0_overflow_count
HardwareSerial
global Serial object
```

Link-time optimization may inline project functions, so validation
will distinguish between object-file definitions and symbols that
remain standalone in the final ELF.

---

## 19. Startup and memory initialization validation

Physical startup validation will indirectly confirm that the runtime
correctly initializes static storage.

The following project state depends on proper `.data` and `.bss`
initialization:

- Global state variables start from their declared values.
- Button structures initialize predictably.
- Calibration state begins idle.
- Timer and UART buffer indices begin at zero.
- Console formatting state is valid.
- Level and operation indicators begin in defined states.

The AVR-LibC runtime remains responsible for these operations before
`main()`.

---

## 20. Upload validation

The bare-metal firmware must be uploaded through the existing Nano
USB serial interface using:

```text
pio run -e nanoatmega328new_baremetal -t upload
```

Validation must confirm:

- PlatformIO detects the upload port.
- The bootloader accepts the firmware.
- The firmware starts after upload.
- Opening the serial monitor resets and starts the firmware normally.
- A full power cycle starts the firmware normally.
- Repeated upload cycles remain reliable.

The bootloader and application are separate pieces of software.

---

## 21. Physical functional validation

The physical Nano must confirm:

- Startup banner is readable.
- The three-second startup delay works.
- Automatic tare starts and completes.
- Timer1 timekeeping works.
- UART transmission works.
- UART interrupt reception works.
- Commands `t`, `c`, `q`, `s` and `x` work.
- Busy-operation input discard remains correct.
- Buttons work.
- LED indicators work.
- HX711 communication works.
- Weight output works.
- Calibration works.
- EEPROM persistence works.
- Power-cycle recovery works.
- No unexpected reset loop occurs.

---

## 22. Native regression

Removing the Arduino framework from the production environment must
not affect native tests.

The expected native regression remains:

```text
native_button:                       10 tests
native_hx711:                        18 tests
native_level_indicator:              14 tests
native_operation_indicator:          14 tests
native_scale:                        32 tests
native_calibration_storage:          40 tests
native_console:                      43 tests
native_time_delay:                    6 tests

Total:                              177 tests
Failures:                             0
```

No new native test is required solely for an infinite entry-point
loop.

The entry point will instead be validated through build inspection,
ELF inspection and physical execution.

---

## 23. Memory comparison

The final documentation will record:

```text
v0.15 Arduino-framework production build:
RAM
Flash

v0.16 direct-entry production build:
RAM
Flash
```

The direct build is expected to remove Arduino Core code and Timer0
timekeeping state.

A reduction is likely, but no specific reduction is required for the
milestone.

Correct startup, interrupt handling and upload behaviour take
priority over memory size.

For a controlled comparison, both entry environments should use the
same application and direct HAL backends.

---

## 24. Failure risks

The main risks are:

### Global interrupts not enabled

Symptom:

```text
startup freezes in hal_time_delay_ms()
```

### Both entry points linked

Symptom:

```text
duplicate entry or duplicate application lifecycle definitions
```

### Arduino framework accidentally retained

Symptom:

```text
Arduino core objects and Timer0 symbols remain in the ELF
```

### Arduino entry point compiled without the framework

Symptom:

```text
undefined setup/loop lifecycle or missing Arduino headers
```

### Bootloader upload configuration lost

Symptom:

```text
firmware builds but serial upload fails
```

### C++ link flags differ

Symptom:

```text
undefined C++ runtime symbols or unexpectedly large firmware
```

The staged environment transition is intended to detect these
problems before production selection.

---

## 25. Non-objectives

This milestone will not:

- Remove AVR-LibC startup.
- Write a custom reset vector.
- Write a custom linker script.
- Use `-nostartfiles`.
- Use `-nostdlib`.
- Remove the Nano bootloader.
- Change fuse bits.
- Upload through ISP.
- Add a watchdog.
- Add sleep modes.
- Add a scheduler.
- Make application operations non-blocking.
- Convert the complete application to C.
- Change hardware pin assignments.
- Change console commands.
- Change calibration behaviour.
- Change EEPROM format.
- Change Timer1 or USART configuration.

---

## 26. Planned commits

### Design

```text
docs: define direct AVR entry point strategy
```

### Entry-point isolation

```text
build: isolate Arduino and AVR entry points
```

### Direct entry point

```text
feat: add direct AVR entry point
```

### Bare-metal validation environment

```text
build: add bare-metal Nano environment
```

### Production selection

```text
build: select direct AVR entry point
```

### Final validation

```text
docs: record direct AVR entry point validation
```

---

## 27. Definition of done

This milestone will be complete when:

- [ ] The direct-entry architecture is documented.
- [ ] The Arduino entry point is isolated as a reference.
- [ ] `main_avr.cpp` exists.
- [ ] The project defines its own `main()`.
- [ ] Global interrupts are enabled explicitly.
- [ ] `app_init()` is called exactly once.
- [ ] `app_update()` is called forever.
- [ ] A bare-metal Nano environment exists.
- [ ] The bare-metal environment omits `framework = arduino`.
- [ ] The Arduino Core is absent from the production link.
- [ ] Arduino Timer0 timekeeping is absent from production.
- [ ] The AVR-LibC runtime startup remains active.
- [ ] Static `.data` and `.bss` initialization remains correct.
- [ ] Required interrupt vectors are linked.
- [ ] Exactly one project `main()` is linked.
- [ ] Firmware upload through the Nano bootloader works.
- [ ] Serial monitoring works.
- [ ] Full power-cycle startup works.
- [ ] Startup delay and automatic tare work.
- [ ] UART input and output work.
- [ ] HX711, buttons, indicators and EEPROM work.
- [ ] All 177 native tests pass.
- [ ] RAM and Flash usage are recorded.
- [ ] The primary production environment is bare-metal.
- [ ] Final validation is documented.
