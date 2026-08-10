# Fault recovery and watchdog design

## Document status

This document defines the proposed design for:

```text
v1.3-fault-recovery-and-watchdog
```

The baseline is:

```text
v1.2-non-blocking-application
a89ea34 — merge: integrate non-blocking application
1b91d9f — docs: correct v1.2 validation record
```

The post-release documentation correction does not change the firmware used as
the technical baseline.

This document is a design contract. It does not claim that recovery or the
watchdog have already been implemented or physically validated.

---

## 1. Motivation

`v1.2` guarantees that startup, tare, calibration and temporary result patterns
return control to the superloop. That removes multi-second application-level
blocking, but it does not yet answer four different questions:

1. Which fault occurred?
2. Is it safe to continue?
3. Can the firmware recover without redefining tare or calibration?
4. What resets the microcontroller if the software itself stops returning?

The current application has one generic latched state:

```text
APP_STATE_FAULT
```

That state is cooperative and safe, but it loses the cause and always requires
a reset. `v1.3` will separate fault detection, classification, recovery and
watchdog supervision.

---

## 2. Important pending HX711 validation

The driver already implements and natively tests:

```c
hx711_power_down()
hx711_power_up()
```

There is no confirmed production bug in those two functions at the start of
this milestone. The real pending item is narrower and must not be overstated:

```text
Repeated power-down and power-up have not received a dedicated physical test
on the real Nano and HX711.
```

This was deliberately kept outside `v0.5`, and `v1.2` continued to list it as
pending. Because sensor recovery may use the same path, `v1.3` must include a
physical power-cycle gate before declaring automatic recovery complete.

Native fake tests prove call order and logical state. They cannot prove:

- the real duration and levels of `PD_SCK`;
- wake-up timing on the installed HX711 module;
- conversion readiness after repeated cycles;
- continuity of real measurements after recovery.

---

## 3. Goals

`v1.3` shall:

- represent runtime fault causes explicitly;
- distinguish recoverable sensor faults from terminal internal faults;
- force measurement and level indication into a safe state immediately;
- cancel incomplete tare or calibration sampling without committing partial
  data;
- preserve the last committed tare offset and calibration factor;
- retry recoverable HX711 faults with a bounded policy;
- stop retrying and latch a terminal fault after the retry budget is exhausted;
- discard input received during recovery so it cannot execute later;
- make recovery and terminal fault indications visually distinct;
- introduce an AVR watchdog HAL;
- feed the watchdog only after a complete `app_update()` returns;
- report the application-visible reset cause when the boot path preserves it;
- keep direct AVR and Arduino-reference builds operational;
- add native tests before physical validation;
- physically validate HX711 power cycling and watchdog reset behaviour.

---

## 4. Non-goals

The milestone shall not add:

- median, moving-average or trimmed-mean filters;
- stable-weight detection;
- general outlier rejection;
- speculative disconnected-load-cell heuristics;
- configurable thresholds or calibration mass;
- EEPROM wear levelling;
- a fault log in EEPROM;
- an RTOS, scheduler, dynamic allocation, classes or templates;
- LoRa communication;
- 24 V output hardware;
- a new ADC backend;
- watchdog feeding from an interrupt;
- automatic tare or automatic recalibration after recovery.

Exact ADC-rail detection and implausible-reading policy belong with measured
signal-robustness work in `v1.4`, unless a real fault observed during `v1.3`
requires a smaller evidence-based check.

---

## 5. Terminology

### Detection

A module observes a condition it can establish directly.

Examples:

- the HX711 did not become ready before a deadline;
- a driver read returned an error;
- the sample collector entered an impossible state.

### Classification

The application assigns a stable fault code and a policy to the detected
condition.

### Recovery

The firmware tries to restore the sensor path while keeping the previous
committed configuration.

### Terminal fault

Normal level indication remains disabled and user reset is required.

### Watchdog reset

The hardware resets the microcontroller because software did not complete and
return from an application iteration within the watchdog interval.

These terms are not interchangeable. A domain fault can be handled while the
watchdog is being fed normally. Conversely, the watchdog can reset software
without knowing which domain operation was active.

