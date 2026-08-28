# HX711 Prototype Hardware Characterization

## Status

This note records the physical evidence collected from the provisional HX711
installation during the measurement-robustness investigation.

The investigation is intentionally preserved even though the next experiment
uses an available ADS1232 module. The observations remain useful for:

- separating converter behaviour from wiring and power-distribution effects;
- defining an A/B comparison between the HX711 and ADS1232;
- establishing requirements for the future custom PCB;
- avoiding the same coupling mechanisms in the 24 V installation.

This work characterizes one prototype assembly. It does **not** establish the
intrinsic performance limit of the HX711 device.

Raw captures are retained in [`../logs/`](../logs/). Their inventory and known
test context are recorded in [`../logs/README.md`](../logs/README.md).

---

## 1. Provisional hardware

The characterized setup used:

- an ATmega328P Nano powered and monitored through the same USB connection;
- a generic HX711 module;
- a single-point load cell on a mechanically stable provisional platform;
- load-cell conductors soldered to the HX711;
- cable strain relief with local slack near the cell and converter;
- two buttons and three active-high indicator LEDs on a breadboard;
- an external `10 kOhm` pull-up from HX711 `DOUT` to the shared logic rail;
- a `1500 g` calibration mass;
- calibration factor `46.606666 counts/g` in the raw captures;
- stored tare offset `-166704 counts` in the raw captures.

The final test group changed the power-return wiring so the HX711 was supplied
directly from the Nano and the LEDs and buttons used the other ground
connection. The breadboard connections were not soldered.

---

## 2. Capture integrity and observed cadence

Eight logs contain machine-readable `DATA` records. Across those captures:

- `19,912` coherent measurement rows were retained;
- every sequence began at zero and increased without a gap or duplicate;
- every timestamp increased monotonically;
- the median interval was `84 ms` in every capture;
- observed intervals were normally `84 ms` or `85 ms`;
- no HX711 timeout, recovery or terminal fault occurred.

The measured cadence is therefore approximately:

```text
1000 ms / 84 ms = 11.9 samples/s
```

This differs from the nominal `10 SPS` usually associated with the slow HX711
mode. It is the cadence of this real module and firmware timing path, so future
comparisons should use measured timestamps rather than assuming a rate.

---

## 3. LED-correlated ground coupling

The most important hardware result was a strong periodic component while the
VERY_LOW indicator LED shared the original breadboard supply-return path with
the HX711.

In `device-monitor-260825-171711.log`:

- the strongest spectral component was approximately `1.985 Hz`;
- its sinusoidal amplitude was approximately `13.16 g`;
- the VERY_LOW LED completes one on/off cycle every `500 ms`, or `2 Hz`.

The frequency match, together with the later wiring experiment, identifies the
LED current return as a dominant coupled disturbance in that configuration.

After feeding the HX711 directly from the Nano and separating the LED/button
return path, the component near `2 Hz` fell to approximately:

| Capture | Approximate 2 Hz amplitude |
| --- | ---: |
| `260827-164621` | `0.21 g` |
| `260827-170408` | `0.15 g` |
| `260827-171637` | `0.10 g` |
| `260827-174251` | `0.07 g` |
| `260827-175623` | `0.10 g` |

Relative to the original `13.16 g` component, the periodic coupling was reduced
by more than `98 %`.

This does not mean that AGND and DGND must be isolated. It means that LED load
current must not share an uncontrolled, resistive breadboard return path with
the low-level bridge and converter current.

---

## 4. Remaining broadband dispersion

Removing the dominant LED-correlated component did not eliminate the remaining
sample-to-sample variation:

| Capture | Samples | Duration | Mean | Standard deviation | Peak-to-peak |
| --- | ---: | ---: | ---: | ---: | ---: |
| `260827-164621` | 1464 | 123.1 s | 22.185 g | 3.486 g | 23.709 g |
| `260827-170408` | 1557 | 130.9 s | 19.567 g | 3.775 g | 26.885 g |
| `260827-171637` | 1719 | 144.5 s | 18.132 g | 3.880 g | 31.819 g |
| `260827-174251` | 1678 | 141.1 s | 20.200 g | 4.701 g | 32.806 g |
| `260827-175623` | 1392 | 117.0 s | 21.065 g | 4.782 g | 31.970 g |

These are whole-capture statistics from undisturbed short runs. They describe
the prototype output, not a filtered operational weight.

