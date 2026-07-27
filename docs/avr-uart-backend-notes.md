# Direct AVR UART Backend Notes

## 1. Purpose

The purpose of this milestone is to replace the Arduino HardwareSerial backend with a direct ATmega328P USART0 implementation.

The current console architecture is:

```text
app.cpp
    |
    v
console.c
    |
    v
hal_serial.h
    |
    v
hal_serial_arduino.cpp
    |
    v
Arduino HardwareSerial
```

The target architecture is:

```text
app.cpp
    |
    v
console.c
    |
    v
hal_serial.h
    |
    v
hal_serial_avr.c
    |
    v
ATmega328P USART0
```

The following modules will remain unchanged:

```text
src/app.cpp
src/console.c
src/console.h
include/hal_serial.h
```

Only the physical serial backend will be replaced.

---

## 2. Existing serial HAL

The existing public interface is:

```c
void hal_serial_init(
    uint32_t baud_rate
);

bool hal_serial_rx_available(void);

bool hal_serial_read_byte(
    uint8_t *received_byte
);

void hal_serial_write_byte(
    uint8_t transmitted_byte
);
```

The direct AVR backend must preserve these semantics.

The HAL remains byte-oriented and does not understand:

* Strings.
* Numbers.
* CRLF line endings.
* Application commands.
* Calibration messages.
* Console formatting.

---

## 3. New backend

The new backend will be:

```text
src/hal_serial_avr.c
```

The previous Arduino implementation:

```text
src/hal_serial_arduino.cpp
```

will remain in the repository as a reference.

During development, only one backend will be linked into the production firmware at a time.

---

## 4. USART peripheral

The Arduino Nano uses USART0 for the USB-to-serial connection.

The relevant physical pins are:

```text
PD0 / Arduino D0 / RX
PD1 / Arduino D1 / TX
```

The direct backend will use:

```text
UCSR0A
UCSR0B
UCSR0C
UBRR0H
UBRR0L
UDR0
```

The backend will not include:

```text
Arduino.h
HardwareSerial.h
```

and will not call the Arduino Serial object.

---

## 5. Frame format

The existing console uses:

```text
8 data bits
no parity
1 stop bit
asynchronous operation
```

The corresponding configuration is:

```text
UMSEL01:0 = 00
UPM01:0   = 00
USBS0     = 0
UCSZ02:0  = 011
```

The relevant register value is therefore:

```c
UCSR0C =
    _BV(UCSZ01) |
    _BV(UCSZ00);
```

Receiver and transmitter operation will be enabled through:

```text
RXEN0
TXEN0
```

---

## 6. Baud-rate configuration

The production application requests:

```text
115200 baud
```

The ATmega328P runs at:

```text
F_CPU = 16000000 Hz
```

The backend will use asynchronous double-speed mode:

```text
U2X0 = 1
```

The baud-rate equation is:

```text
baud =
F_CPU / (8 × (UBRR0 + 1))
```

For:

```text
F_CPU = 16000000
UBRR0 = 16
```

the actual baud rate is approximately:

```text
117647 baud
```

The error relative to 115200 baud is approximately:

```text
+2.12 %
```

This matches the mode selected by the existing Arduino AVR HardwareSerial implementation for the current configuration.

---

## 7. Baud calculation

The direct backend will calculate a rounded double-speed UBRR value from the baud-rate argument.

Conceptually:

```c
UBRR =
    (
        F_CPU +
        (4UL * baud_rate)
    )
    /
    (
        8UL * baud_rate
    )
    -
    1UL;
```

The current application only requests 115200 baud.

A compile-time check will require:

```text
F_CPU = 16000000UL
```

during the first implementation.

A zero baud-rate argument will disable the USART instead of performing a division by zero.

The calculated value must fit inside the 12-bit UBRR register.

---

## 8. Initialization sequence

The planned initialization sequence is:

