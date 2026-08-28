# ADS1232 Driver Notes

## Status

This document records the ADS1232 driver and its compile-time application
integration developed after the HX711 prototype characterization.

The increment adds:

- a project-owned C driver;
- an AVR/Arduino-independent platform boundary;
- a fake platform for host-side protocol tests;
- 20 native Unity tests;
- a small `scale_adc` boundary selected at compile time;
- dedicated direct-AVR and Arduino-reference ADS1232 environments;
- independent ADS1232 calibration and tare records.

HX711 remains the default backend. Selecting an ADS1232 production environment
changes only the converter adapter, pin configuration, console backend name and
active EEPROM slots; `scale` and the application state machine remain shared.

## Physical module under evaluation

The available module contains an ADS1232, a 4.9152 MHz crystal, an onboard
reference source and an onboard 3.3 V rail. The module was checked with a
multimeter before firmware integration.

Measured with the Nano powered from USB-C:

| Point | Measured value |
| --- | ---: |
| Nano 5 V to Nano GND | 4.625 V |
| P2 5V to P2 GND | 4.605 V |
| P3 VCC to P3 AGND | 3.299 V |
| A1P+ to P3 AGND | 1.647 V |
| A1N- to P3 AGND | 1.647 V |
| A1P+ to A1N- | -0.3 mV |

P2 GND and P3 AGND have continuity. The approximately half-supply common-mode
voltage at both channel-1 inputs is consistent with an unloaded bridge excited
from P3 VCC. The small differential reading is plausible for the unloaded
cell and does not indicate a short between the signal inputs.

P1 is currently set to the `VCC` position, so the load-cell excitation and ADC
reference are both approximately 3.3 V. This ratiometric arrangement is the
selected starting point. The 2.5 V position is not used for the present gain-128
test because its approximately 1.26 V bridge midpoint would leave less
common-mode margin.

## Prototype wiring used by the driver

| Nano connection | ADS1232 module | Electrical treatment |
| --- | --- | --- |
| D2 | P2 `DOUT` | Direct 3.3 V output to Nano input; no pull-up |
| D3 | P2 `SCLK` | 1 kΩ series, then 2 kΩ from ADS input to GND |
| D9 | P2 `PDWN` | 1 kΩ series, then 2 kΩ from ADS input to GND |
| A0 used as digital output | P2 `GAIN0` | 1 kΩ series, then 2 kΩ from ADS input to GND |
| A1 used as digital output | P2 `GAIN1` | 1 kΩ series, then 2 kΩ from ADS input to GND |
| GND | P2 `GND` | Common logic ground |
| 5 V | P2 `5V` | Module supply input |

The 1 kΩ / 2 kΩ dividers reduce a measured Nano HIGH of approximately 4.61 V
to approximately 3.07 V at the ADS1232 control input. The two gain controls are
HIGH for gain 128.

The module straps remain fixed during this comparison:

| Module input | Strap | Effect |
| --- | --- | --- |
| P2 `A0` | GND | Channel 1 |
| P2 `SPEED` | GND | 10 samples/s |
| P1 | `VCC` | Approximately 3.3 V reference/excitation |

The load cell is soldered to P3:

| Load-cell function | P3 terminal |
| --- | --- |
| Excitation negative | `AGND` |
| Signal negative | `A1N-` |
| Signal positive | `A1P+` |
| Excitation positive | `VCC` |

Nano `A0` in the first table is a microcontroller GPIO. Module P2 `A0` in the
second table is the ADS1232 channel-select input. They are different signals.

## Driver boundary

The driver owns only signals that are connected to the Nano:

- `DOUT`;
- `SCLK`;
- `PDWN`;
- `GAIN0`;
- `GAIN1`.

Channel selection and sample rate remain physical straps in this prototype.
They are intentionally not represented by fictitious Nano pins.

The initial public interface supports:

- the documented ADS1232 power-up reset sequence;
- ready-state polling and bounded ready waits;
- signed 24-bit reads;
- explicit powered-down status;
- bounded power-down and non-blocking wake-up;
- asynchronous internal offset-calibration start.

Runtime gain changes are intentionally absent from this first interface. Such a
change needs an explicit settling and recalibration contract before it is made
part of the public API. The current prototype uses gain 128 from initialization
onward, so inventing that contract now would add risk without helping the
comparison.

## Protocol decisions

Each ordinary read emits exactly:

```text
24 data clocks + 1 completion clock = 25 SCLK pulses
```

The 24-bit word is shifted most-significant bit first and sign-extended to a
signed 32-bit result. The 25th pulse returns `DOUT` HIGH until the next
conversion is ready. Omitting it could make a zero least-significant bit look
like an immediately ready new conversion.

Internal offset calibration emits:

```text
24 data clocks + completion clock + calibration clock = 26 SCLK pulses
```

The 26th falling edge starts the internal calibration. The driver returns after
that edge instead of blocking for approximately eight conversions. Completion
can be observed cooperatively when `DOUT` becomes LOW again.

The driver uses 1 microsecond HIGH and LOW clock phases, comfortably above the
100 ns minimum. Interrupt state is preserved around the approximately 50-52
microsecond transfer. Wake-up remains non-blocking: the module's crystal can
start while the application polls `DOUT` for the next ready conversion.

The implementation is based on the official Texas Instruments
[ADS1232 data sheet](https://www.ti.com/lit/ds/symlink/ads1232.pdf).

## Native test coverage

The `native_ads1232` suite verifies:

- pin modes, gain encoding and the power-up reset waveform;
- all four gain bit patterns;
- ready polarity, timeout and millisecond wraparound;
- positive and negative signed 24-bit reconstruction;
- exactly 25 clocks for ordinary readings;
- exactly 26 clocks for offset calibration;
- critical-section state restoration;
- no output mutation after a timeout;
- explicit initialized and powered-down state handling;
- power-down, wake-up and idempotence;
- invalid-argument rejection.

The two `test_scale_adc` environments add eight adapter tests per backend. They
verify backend identity, pin and gain forwarding, status mapping, raw-value and
ready forwarding, power operations, backend-specific default factors and the
complete four-record EEPROM layout. The existing 35-test `scale` suite now uses
a fake `scale_adc` implementation, so it verifies the converter-independent
contract rather than HX711-specific calls.

## Application boundary

`src/scale_adc.c` is a compile-time adapter with five operations:

- initialize the selected converter;
- report whether one conversion is ready;
- read one raw signed 24-bit conversion;
- power down;
- power up.

There are no function pointers, dynamic allocation or runtime backend choice.
This keeps the boundary small and gives the linker one statically selected
path. The default environments select HX711. The following environments select
ADS1232 with `SCALE_ADC_BACKEND=2`:

```text
nanoatmega328new_ads1232
nanoatmega328new_arduino_ads1232
```

The ADS1232 adapter initializes channel 1, 10 SPS and gain 128 according to the
documented physical straps and control wiring. Channel and speed remain module
straps rather than fictitious Nano GPIOs.

Internal offset calibration is deliberately not run automatically in this
increment. The low-level driver supports starting it asynchronously, but the
application does not yet have a cooperative state and completion contract for
it. Tare removes the complete installed system's operating zero; the first
physical captures will determine whether explicit internal calibration is also
useful at startup or after recovery.

## Persistent data isolation

Raw counts, tare and calibration factors are converter-specific. The fixed
EEPROM layout therefore assigns separate records:

| Addresses | Owner | Record |
| --- | --- | --- |
| 0–11 | HX711 | calibration |
| 12–23 | HX711 | tare |
| 24–35 | ADS1232 | calibration |
| 36–47 | ADS1232 | tare |

An ADS1232 build cannot load the previous HX711 factor or offset. Its initial
factor is a non-zero arithmetic placeholder of `1.0 counts/g`, not a claimed
calibration. With the ADS1232 tare slot initially absent, startup enters
`TARE_REQUIRED`; the normal calibration workflow must be completed before the
reported weight is meaningful. Startup prints an explicit warning whenever an
ADS1232 build has no valid stored calibration.

## Next increment

After all native suites and both ADS1232 production builds pass, the next
increment is physical validation on the current platform:

1. upload the direct-AVR ADS1232 environment with the documented wiring;
2. verify startup, backend identification and the expected `TARE_REQUIRED` state;
3. complete tare and calibration with the configured reference mass;
4. capture untouched raw ADS1232 data with the LEDs disconnected;
5. repeat controlled zero and known-load checks across restart;
6. reconnect the LEDs only as a separate coupling comparison;
7. decide from those records whether internal offset calibration or filtering is justified.

The LEDs should remain disconnected during the first ADS1232 baseline capture.
They can then be reintroduced as a controlled comparison of power-return
coupling.
