# Measurement Robustness Investigation

## Status

This document defines the investigation and implementation plan for the first
production milestone after:

```text
v1.3-fault-recovery-and-watchdog
```

The provisional milestone name is:

```text
v1.4-measurement-robustness
```

The work must begin with evidence from the real load cell, HX711 and mechanical
assembly. No filtering or stable-weight algorithm is selected by this document.

---

## 1. Purpose

The existing firmware obtains one new HX711 conversion whenever one is ready,
converts it to grams and immediately updates the level indicator. This is a
valid non-blocking baseline, but it does not yet distinguish between:

- a stable measurement;
- normal ADC noise;
- mechanical settling after a load change;
- creep or slow zero drift;
- a short physical disturbance;
- an isolated implausible sample;
- ADC saturation or a disconnected load-cell connection.

The objective of this milestone is to improve measurement quality without
hiding real mechanical behaviour or weakening the fault and watchdog guarantees
introduced in `v1.3`.

The selected solution should be the simplest one justified by recorded data.

---

## 2. Stable baseline and constraints

Development starts from the stable tag:

```text
v1.3-fault-recovery-and-watchdog
```

and continues on:

```text
feature/measurement-robustness
```

The following existing properties must be preserved:

- The ATmega328P production path does not depend on Arduino Core.
- `app_update()` remains cooperative and non-blocking.
- At most one ready HX711 conversion is consumed per application update.
- Runtime HX711 readiness supervision remains active.
- Recovery attempts remain bounded.
- The watchdog is fed only after a complete main-loop iteration.
- Tare and calibration persistence semantics are not silently changed.
- The normal LED thresholds and hysteresis remain unchanged until measurement
  evidence justifies a separate change.
- No dynamic memory allocation is introduced.
- Native tests remain deterministic and independent of physical hardware.

Measurement diagnostics must not be active automatically after reset and must
not be stored in EEPROM.

---

## 3. Preliminary hardware evidence

A mechanically stable provisional platform was installed before this milestone.
The load-cell wires were soldered to the HX711 and the cable was strain-relieved
while retaining local slack near the cell and converter.

The previous tare and calibration records were cleared because they belonged to
the earlier mechanical installation.

The new successful calibration used a `1500.00 g` reference mass and produced:

```text
Calibration tare offset:  -166841 counts
Net calibration signal:     69910 counts
Calibration factor:     46.606666 counts/g
```

For comparison, the earlier provisional factor was:

```text
45.589332 counts/g
```

The approximately `2.23 %` factor change confirms that calibration belongs to
the complete mechanical measurement chain and should not be reused blindly
after changing the installation.

Four stable loaded plateaus were visible in the initial serial log. The first
was the post-calibration plateau and the following three were repeated mass
placements. After excluding obvious placement and removal transitions, their
approximate statistics were:

| Plateau | Samples | Mean | Standard deviation | Minimum | Maximum |
| --- | ---: | ---: | ---: | ---: | ---: |
| Post-calibration | 73 | 1499.040 g | 0.693 g | 1497.90 g | 1502.15 g |
| Reapplication 1 | 20 | 1500.624 g | 0.806 g | 1499.42 g | 1502.51 g |
| Reapplication 2 | 27 | 1500.764 g | 0.508 g | 1499.79 g | 1501.54 g |
| Reapplication 3 | 26 | 1498.116 g | 0.658 g | 1496.67 g | 1499.42 g |

The observed unloaded plateau means were approximately:

```text
-0.105 g
-1.713 g
-0.530 g
-2.217 g
```

This first log suggests:

- sub-gram short-term noise under stable load;
- approximately `2.65 g` spread between stable loaded means;
- a small return-to-zero displacement after repeated loading;
- real transient readings and overshoot during mass placement and removal;
- no HX711 timeout, recovery, terminal fault or obvious saturation event.

These conclusions are provisional because the existing human-readable output,
printed every `500 ms`, contains only occasional weight snapshots. It does not
expose every raw HX711 conversion or preserve exact sample timing.

---

## 4. Important calibration observation

The preliminary session also confirmed the current calibration transaction
boundary.

After the zero phase is confirmed, the candidate tare offset is saved and made
active before the reference-mass phase completes. Cancelling later preserves the
previous calibration factor but does not restore the previous tare.

This behaviour explains the observed sequence in which a zero confirmation with
the `1500 g` mass still present stored approximately `-96770` counts and produced
approximately `-1500 g` after the mass was removed.

This is not an HX711 measurement fault. The console accurately states that the
active calibration factor was not changed, but the overall user operation is not
fully transactional.

A possible future change could defer the tare commit until both calibration
phases succeed or explicitly restore the previous tare on cancellation. That
change is not part of the initial measurement-capture work and must not be mixed
silently into the filtering investigation.

---

## 5. Questions to answer with recorded data

The investigation should answer the following questions before an algorithm is
selected:

1. What is the real HX711 output rate and sample interval distribution?
2. What are the raw-count mean, median, standard deviation and peak-to-peak range
   under an undisturbed constant load?