```text
Enter project critical section
        |
        v
Disable USART0 interrupts and RX/TX
        |
        v
Reset software receive-buffer indices
        |
        v
Configure U2X0
        |
        v
Load UBRR0H and UBRR0L
        |
        v
Configure asynchronous 8N1
        |
        v
Enable receiver and transmitter
        |
        v
Enable receive-complete interrupt
        |
        v
Restore previous interrupt state
```

Initialization must not enable global interrupts if they were previously disabled.

The project critical-section HAL will preserve the previous interrupt state.

---

## 9. Receive architecture

Reception will use the USART receive-complete interrupt:

```text
USART_RX_vect
```

The ISR will immediately read:

```text
UDR0
```

and place the byte into a project-owned circular buffer.

The architecture is:

```text
USART0 hardware
       |
       v
USART_RX_vect
       |
       v
64-byte ring buffer
       |
       v
hal_serial_rx_available()
hal_serial_read_byte()
       |
       v
console.c
```

This allows incoming command characters to be retained while the main application is printing output or performing other work.

---

## 10. Receive ring buffer

The receive buffer will contain:

```text
64 bytes
```

The buffer state will use:

```c
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;
```

The ISR exclusively modifies the head index.

Normal application code exclusively modifies the tail index.

Because both indices are eight-bit values on an eight-bit AVR, individual reads and writes are atomic.

The buffer size will be a power of two so index wrapping can be implemented efficiently.

---

## 11. Receive-buffer overflow

When the ring buffer is full, the ISR cannot wait for application code.

The selected policy will be:

```text
discard the newly received byte
preserve all bytes already queued
```

This is appropriate for the current human-operated single-character command interface.

No dynamic allocation or blocking operation will occur inside the ISR.

The ISR will remain short and will not:

* Print messages.
* Format data.
* Call application code.
* Perform floating-point operations.
* Wait for another peripheral.
* Disable interrupts manually.

---

## 12. Receive availability

```c
bool hal_serial_rx_available(void);
```

will return true when:

```text
rx_head != rx_tail
```

It will not return the exact number of waiting bytes because the public HAL only requires a boolean result.

The function will not consume input.

---

## 13. Byte reading

```c
bool hal_serial_read_byte(
    uint8_t *received_byte
);
```

must:

* Reject a null output pointer.
* Return false when the ring buffer is empty.
* Consume exactly one queued byte.
* Preserve byte ordering.
* Leave the caller's output unchanged after failure.

The tail index will advance only after a successful read.

---

## 14. Transmission architecture

Transmission will use polling rather than a software transmit buffer.

The sequence for each byte is:

```text
Wait until UDRE0 is set
        |
        v
Write the byte to UDR0
```

The implementation will be:

```c
while ((UCSR0A & _BV(UDRE0)) == 0U)
{
    /* Wait for the data register. */
}

UDR0 = transmitted_byte;
```

Global interrupts remain in their previous state while waiting.

No transmit ISR or software TX ring buffer is required.

---

## 15. Blocking transmission

`hal_serial_write_byte()` may block until the USART transmit data register can accept the next byte.

This is compatible with the current serial HAL contract.

The function does not need to wait for the complete physical transmission of the byte.

It only waits until the byte can be written safely to `UDR0`.

The next console byte will perform the same check.

---

## 16. Why RX is interrupt-driven but TX is polled

Receive bytes arrive independently from application execution.

An interrupt-driven receive buffer prevents ordinary command bytes from being lost while the application is busy.

Transmit bytes are generated synchronously by the application.

A polling transmitter therefore provides a simpler implementation without requiring:

* A TX ring buffer.
* A data-register-empty ISR.
* Additional shared state.
* Additional SRAM.
* Interrupt-disabled deadlock handling.

This design preserves robust input while keeping output simple.

---

## 17. SRAM comparison

Arduino HardwareSerial normally owns receive and transmit software buffers.

The direct backend will own only:

```text
one 64-byte receive buffer
two receive indices
```

No software transmit buffer will be allocated.

The final SRAM and flash differences will be recorded after the backend is selected.

A reduction in static SRAM is possible, but it is not a requirement for the milestone.

---

## 18. Bootloader and upload behaviour

The Nano bootloader uses USART0 before the application starts.

After the bootloader transfers control to the firmware, the project will configure USART0 for its own console.

The physical pins and baud rate remain compatible with the current USB-to-serial connection.

Normal PlatformIO uploads continue resetting the Nano before bootloader communication.

The direct application backend does not replace or modify the bootloader.

---

## 19. Arduino Core interaction

The firmware will still use the Arduino Core temporarily for:

```text
startup
setup()
loop()
delay()
```

However, no production module will reference the global Arduino Serial object after the direct backend is selected.

The Arduino HardwareSerial implementation should therefore not be linked into the final firmware.

The build must be checked for duplicate USART interrupt vectors or unwanted HardwareSerial symbols.

---

## 20. Backend development sequence

During development:

```text
hal_serial_arduino.cpp active
hal_serial_avr.c excluded
```

After the direct backend is complete:

```text
hal_serial_arduino.cpp excluded
hal_serial_avr.c active
```

Both backends define:

```text
hal_serial_init()
hal_serial_rx_available()
hal_serial_read_byte()
hal_serial_write_byte()
```

They must never be linked simultaneously.

---

## 21. Automated validation

The direct AVR backend will first be compiled in isolation with AVR-GCC.

After activation, the production firmware will be compiled using:

```text
pio run -e nanoatmega328new
```

The existing native regression remains:

```text
native_button:                       10
native_hx711:                        18
native_level_indicator:              14
native_operation_indicator:          14
native_scale:                        32
native_calibration_storage:          40
native_console:                      43

Total:                              171
```

The console tests will continue using:

```text
fake_hal_serial.c
```

and will remain independent from the physical USART backend.

---

## 22. Physical validation

After selecting the AVR backend, validation on the Nano must confirm:

* Firmware upload still works.
* Firmware starts normally.
* The startup banner is readable.
* No corrupted characters appear.
* CRLF output remains correct.
* Long messages remain complete.
* Weight values retain two decimals.
* Calibration factors retain six decimals.
* Level names remain correct.
* Commands `t`, `c`, `q`, `s` and `x` work.
* Uppercase and lowercase commands work.
* Commands typed with line endings work.
* Pending extra input can be discarded.
* Complete calibration works.
* Calibration persistence works.
* EEPROM clear works.
* HX711 operation remains functional.
* Timer1 timing remains functional.
* Buttons and indicators remain functional.
* Full power-cycle behaviour remains functional.

---

## 23. Receive stress checks

The following practical checks should also be performed:

* Send one command character at a time.
* Send a command followed by CRLF.
* Paste several command characters quickly.
* Send input while a long message is being printed.
* Send input during calibration instructions.
* Confirm that stale queued input is discarded where expected.
* Confirm that no command is processed repeatedly.

Continuous high-speed serial streams are outside the current application requirements.

---

## 24. Memory validation

The production build before selecting the direct AVR backend will be recorded.

The production build after selection will also be recorded.

The comparison will include:

```text
Static SRAM
Flash usage
```

The expected architectural change is:

```text
remove Arduino HardwareSerial dependency
add direct register backend
add one project-owned RX ring buffer
```

Correctness and isolation of the UART backend are the primary objectives.

---

## 25. Non-objectives

This milestone will not:

* Change the serial HAL interface.
* Change `console.c`.
* Change application commands.
* Change application messages.
* Add line-based command parsing.
* Add binary protocols.
* Add DMA.
* Add hardware flow control.
* Add parity.
* Add a second UART.
* Add a software transmit buffer.
* Add a TX interrupt.
* Add receive error reporting.
* Add baud-rate auto-detection.
* Remove blocking millisecond delays.
* Remove `setup()` or `loop()`.
* Remove the Arduino Core.
* Modify the bootloader.