---

## 6. Fault inventory for this milestone

The application now uses these stable symbolic and numeric codes:

```c
typedef enum
{
    APP_FAULT_NONE = 0,
    APP_FAULT_HX711_INITIALIZATION = 1,
    APP_FAULT_HX711_STARTUP_TIMEOUT = 2,
    APP_FAULT_HX711_RUNTIME_TIMEOUT = 3,
    APP_FAULT_HX711_READ = 4,
    APP_FAULT_SAMPLE_COLLECTION_TIMEOUT = 5,
    APP_FAULT_SAMPLE_COLLECTION_STATE = 6,
    APP_FAULT_INVALID_ACTIVE_CALIBRATION = 7,
    APP_FAULT_INTERNAL_STATE = 8
} app_fault_code_t;
```

These values form the serial diagnostic contract for `v1.3`. Existing meanings
must not be renumbered if a later release adds another cause.

### Initial policy matrix

| Fault | Detected by | Initial policy |
| --- | --- | --- |
| HX711 initialization failure | `scale_init()` / app | Terminal internal/configuration fault |
| HX711 startup timeout | app deadline | Recoverable sensor fault |
| No runtime conversion before deadline | app health deadline | Recoverable sensor fault |
| HX711 read failure | `scale` result | Recoverable sensor fault |
| Multi-sample collection timeout | app deadline | Recoverable sensor fault |
| Impossible collector status | app invariant | Terminal internal fault |
| Invalid active calibration factor | scale/app validation | Terminal configuration fault |
| Impossible application state | app invariant | Terminal internal fault |

An exhausted retry budget is not required to replace the original cause with a
new cause. Diagnostics should retain the original code and additionally report
that recovery attempts were exhausted.

---

## 7. Conditions that are not terminal system faults

Several existing failures already have safe transactional behaviour:

- a tare record cannot be saved;
- a calibration record cannot be saved;
- a calibration mass produces too little signal;
- the user cancels tare or calibration;
- no stored calibration exists;
- no stored tare exists.

These conditions shall not automatically enter terminal fault. The application
can keep the previous committed configuration or require a new tare.

Persistent-record loading should eventually distinguish:

```text
valid
absent or deliberately cleared
invalid or corrupt
storage access failure
```

The safe startup policy remains:

- missing/invalid calibration: use the compile-time default and print a clear
  diagnostic;
- missing/invalid tare: disable normal level indication and require tare;
- invalid compile-time or active calibration: terminal fault.

The status-rich storage API may be introduced in a separate reviewable commit
inside `v1.3`; it must not be mixed into the first sensor-recovery change.

---

## 8. Safe-state policy

Entering any recoverable or terminal system fault shall immediately:

1. set `measurement_available = false`;
2. cancel any incremental sample collection;
3. reset the normal level indicator;
4. prevent normal weight printing and level decisions;
5. suppress button holds until release;
6. consume and discard queued serial input;
7. preserve the last committed tare and calibration values;
8. select the appropriate operation-indicator pattern.

No automatic recovery path may:

- save EEPROM;
- change the tare offset;
- change the calibration factor;
- resume halfway through tare or calibration;
- replay an input received while the fault was active.

If the HX711 recovers, the application returns only to an idle state:

```text
valid tare available  -> APP_STATE_NORMAL_OPERATION
no valid tare         -> APP_STATE_TARE_REQUIRED
```

If a calibration zero had already been committed before a later sensor fault,
that committed tare remains valid. The incomplete calibration factor does not.

---

## 9. Application states

The generic `APP_STATE_FAULT` shall be refined into explicit control states.
The expected logical flow is:

```text
detected recoverable fault
        |
        v
APP_STATE_FAULT_RECOVERY_BACKOFF
        |
        v
APP_STATE_FAULT_RECOVERY_WAIT_FOR_SCALE
        | ready
        v
safe idle state

backoff/wait failure repeated up to the retry limit
        |
        v
APP_STATE_TERMINAL_FAULT
```