3. Does the zero drift monotonically after power-up or after tare?
4. How much creep occurs while a constant load remains applied?
5. How repeatably does the reading return to zero after unloading?
6. How repeatably does the same reference mass return to the same value?
7. How long does the mechanical system take to settle after placement or removal?
8. Do placement transients contain isolated spikes or a continuous physical
   trajectory?
9. How sensitive is the platform to load position and off-axis force?
10. Which raw signatures are produced by safe, controlled connection faults?
11. Can an abnormal measurement be identified without rejecting valid negative
    values?
12. What latency and memory cost would each candidate robustness method add?

---

## 6. Raw diagnostic capture

### 6.1 Principle

The first firmware change after this document should expose the information from
each successful HX711 conversion before any new filter is introduced.

The earlier normal measurement path read a raw value inside `scale`, converted
it to grams and returned only the floating-point weight. The first code commit
of this milestone replaced that result with one coherent measurement object.

The implemented representation is:

```c
typedef struct
{
    int32_t raw_counts;
    int32_t net_counts;
    float weight_grams;
} scale_measurement_t;
```

`scale_try_read_measurement()` fills this object only after one successful
conversion. The diagnostic path does not read the HX711 a second time. Raw
counts, net counts and grams therefore describe one and the same conversion.

### 6.2 Opt-in mode

Raw capture is controlled by a service-console command and disabled after every
reset.

The command is:

```text
d = toggle diagnostic data capture
```

While diagnostic capture is enabled:

- each successful normal-operation conversion produces one machine-readable
  record;
- the ordinary periodic `Weight: ... | Level: ...` line is suppressed to avoid
  mixing formats;
- level calculation and LED operation continue normally;
- HX711 supervision, recovery and watchdog behaviour remain unchanged;
- no setting is written to EEPROM;
- busy-state input policies remain explicit;
- starting tare or calibration stops capture before the operation begins;
- clearing the stored tare stops capture before entering `TARE_REQUIRED`;
- entering fault handling stops capture before printing the fault diagnostic;
- completing, cancelling or recovering from an operation does not restart
  capture automatically.

Capture can start only during normal measurement with a valid tare. Sending `d`
again stops it explicitly. Every new capture session resets the sequence number
to zero. Native application tests cover these start, stop and reset semantics.

### 6.3 Record format

The proposed output is CSV-compatible and begins with a stable record marker:

```text
DATA,sequence,timestamp_ms,raw_counts,tare_offset,net_counts,weight_grams
```

Example:

```text
DATA,1842,237510,-96894,-166841,69947,1500.800000
```

Fields:

| Field | Purpose |
| --- | --- |
| `sequence` | Detect missing, duplicated or reordered application records. |
| `timestamp_ms` | Measure sample intervals, settling time and long-term drift. |
| `raw_counts` | Preserve the unmodified HX711 result for later analysis. |
| `tare_offset` | Record the zero reference active for this conversion. |
| `net_counts` | Avoid ambiguity and permit integer-domain analysis. |
| `weight_grams` | Compare analysis with the value used by the application. |

The header should be printed once when capture starts. Data rows should use no
localized decimal commas so common spreadsheet and scripting tools can parse
them reliably.

The sequence counter wraps naturally as an unsigned 32-bit integer. Every new
capture session restarts it at zero.

### 6.4 Serial bandwidth

The current HX711 configuration is expected to produce approximately `10 SPS`.
At `115200 bit/s`, a compact line for every conversion is comfortably below the
available serial bandwidth.

This assumption must still be verified from recorded timestamps. Diagnostic
printing must not introduce enough blocking time to disturb sampling, recovery
timing or watchdog servicing.

---

## 7. Physical capture plan

All tests should record the exact hardware configuration, calibration factor,
tare offset, mass, placement position, power source and approximate ambient
conditions.

### Test A: warm-up and unloaded drift

1. Start from a cold or well-defined power-up condition.
2. Leave the platform unloaded and mechanically undisturbed.
3. Start diagnostic capture.
4. Record at least 15 minutes without retaring.
5. Note the time at which tare was established, if it occurred after power-up.

Purpose:

- measure short-term noise;
- detect warm-up drift;
- estimate the time required before calibration-quality measurements.

### Test B: constant-load creep

1. Record an unloaded baseline.
2. Place the `1500 g` reference mass at the marked centre position.
3. Do not touch the assembly again.
4. Record at least 15 minutes.
5. Remove the mass and continue recording for at least 5 minutes.

Purpose:

- capture the complete placement transient;
- measure settling time;
- quantify creep under constant load;
- measure return-to-zero behaviour after unloading.

### Test C: repeated loading

Perform at least ten complete cycles using the same mass and marked position:

1. Record unloaded for 30 seconds.
2. Place the mass without deliberately smoothing the transition.
3. Record loaded for 60 seconds.
4. Remove the mass.
5. Record unloaded for 60 seconds.

Purpose:

- quantify repeatability;
- compare placement overshoot;
- measure zero return and hysteresis;
- determine whether a fixed settling rule can work reliably.

### Test D: load position

With a safe mass, record stable windows at:

- centre;
- front;
- rear;
- left;
- right.

Use repeatable marked positions and avoid loads that could twist or overload the
cell.

Purpose:

- quantify sensitivity to off-centre loading;
- determine whether the mechanical platform, rather than software, limits
  accuracy.

### Test E: controlled disturbances

Record brief, repeatable disturbances such as:

- a light touch on the platform;
- a nearby table vibration;
- a cable movement away from the strain-relieved cell connection.

Purpose:

- distinguish isolated disturbances from genuine load changes;
- determine whether an outlier rule would help or incorrectly hide real motion.

### Test F: fault signatures

Fault-signature experiments must be defined separately before execution. They
must avoid shorting excitation or signal connections and must not exceed the load
cell or HX711 electrical limits.

The existing external `10 kOhm` DOUT pull-up method remains the deterministic
test for a missing digital connection. Bridge-wire faults, saturation and
implausible measurement detection require their own safe procedure.

---

## 8. Analysis metrics

For each known stable window, calculate:

- sample count;
- duration and sample intervals;
- arithmetic mean;
- median;
- minimum and maximum;
- peak-to-peak range;
- standard deviation;
- median absolute deviation;
- selected percentiles;
- linear drift slope;
- difference from the expected mass;
- difference from the previous stable plateau.

For every load transition, calculate:

- first detected change time;
- maximum overshoot or undershoot;
- time to enter a provisional tolerance band;
- time continuously maintained inside that band;
- final stable value;
- return-to-zero error after unloading.

The raw dataset must be retained. Analysis must not replace the original samples
with rounded console values.

---

## 9. Candidate methods and selection rules

Methods that may be evaluated include:

- no amplitude filter, used as the baseline;
- a small rolling median;
- a trimmed mean over a fixed window;
- limited outlier rejection based on recent coherent samples;
- stable-weight qualification using range, deviation or successive differences;
- minimum residence time before changing the indicated level;
- explicit settling state after a significant load change.

No method is approved merely by appearing in this list.

The preferred solution should:

- solve a problem visible in the recorded data;
- preserve genuine load changes;
- avoid biasing the stable mean;
- have a deterministic and bounded execution cost;
- use a small fixed amount of SRAM;
- remain non-blocking;
- be testable with synthetic native sequences;
- define startup and reset behaviour;
- define interaction with tare, calibration, recovery and faults;
- degrade safely when samples stop arriving.

A method that only makes serial output look smoother is insufficient.

---

## 10. Negative values and measurement faults

Negative raw counts, negative net counts and negative weights are not faults by
themselves.

Valid negative weights can result from:

- ordinary noise around a stored tare;
- return-to-zero error;
- mechanical unloading relative to the tare condition;
- load-cell orientation;
- calibration-factor sign.

Therefore, the firmware must not clamp every negative weight to zero or discard
every negative sample.

A future implausible-measurement policy should instead use evidence such as:

- known ADC limits or repeated saturation;
- physical minimum and maximum limits for the installation;
- persistence over several conversions;
- impossible rate of change;
- disagreement with surrounding coherent samples;
- HX711 readiness and driver status;
- a fault signature confirmed by controlled hardware tests.

The HX711 digital connection supervision introduced in `v1.3` cannot identify
every possible bridge-wire failure. Measurement plausibility is complementary to
communication readiness, not a replacement for it.

---

## 11. Proposed implementation sequence

The provisional commit sequence is:

1. `docs: define measurement robustness investigation`
2. Refactor the scale read result so one conversion exposes raw counts, net
   counts and grams without changing measurement behaviour.
3. Extend native scale and application fakes/tests for the new result.
4. Add opt-in machine-readable diagnostic capture.
5. Validate serial bandwidth, timing and normal-operation behaviour.
6. Record the physical datasets defined in this document.
7. Add an analysis note with reproducible metrics and conclusions.
8. Select the minimum justified robustness policy.
9. Implement the policy with native tests.
10. Perform physical regression and fault-recovery validation.
11. Finalize release documentation, merge and tag only after validation.

Small refactors may be separated further when that improves reviewability. Data
capture and filtering must not be combined in the same first implementation
commit.

---

## 12. Initial non-goals

The first capture work does not include:

- choosing or implementing a final filter;
- changing level thresholds;
- clamping negative measurements to zero;
- automatically retaring to hide drift;
- changing the HX711 sample-rate hardware configuration;
- making diagnostic mode persistent;
- adding EEPROM configuration fields;
- redesigning the calibration transaction;
- changing the 24 V power architecture;
- supporting a different ADC;
- validating the final production platform or final load cell;
- adding LoRa communication.

These exclusions keep the recorded baseline comparable with stable `v1.3`.

---

## 13. Definition of done

The milestone is complete only when:

- raw datasets from the real installation have been recorded and retained;
- the selected robustness policy is justified by those datasets;
- unfiltered and processed behaviour can be compared;
- native tests cover normal, noisy, transitional and abnormal sequences;
- tare, calibration, level indication, recovery and watchdog behaviour remain
  correct;
- SRAM and flash use remain acceptable for the ATmega328P;
- the physical validation procedure and results are documented;
- limitations and undetectable fault cases are stated explicitly;
- the stable result is integrated and tagged;
- the separate study repository is updated only after the production milestone
  is stable.