---

## 26. Planned commits

### Design

```text
docs: define direct AVR UART strategy
```

### Development isolation

```text
build: isolate AVR UART backend during development
```

### UART initialization and transmission

```text
feat: add AVR UART configuration and transmit
```

### Interrupt-driven reception

```text
feat: add AVR UART receive buffer
```

### Production selection

```text
build: select AVR UART backend
```

### Final validation

```text
docs: record direct AVR UART validation
```

---

## 27. Definition of done

This milestone will be complete when:

* [ ] The direct AVR UART architecture is documented.
* [ ] `hal_serial_avr.c` exists.
* [ ] USART0 is configured directly.
* [ ] Double-speed asynchronous mode is configured.
* [ ] 115200 baud works at 16 MHz.
* [ ] `UBRR0` is calculated safely.
* [ ] 8N1 frame format is configured.
* [ ] Receiver and transmitter are enabled.
* [ ] Direct polling transmission works.
* [ ] Receive-complete interrupt is enabled.
* [ ] A 64-byte receive ring buffer exists.
* [ ] Received byte ordering is preserved.
* [ ] Buffer overflow behaviour is defined.
* [ ] Null read pointers are rejected.
* [ ] Failed reads preserve caller output.
* [ ] The Arduino serial backend remains as a reference.
* [ ] The AVR backend is active in the Nano build.
* [ ] Arduino HardwareSerial is no longer in the active console path.
* [ ] All 171 native tests pass.
* [ ] The Nano firmware compiles.
* [ ] Firmware upload still works.
* [ ] Console output remains readable.
* [ ] All command characters work.
* [ ] Calibration remains functional.
* [ ] EEPROM persistence remains functional.
* [ ] HX711, Timer1, buttons and indicators remain functional.
* [ ] Static SRAM and flash usage are recorded.
* [ ] Final physical validation is documented.

## 28. Final architecture

The direct AVR UART backend has been implemented and selected successfully.

The final console path is:

```text
app.cpp
    |
    v
console.c
    |
    v
hal_serial.h
    |
    v
hal_serial_avr.c
    |
    v
ATmega328P USART0
```

The application, console module and public serial HAL were not modified when the physical backend was selected.

The previous backend remains in the repository as a reference:

```text
src/hal_serial_arduino.cpp
```

but it is excluded from the production Nano build.

---

## 29. Active build selection

The production `build_src_filter` excludes:

```text
hal_gpio_arduino.cpp
hal_time_arduino.cpp
hal_storage_arduino.cpp
hal_serial_arduino.cpp
```

The active direct AVR backends are:

```text
hal_gpio_avr.c
hal_critical_avr.c
hal_time_avr.c
hal_storage_avr.c
hal_serial_avr.c
```

Only one serial HAL implementation is linked into the firmware.

---

## 30. USART0 configuration

The direct backend configures USART0 with:

```text
Clock frequency:       16000000 Hz
Requested baud rate:     115200 baud
Operating mode:       asynchronous
Double speed:         enabled
UBRR0 value:          16
Data bits:            8
Parity:               none
Stop bits:            1
Receiver:             enabled
Transmitter:          enabled
RX interrupt:         enabled
```

The resulting physical baud rate is approximately:

```text
117647 baud
```

with an error of approximately:

```text
+2.12 %
```

This configuration is compatible with the USB-to-serial interface used by the Arduino Nano.

---

## 31. Initialization and transmission

`hal_serial_init()`:

* Validates the requested baud rate.
* Rejects zero and unsupported values.
* Calculates a rounded UBRR value.
* Checks all relevant integer operations for overflow.
* Checks that the result fits inside the 12-bit UBRR register.
* Preserves the previous global interrupt state.
* Disables USART0 while reconfiguring it.
* Clears the project receive buffer.
* Configures double-speed asynchronous mode.
* Configures 8N1.
* Enables the receiver and transmitter.
* Enables the receive-complete interrupt.