The original fault code, attempt count and recovery deadline are application
state. They must be reset explicitly during `app_init()`.

The recovery states must remain cooperative. Waiting for an HX711 conversion is
performed by polling `scale_is_ready()` once per `app_update()`, never by a new
busy loop.

---

## 10. Recovery timing policy

Initial constants shall be explicit in `config.h`:

```text
FAULT_RECOVERY_BACKOFF_MS         = 500 ms
FAULT_RECOVERY_READY_TIMEOUT_MS   = 2000 ms
FAULT_RECOVERY_MAX_ATTEMPTS       = 3
SCALE_RUNTIME_READY_TIMEOUT_MS    = 2000 ms
WATCHDOG_TIMEOUT                  = 2 s hardware period
```

The numeric values are provisional until hardware validation, but the policy is
fixed:

- all elapsed-time checks use unsigned subtraction and remain safe across
  `millis()` overflow;
- a retry count is finite;
- a new attempt begins only after its backoff;
- a ready timeout is cooperative;
- watchdog timeout and sensor timeout solve different problems.

The watchdog may continue to be fed during a two-second sensor wait because the
software is alive and the domain deadline is responsible for that wait.

---

## 11. Scale API preserves error information

The scale read interface now returns a status-rich result:

```c
typedef enum
{
    SCALE_READ_NO_DATA,
    SCALE_READ_VALUE,
    SCALE_READ_ERROR
} scale_read_status_t;

scale_read_status_t scale_try_read_weight(
    float *weight_grams
);
```

The output weight is written only for `SCALE_READ_VALUE`.

`SCALE_READ_NO_DATA` means that the immediate non-blocking poll found no ready
conversion. `SCALE_READ_ERROR` means that the output pointer was invalid or the
driver failed after readiness was observed. The application fault-policy commit
will consume this distinction; the boundary commit deliberately does not yet
change application recovery behaviour.

The sample collector already distinguishes in-progress, complete and error.
The application deadline continues to distinguish a stalled collector from an
immediate read failure.

Low-level driver statuses remain inside the driver/scale boundary unless a
specific distinction changes application policy. Do not expose protocol detail
merely to produce longer messages.

---

## 12. Runtime readiness supervision

Normal operation currently waits indefinitely when `scale_try_read_weight()`
keeps reporting no new conversion. `v1.3` shall track the last successful or
known-ready sensor activity.

At 10 SPS a healthy conversion normally arrives about every 100 ms. A 2000 ms
deadline therefore provides substantial margin without confusing an ordinary
sampling interval with a fault.

The deadline is active only in states that expect conversions. User-controlled
waiting states must not time out merely because the application deliberately
does not consume a ready conversion.

---

## 13. HX711 power-cycle recovery

The scale layer now exposes:

```c
bool scale_recover(void);
```

This bounded recovery operation:

1. cancels any sample collection;
2. places the initialized HX711 in power-down;
3. brings `PD_SCK` LOW again;
4. preserves the runtime tare offset;
5. preserves the runtime calibration factor;
6. returns without waiting for the next conversion;
7. reports failure when either driver power operation fails.

The application then enters the cooperative ready-wait state.

The current product uses channel A, gain 128, and the scale API does not expose
a gain-changing operation. Therefore the current `hx711_power_up()` path lowers
`PD_SCK` and returns without waiting for a conversion. If future non-default
gain support makes it wait for and discard a conversion, that blocking path must
not silently enter this recovery design. The scale recovery contract must
remain bounded and documented.

The application does not call `scale_recover()` until the later cooperative
recovery-state commit defines when attempts start and how they are counted.

Automatic recovery shall not be declared complete until repeated real hardware
power cycles pass.

---

## 14. Recovery input policy

During recovery:

- D4 and D8 are sampled so debouncers remain current;
- candidate presses and holds are suppressed until release;
- one received UART command may produce `Recovery in progress.`;
- all remaining buffered UART input is discarded;
- no input is queued for the recovered idle state.

During terminal fault:

- the same input suppression applies;
- UART reports the stable fault code and `Reset required.`;
- no service command changes configuration.

