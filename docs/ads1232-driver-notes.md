# ADS1232 Driver Notes

## Status

This document records the first isolated ADS1232 increment developed after the
HX711 prototype characterization.

The increment adds:

- a project-owned C driver;
- an AVR/Arduino-independent platform boundary;
- a fake platform for host-side protocol tests;
- 20 native Unity tests;
- no change to the active `scale` backend.

The application therefore continues to use the HX711 until a later increment
introduces a small measurement-backend boundary.

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

## Next increment

After this isolated driver passes both native and production builds, the next
increment should add a compile-time measurement-backend boundary in front of
`scale`. That change should:

1. keep HX711 as a selectable backend;
2. add the ADS1232 pin constants without reusing misleading HX711 names;
3. map driver results into the existing scale result model;
4. preserve one-conversion-per-update behaviour;
5. avoid reusing HX711 calibration or tare records for ADS1232 data;
6. expose raw ADS1232 records before adding filtering.

The LEDs should remain disconnected during the first ADS1232 baseline capture.
They can then be reintroduced as a controlled comparison of power-return
coupling.