Transmission uses polling:

```text
wait for UDRE0
write byte to UDR0
```

No transmit interrupt or software transmit buffer is used.

---

## 32. Receive buffer

Reception is interrupt-driven through:

```text
USART_RX_vect
```

The ISR reads every received byte from:

```text
UDR0
```

and places it in a project-owned circular buffer.

The receive buffer contains:

```text
64 bytes
```

The implementation uses eight-bit sequence counters:

```text
rx_head
rx_tail
```

The number of queued bytes is calculated as:

```text
(uint8_t)(rx_head - rx_tail)
```

The physical array position is calculated from the lower six bits of the sequence counter.

This permits all 64 array positions to be used without reserving an empty slot.

---

## 33. Receive concurrency

The receive-complete ISR is the only context that modifies:

```text
rx_head
```

Application code is the only context that modifies:

```text
rx_tail
```

Both variables are eight-bit values on an eight-bit AVR, so individual reads and writes are atomic.

`hal_serial_rx_available()` does not consume data.

`hal_serial_read_byte()`:

* Rejects null pointers.
* Returns false when the buffer is empty.
* Preserves the caller output after failure.
* Consumes exactly one byte after success.
* Preserves byte ordering.

---

## 34. Overflow policy

When 64 bytes are already queued, the receive ISR:

```text
discards the newly received byte
preserves every previously queued byte
```

The ISR does not block, print messages or call application code.

This policy is suitable for the current human-operated single-character console.

Continuous high-speed streams are outside the requirements of this milestone.

---

## 35. Busy-operation input policy

A behavioural inconsistency was identified during physical validation.

Physical button presses are read through polling and are not observed while the application is blocked inside an operation such as scale taring.

UART input is received by interrupt and therefore remained queued while the same blocking operation was running.

This could produce behaviour such as:

```text
start tare
send another 't' during tare
first tare completes
queued 't' starts a second tare
```

It could also allow an additional calibration confirmation command to advance the calibration state immediately after a blocking tare.

The selected application policy is:

```text
commands received while an operation is busy are discarded
```

The application now clears pending console input:

* After an ordinary blocking tare.
* After the tare performed during calibration.
* After blocking calibration sample collection.
* While finite success or error indication patterns are active.

This makes serial commands behave consistently with physical buttons during busy states.

Commands sent after the operation has completed remain functional.

---

## 36. Automated validation

The native regression remains:

```text
native_button:                       10 tests
native_hx711:                        18 tests
native_level_indicator:              14 tests
native_operation_indicator:          14 tests
native_scale:                        32 tests
native_calibration_storage:          40 tests
native_console:                      43 tests

Total:                              171 tests
Failures:                             0
```

The console tests continue using a fake serial HAL and remain independent from the physical AVR backend.

The Nano production firmware compiles successfully with the direct AVR UART backend selected.

---

## 37. Link-time validation

Inspection of the final ELF confirms that it contains:

```text
hal_serial_init
hal_serial_rx_available
hal_serial_read_byte
hal_serial_write_byte
__vector_18
```

The final ELF contains one USART0 receive-complete interrupt implementation.

The project-owned Arduino serial backend is not compiled into the production firmware.

No active application module references the global Arduino `Serial` object.

---

## 38. Physical validation

The direct AVR UART backend was validated successfully on the physical Arduino Nano.

The following behaviour was confirmed:

* [x] Firmware upload through the bootloader works.
* [x] Firmware starts normally.
* [x] Startup output is readable.
* [x] No corrupted characters appear.
* [x] CRLF line endings remain correct.
* [x] Long output messages remain complete.
* [x] Weight values retain two decimal places.
* [x] Calibration factors retain six decimal places.
* [x] Level names remain readable.
* [x] Command `t` works.
* [x] Command `c` works.
* [x] Command `q` works.
* [x] Command `s` works.
* [x] Command `x` works.
* [x] Uppercase commands work.
* [x] Lowercase commands work.
* [x] Commands followed by line endings work.
* [x] Complete calibration remains functional.
* [x] Calibration persistence remains functional.
* [x] EEPROM clearing remains functional.
* [x] HX711 readings remain functional.
* [x] Timer1 timing remains functional.
* [x] Buttons remain functional.
* [x] Indicators remain functional.
* [x] A command received during tare is not executed after tare.
* [x] A new command sent after tare is processed normally.
* [x] Additional calibration confirmations received during tare are discarded.
* [x] Commands received during temporary indication patterns are discarded.
* [x] Rapidly pasted input does not block or corrupt the application.
* [x] Full power-cycle behaviour remains functional.

No application-level regression was detected.

---

## 39. Memory usage

Previous console-abstraction milestone:

```text
RAM:   312 bytes
Flash: 12600 bytes
```

Direct AVR UART milestone:

```text
RAM:   203 bytes
Flash: 11762 bytes
```

The final values were obtained from a clean production build with:

```text
hal_serial_avr.c active
hal_serial_arduino.cpp excluded
```

The direct backend adds a project-owned 64-byte RX buffer and does not allocate a software TX buffer.

---

## 40. Remaining Arduino dependencies

The application no longer uses Arduino GPIO, timekeeping, EEPROM or serial APIs through its active project HAL backends.

The Arduino Core remains temporarily responsible for:

```text
startup
setup()
loop()
delay()
```

`app.cpp` still includes Arduino support because it contains blocking calls to:

```text
delay(1000)
delay(3000)
```

Removing those delays and eventually replacing Arduino startup are separate milestones.

---

## 41. Architectural result

The active architecture is now:

```text
GPIO:
hal_gpio.h
    -> hal_gpio_avr.c

Critical sections:
hal_critical.h
    -> hal_critical_avr.c

Time:
hal_time.h
    -> hal_time_avr.c

Non-volatile storage:
hal_storage.h
    -> hal_storage_avr.c

Serial transport:
hal_serial.h
    -> hal_serial_avr.c

Console formatting:
console.c
    -> text, numbers, CRLF and input
```

The production console path no longer depends on Arduino HardwareSerial.

---

## 42. Definition of done

This milestone is complete because:

* [x] The direct AVR UART architecture is documented.
* [x] `hal_serial_avr.c` exists.
* [x] USART0 is configured directly.
* [x] Double-speed asynchronous mode is configured.
* [x] 115200 baud works at 16 MHz.
* [x] `UBRR0` is calculated safely.
* [x] Integer overflow checks exist.
* [x] 8N1 frame format is configured.
* [x] Receiver and transmitter are enabled.
* [x] Polling transmission works.
* [x] The receive-complete interrupt is enabled.
* [x] A 64-byte receive ring buffer exists.
* [x] All 64 buffer positions can be used.
* [x] Received byte ordering is preserved.
* [x] Buffer overflow behaviour is defined.
* [x] Null read pointers are rejected.
* [x] Failed reads preserve caller output.
* [x] Busy-operation command behaviour is defined.
* [x] Commands received while busy are discarded.
* [x] The Arduino serial backend remains as a reference.
* [x] The AVR backend is active in the Nano build.
* [x] Arduino HardwareSerial is no longer in the active console path.
* [x] Only one USART RX interrupt vector is linked.
* [x] All 171 native tests pass.
* [x] The Nano firmware compiles.
* [x] Firmware upload still works.
* [x] Console output remains readable.
* [x] All command characters work.
* [x] Calibration remains functional.
* [x] EEPROM persistence remains functional.
* [x] HX711, Timer1, buttons and indicators remain functional.
* [x] Static SRAM and flash usage are recorded.
* [x] Final physical validation is documented.