Possible contributors left unresolved include:

- breadboard contact resistance and intermittent connections;
- USB and Nano rail noise;
- the generic HX711 module layout and regulator;
- residual ground impedance;
- environmental or mechanical vibration;
- load-cell creep and temperature behaviour.

The test series does not justify assigning the residual noise to any one of
those causes.

---

## 5. DOUT pull-up result

The external `10 kOhm` DOUT pull-up was accidentally left disconnected during
one wiring change and was restored before a follow-up capture.

Restoring it did not reduce the analog measurement dispersion:

```text
Before restoration: 3.486 g standard deviation
After restoration:  3.775 g standard deviation
```

That result is expected. The pull-up is a digital fail-safe that makes a broken
or disconnected DOUT conductor produce a deterministic timeout. It is not an
analog noise filter and should not be judged by weight dispersion.

---

## 6. Breadboard decoupling experiment

Local ceramic and electrolytic capacitors were tested on the breadboard supply
near the HX711. The final capture still used unsoldered breadboard connections.

The two final standard deviations were:

```text
4.701 g
4.782 g
```

No improvement was demonstrated. This result does not prove that local
decoupling is ineffective. A capacitor cannot correct an uncontrolled return
path or a poor breadboard contact, and its own connection inductance and
resistance matter.

The project deliberately stopped short of soldering this entire provisional
assembly because the final hardware will use a purpose-designed ADS1232 PCB.

---

## 7. Zero displacement and warm-up observations

The stored tare survived resets and power removal, so later zero readings expose
changes in the physical/electrical chain rather than a lost software offset.

For the capture started after approximately `16-17 minutes` without power at an
ambient temperature of approximately `18 degC`:

```text
First 30 s mean: -10.280 g
Last 30 s mean:  -14.585 g
Change:           -4.305 g over approximately 12.5 minutes
```

After operating overnight, the next short capture began around `+5 g`:

```text
Whole-capture mean: 4.873 g
Standard deviation: 1.649 g
First-to-last 30 s change: -0.406 g
```

These observations demonstrate that persistent tare is not automatic
zero-tracking. A stored electrical count remains reproducible in EEPROM while
the mechanical zero, bridge output and electronics can still move.

The available tests do not separate:

- load-cell creep;
- temperature sensitivity;
- mounting stress relaxation;
- excitation/reference change;
- HX711 offset drift.

Automatic retare must not be introduced merely to hide this behaviour.

---

## 8. USB observation

A faint PC coil-whine sound was noticed in synchronism with serial activity.
The same USB cable and USB-powered Nano were used in the compared sessions.

This is recorded as an observation, not as proof that serial transmission caused
the measured ADC noise. It does reinforce the requirement to characterize power
rails and avoid depending on an uncontrolled PC USB supply in the final design.

---

## 9. Engineering decision

For a future `25 kg` container, an error or short-term variation of `5-10 g`
corresponds to approximately `0.02-0.04 %` of full scale. The provisional HX711
assembly is therefore sufficient to continue firmware development, but it is
not an appropriate reference layout for the final hardware.

The selected decision is:

1. Preserve the HX711 raw captures and conclusions.
2. Do not spend more prototype time soldering the complete breadboard assembly.
3. Do not select a software filter solely to conceal hardware coupling.
4. Evaluate the available ADS1232 module with the same cell and mechanics.
5. Keep the HX711 driver as a supported reference backend.
6. Compare both converters before resuming the robustness-policy selection.

This decision pauses, but does not discard, the `v1.4` robustness investigation.

---

## 10. Requirements carried into the ADS1232 and custom-PCB work

The next hardware must provide:

- a defined analog and digital current-return strategy;
- no LED or output-load current in the sensitive analog return path;
- local decoupling with short connections;
- a clean ratiometric bridge excitation/reference path;
- differential routing for bridge inputs;
- a continuous ground plane where practical;
- test points for AVDD, DVDD, reference, excitation and both bridge signals;
- explicit logic-level compatibility with the 5 V Nano prototype;
- separate calibration records or an ADC-backend identifier;
- identical raw-data capture semantics for the A/B comparison.

The first ADS1232 experiment uses the module's measured `3.299 V` ratiometric
excitation/reference setting. With the connected bridge, both signal inputs
measured `1.647 V` relative to AGND and the unloaded differential signal was
approximately `-0.3 mV`, leaving ample PGA headroom.