The state transition out of recovery reserves the entire current update. Normal
work begins on the following `app_update()`, preserving the boundary rule from
`v1.2`.

---

## 15. LED policy

The three LEDs remain shared between normal level and operation indication.

The proposed patterns are:

| Condition | Pattern |
| --- | --- |
| Recovery attempt/backoff | LOW and HIGH alternate every 250 ms |
| Terminal fault | HIGH blinks persistently at the existing slow rate |
| Watchdog reset report | Console only during startup |

The recovery pattern is intentionally distinct from:

- VERY_LOW: LOW alone blinks;
- tare required: all LEDs blink slowly;
- tare sampling: all LEDs remain on;
- calibration zero: LOW alone blinks slowly;
- calibration mass: MEDIUM alone blinks;
- terminal fault: HIGH alone blinks.

Normal level indication never owns the LEDs during recovery or terminal fault.

---

## 16. Watchdog responsibility

The watchdog detects one class of failure:

```text
The program stopped completing application iterations.
```

It does not detect:

- an HX711 that is disconnected while the superloop still runs;
- a logically incorrect but fast state transition;
- a stale measurement unless the application deadline detects it;
- a corrupt EEPROM record unless the storage layer validates it.

Therefore the watchdog is the last layer, not the fault manager.

---

## 17. Watchdog feed point

The watchdog shall be fed after, not before, one complete application update:

```c
while (true)
{
    app_update();
    hal_watchdog_kick();
}
```

The Arduino reference entry point follows the same rule in `loop()`.

Feeding from a timer interrupt is forbidden. An interrupt can continue running
while the main program is deadlocked; feeding there would hide the failure the
watchdog is meant to detect.

Returning from a deliberate terminal-fault update is healthy software
execution, so the watchdog is still fed. A domain fault must not cause an
unexplained reset loop.

---

## 18. Watchdog startup policy

AVR reset handling must occur early enough to avoid inheriting an enabled
watchdog after a watchdog reset.

The intended order is:

```text
early startup hook
-> capture application-visible reset flags
-> clear watchdog reset flag
-> disable inherited watchdog
-> normal C/C++ startup
-> app_init() establishes safe outputs and state
-> enable watchdog
-> repeated app_update() + kick
```

Enabling after `app_init()` is conservative: all current initialization is
bounded, and safe GPIO/indicator state exists before watchdog supervision
begins. Any future long initialization must be reviewed against this rule.

---

## 19. Watchdog HAL

A small C-compatible HAL is expected, for example:

```c
typedef enum
{
    HAL_RESET_CAUSE_UNKNOWN      = 0U,
    HAL_RESET_CAUSE_POWER_ON     = 1U << 0,
    HAL_RESET_CAUSE_EXTERNAL     = 1U << 1,
    HAL_RESET_CAUSE_BROWN_OUT    = 1U << 2,
    HAL_RESET_CAUSE_WATCHDOG     = 1U << 3
} hal_reset_cause_t;

hal_reset_cause_t hal_watchdog_get_reset_cause(void);
void hal_watchdog_enable(void);
void hal_watchdog_kick(void);
void hal_watchdog_disable(void);
```

Exact names may change. The interface must remain minimal and testable through
a native fake at the application boundary.

Both current firmware environments target the ATmega328P, so one AVR backend
may serve both. The build filters must make that choice explicit.

---

## 20. Reset-cause limitation with a bootloader

The Nano new-bootloader path may inspect or clear `MCUSR` before the application
starts. Therefore `v1.3` must not promise reliable original reset-cause
reporting until it is tested on the actual upload/boot path.

The release may report only the flags visible to the application. If the
bootloader removes them, the honest result is:

```text
Reset cause unavailable or unknown on this boot path.
```

Watchdog reset operation can still be valid even if the original cause cannot
be reported reliably. The two acceptance criteria are separate.

---

## 21. Console diagnostics

Each system fault is printed once when detected, not on every superloop
iteration.

Suggested format:

```text
FAULT 03: HX711 runtime conversion timeout.
Normal level indication disabled.
Recovery attempt 1 of 3 in 500 ms.
```

On success:

```text
HX711 recovery succeeded.
Previous tare and calibration remain active.
Normal measurement resumed.
```

On exhaustion:

```text
FAULT 03: HX711 runtime conversion timeout.
Recovery attempts exhausted.
Reset required.
```

Numeric codes must remain stable within the release so a field report can be
matched to one symbolic cause.

---

## 22. Transactional guarantees

The `v1.2` persistence invariants remain mandatory:

### Operational tare

```text
collect candidate
-> save and verify candidate
-> apply candidate to RAM
```

### Calibration zero

```text
collect zero
-> save and verify tare
-> apply tare
-> wait for mass
```

### Calibration factor

```text
calculate candidate
-> validate candidate
-> save and verify candidate
-> keep candidate active
```

Sensor recovery may cancel an unfinished transaction, but it may not change the
last completed commit point.

---

## 23. Native test strategy

Native tests shall cover at least:

### Fault policy

- every fault code maps to one policy;
- unknown codes map to terminal/internal handling;
- console names/codes remain stable where exposed.

### Scale

- no-data is distinct from read error;
- successful value writes the output;
- error never overwrites the output;
- recovery cancels an in-progress collector;
- recovery preserves tare and calibration;
- power-down precedes power-up;
- driver recovery failure propagates.

### Application

- startup timeout enters bounded recovery rather than generic terminal fault;
- runtime no-data reaches the exact deadline;
- read failure starts recovery immediately;
- collector timeout cancels the active operation;
- recovery never resumes halfway through tare/calibration;
- each failed attempt increments exactly once;
- success returns to the correct safe idle state;
- retry exhaustion enters terminal fault;
- terminal internal faults are never retried;
- inputs during recovery are consumed and not replayed;
- buttons held through recovery remain suppressed until release;
- level indication stays disabled throughout recovery;
- tare and factor remain unchanged;
- millisecond wraparound preserves all deadlines;
- fault messages are emitted once per transition.

### Watchdog integration

- app receives the fake reset cause during startup;
- watchdog is enabled only after application initialization;
- the main feed rule is verified by code review and target test;
- both AVR target environments compile.

Hardware watchdog registers are not meaningfully proven by a native fake.

---

## 24. Physical HX711 power-cycle validation

The real Nano, HX711, load cell and serial monitor shall verify:

1. Establish a valid persistent tare and calibration.
2. Record a stable empty and loaded measurement.
3. Execute at least ten controlled power-down/power-up cycles.
4. Confirm `PD_SCK` goes HIGH for power-down and LOW for power-up.
5. Confirm a fresh conversion becomes ready after every cycle.
6. Confirm no cycle silently changes the configured gain.
7. Confirm tare offset and calibration factor remain unchanged in RAM.
8. Confirm post-cycle weights return near the pre-cycle values.
9. Confirm a failed recovery never enables normal level indication.
10. Repeat through the automatic application recovery path.

An oscilloscope or logic analyser is useful for the `PD_SCK` timing check, but
measurement continuity and application state must also be observed.

This test closes the deliberately pending power-control item; it must be
recorded as real hardware evidence, not inferred from native tests.

---

## 25. Physical fault-recovery validation

At minimum:

- start with DOUT prevented from becoming ready and verify bounded retries;
- restore the connection during an early attempt and verify automatic recovery;
- leave the fault present and verify terminal fault after the exact retry count;
- introduce the fault during normal measurement;
- introduce it during tare sampling;
- introduce it during calibration zero sampling;
- introduce it during calibration mass sampling;
- verify no partial tare or factor is saved;
- hold D4 and D8 across a recovery boundary and verify no delayed action;
- send UART commands during recovery and verify they are discarded;
- verify the LED recovery and terminal patterns;
- verify the recovered system uses the previous committed configuration.

Electrical manipulation must not short `DOUT`, `PD_SCK`, VCC or ground. Use a
controlled disconnection or another test method defined before the physical
procedure is executed.

---

## 26. Physical watchdog validation

A dedicated test build shall intentionally stop returning from the main
execution path without feeding the watchdog.

It shall verify:

- normal firmware does not reset spuriously;
- the forced stall produces a hardware reset near the selected watchdog period;
- outputs return to the normal safe startup sequence after reset;
- watchdog reset does not create an endless boot loop;
- application-visible reset-cause reporting is recorded as supported or
  unavailable on the real Nano bootloader path;
- direct AVR and Arduino-reference entry points use the same feed rule.

The intentional stall hook must not be enabled in the production environment.

---

## 27. Proposed implementation sequence

The milestone should remain reviewable through small commits:

1. `docs: define fault recovery and watchdog design`
2. `refactor: expose scale read and recovery status`
3. `feat: add explicit application fault policy`
4. `feat: add cooperative HX711 recovery states`
5. `feat: supervise runtime HX711 readiness`
6. `feat: add recovery and terminal fault indicators`
7. `feat: distinguish persistent configuration load status`
8. `feat: add AVR watchdog and reset-cause HAL`
9. `test: add watchdog hardware validation build`
10. `docs: record fault recovery and watchdog validation`
11. `docs: finalize v1.3 release documentation`

The exact split may change if one diff is too large, but watchdog activation
must remain after the recovery model is explicit and tested.

Steps 1 through 3 are now implemented on the feature branch.

---

## 28. Definition of done

- [x] Fault codes are explicit and documented.
- [x] Each code has one recovery or terminal policy.
- [x] Normal level output is disabled immediately on system fault.
- [x] Scale no-data and read-error results are distinguishable.
- [ ] Runtime readiness has a finite cooperative deadline.
- [ ] HX711 recovery has bounded backoff, timeout and retry count.
- [ ] Recovery preserves committed tare and calibration.
- [ ] Interrupted tare/calibration never resumes halfway through.
- [ ] Inputs are not replayed across fault-state transitions.
- [ ] Recovery and terminal LED patterns are distinct.
- [ ] Power-down/power-up pass a dedicated physical test.
- [ ] The watchdog is fed only after complete application iterations.
- [ ] The watchdog is never fed from an interrupt.
- [ ] A forced software stall causes a real watchdog reset.
- [ ] Reset-cause support or bootloader limitation is documented from evidence.
- [ ] Direct AVR production builds successfully.
- [ ] Arduino reference builds successfully.
- [ ] All native tests pass.
- [ ] SRAM and flash usage are recorded.
- [ ] Physical recovery scenarios pass.
- [ ] README, roadmap, seed, validation record and release notes agree.
- [ ] The release is merged and tagged only after physical validation.

---

## 29. First implementation result

The watchdog remains disabled.

The first code change made the scale boundary status-rich and testable:

```text
no conversion ready
successful conversion
driver read error
```

It also added a bounded scale-level HX711 power-cycle operation that cancels an
active collector and preserves tare and calibration. Native tests cover the
successful order, power-down failure and power-up failure. Only after these
contracts exist does the next commit introduce application fault codes and
policy; automatic retries and watchdog activation still do not belong here.

---

## 30. Explicit application fault-policy result

The watchdog and automatic retry states remain disabled.

The application now records one stable cause whenever a system fault is
detected. A single safe-state entry path:

1. normalizes unknown codes to `APP_FAULT_INTERNAL_STATE`;
2. disables measurement availability;
3. cancels any incremental sample collection;
4. resets normal level indication;
5. suppresses both button holds until release;
6. discards queued serial input;
7. preserves the committed tare and calibration values;
8. emits one numbered diagnostic before latching the current fault state.

The pure `app_fault` module maps startup/runtime sensor timeouts, HX711 read
failure and collection timeout to `APP_FAULT_POLICY_RECOVER_SENSOR`.
Initialization failure, impossible collector state, invalid active calibration
and internal application state map to `APP_FAULT_POLICY_TERMINAL`. Unknown
numeric values normalize to fault `08` and therefore fail safely.

Until the next commit adds cooperative backoff and ready-wait states, both
policies deliberately share the existing latched fallback. This intermediate
state is safe and fully diagnosed, but it is not yet automatic recovery.
