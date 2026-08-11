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

This document is the completed design contract and incremental implementation
record. Cooperative HX711 recovery, runtime readiness supervision, distinct
recovery and terminal-fault indicators, persistent-load classification, the
AVR watchdog HAL and dedicated watchdog hardware-validation builds are
implemented. Native, build and physical validation passed on the real Nano.
The physical matrix includes watchdog stalls in both entry-point environments,
deterministic `DOUT` disconnection, repeated real power cycles, direct `PD_SCK`
measurement, interrupted tare and calibration operations, input suppression
across recovery and a final normal-operation regression.

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

That state was cooperative and safe, but it lost the cause and always required
a reset. `v1.3` separates fault detection, classification, recovery and
watchdog supervision.

---

## 2. HX711 validation pending at milestone start

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

The normal-operation automatic path has now been exercised physically, as
recorded in section 38. That result proves that the installed system can detect
a controlled `DOUT` disconnection, run bounded recovery and resume real
measurements after reconnection. The dedicated repeated power-cycle and timing
checks listed in section 24 subsequently passed and are recorded in section 39.

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
    APP_FAULT_INTERNAL_STATE = 8,
    APP_FAULT_PERSISTENT_STORAGE_ACCESS = 9
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
| Persistent storage access failure | storage/app boundary | Terminal storage fault |

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

Persistent-record loading now distinguishes:

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
- storage access failure: terminal fault with reset required;
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

Normal operation now tracks the last successful sensor activity. Every entry
into `APP_STATE_NORMAL_OPERATION` starts a fresh health window, and every
`SCALE_READ_VALUE` renews it. `SCALE_READ_NO_DATA` remains an ordinary
non-blocking result while less than `2000 ms` has elapsed.

At 10 SPS a healthy conversion normally arrives about every 100 ms. A 2000 ms
deadline therefore provides substantial margin without confusing an ordinary
sampling interval with a fault.

At the exact deadline, a valid value wins because the result is evaluated before
the elapsed-time check. Otherwise, `NO_DATA` records
`APP_FAULT_HX711_RUNTIME_TIMEOUT` and enters the existing recovery FSM. Unsigned
subtraction keeps the comparison correct across `millis()` overflow.

The deadline is active only in normal operation. Startup, recovery, tare,
calibration, result patterns and `TARE_REQUIRED` use their own timing policy or
deliberately consume no normal measurements.

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

The implemented patterns are:

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
The timing is centralized in `FAULT_RECOVERY_INDICATOR_PERIOD_MS`; the
application selects `OPERATION_INDICATOR_RECOVERY`, while
`operation_indicator` owns the non-blocking phase and physical LED outputs.

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

The dedicated hardware-validation trigger also has native logic tests proving
that it:

- starts disarmed;
- cannot trigger while both buttons remain held across reset;
- arms only after both buttons have been observed released;
- requests a stall only when both buttons are later pressed together;
- returns to the disarmed state when reinitialized.

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
controlled disconnection while a 10 kΩ pull-up holds Nano D2 at the shared
logic supply, or another test method defined before the physical procedure is
executed. The pull-up must be on the microcontroller side of the disconnected
conductor. Leaving D2 floating is not a valid deterministic test method.

---

## 26. Physical watchdog validation

Two dedicated test environments intentionally stop returning from the main
execution path without feeding the watchdog:

```text
nanoatmega328new_watchdog_validation
nanoatmega328new_arduino_watchdog_validation
```

Both define `WATCHDOG_HARDWARE_VALIDATION`. The production environments exclude
the trigger module and do not define that macro.

The validation trigger is deliberately manual and one-shot per release cycle:

1. after startup, release both D4 and D8 so the trigger can arm;
2. press D4 and D8 together;
3. the current `app_update()` completes;
4. the validation hook prints its diagnostic and enters an intentional loop;
5. execution never reaches the following `hal_watchdog_kick()`;
6. the hardware watchdog must reset the ATmega328P near its two-second period;
7. after reset, the trigger starts disarmed and cannot fire again while both
   buttons remain pressed;
8. release both buttons before repeating the test.

Interrupts remain enabled during the intentional loop. This allows serial
transmission and the project timebase to continue while proving specifically
that loss of progress through the main execution path is sufficient to stop
watchdog feeding.

It shall verify:

- normal firmware does not reset spuriously;
- the forced stall produces a hardware reset near the selected watchdog period;
- outputs return to the normal safe startup sequence after reset;
- watchdog reset does not create an endless boot loop;
- application-visible reset-cause reporting is recorded as supported or
  unavailable on the real Nano bootloader path;
- direct AVR and Arduino-reference entry points use the same feed rule.

The intentional stall hook must not be enabled in the production environment.
After testing, the direct AVR production build must be uploaded again.

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

Steps 1 through 11 are complete on the feature branch. The detailed acceptance
record is stored in
[`v1.3-fault-recovery-and-watchdog-validation.md`](v1.3-fault-recovery-and-watchdog-validation.md).
Only the final repository operations remain: commit this documentation, merge
the branch and create the annotated release tag.

---

## 28. Definition of done

- [x] Fault codes are explicit and documented.
- [x] Each code has one recovery or terminal policy.
- [x] Normal level output is disabled immediately on system fault.
- [x] Scale no-data and read-error results are distinguishable.
- [x] Runtime readiness has a finite cooperative deadline.
- [x] HX711 recovery has bounded backoff, timeout and retry count.
- [x] Recovery preserves committed tare and calibration.
- [x] Interrupted tare/calibration never resumes halfway through.
- [x] Inputs are not replayed across fault-state transitions.
- [x] Recovery and terminal LED patterns are distinct.
- [x] Persistent loads distinguish absent, corrupt and access-failure states.
- [x] Power-down/power-up pass a dedicated physical test.
- [x] The watchdog is fed only after complete application iterations.
- [x] The watchdog is never fed from an interrupt.
- [x] A forced software stall causes a real watchdog reset.
- [x] Reset-cause support or bootloader limitation is documented from evidence.
- [x] Direct AVR production builds successfully.
- [x] Arduino reference builds successfully.
- [x] All native tests pass.
- [x] SRAM and flash usage are recorded.
- [x] Normal-measurement DOUT disconnection recovers after reconnection and reaches terminal fault after three failed attempts.
- [x] Physical recovery scenarios pass.
- [x] README, roadmap, seed, validation record and release notes agree.
- [x] Release integration and tagging are deferred until after physical validation.

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

---

## 31. Cooperative HX711 recovery result

The watchdog remains disabled, and runtime readiness supervision is still a
separate next commit.

Recoverable sensor faults now enter two explicit cooperative states:

```text
APP_STATE_FAULT_RECOVERY_BACKOFF
APP_STATE_FAULT_RECOVERY_WAIT_FOR_SCALE
```

The application waits `500 ms` before each attempt, calls the bounded
`scale_recover()` power cycle once, and then polls `scale_is_ready()` at most
once per `app_update()`. A conversion ready at the exact `2000 ms` deadline
wins over the timeout. Failed power cycles and ready timeouts consume one
attempt each; the third failure enters `APP_STATE_TERMINAL_FAULT` while
retaining the original fault code.

A successful startup recovery proceeds to the configuration-loading boundary
on the following update. A successful runtime recovery returns to
`APP_STATE_NORMAL_OPERATION` only when a valid tare was already active;
otherwise it returns to `APP_STATE_TARE_REQUIRED`. Interrupted tare and
calibration collections are cancelled and never resumed halfway through.

Buttons remain sampled and hold-suppressed during recovery. UART input is
consumed and discarded, and the transition update is reserved so no command or
button action crosses into the recovered state. Committed tare and calibration
values are never rewritten by recovery.

Ten new integral application tests cover:

- exact backoff and ready deadlines;
- unsigned-time overflow;
- exactly three failed attempts before terminal fault;
- startup and runtime recovery destinations;
- recovery with and without a valid tare;
- input discard and hold suppression;
- preservation of committed tare and calibration;
- cancellation of interrupted tare and calibration workflows.

The `native_app` suite now contains `59` tests. The complete expected native
inventory is `287` tests across `12` suites. Recovery and terminal faults still
share the existing HIGH-LED fault pattern until the dedicated indicator commit.

---

## 32. Runtime HX711 readiness supervision result

The watchdog remains disabled.

Normal operation now owns a finite sensor-health window:

```text
enter normal operation -> start 2000 ms window
SCALE_READ_VALUE       -> accept value and restart window
SCALE_READ_NO_DATA     -> keep polling before the deadline
SCALE_READ_NO_DATA     -> fault 03 at the deadline
SCALE_READ_ERROR       -> fault 04 immediately
```

The window is restarted whenever the application returns safely to normal
operation, including after a successful recovery. Time spent intentionally in
startup, recovery, tare, calibration, `TARE_REQUIRED` or a temporary result
pattern never counts as missing runtime sensor activity.

At the exact deadline a valid value wins and renews the window. Elapsed time is
computed with unsigned subtraction, so supervision remains correct when the
32-bit millisecond counter wraps.

Six new integral application tests cover:

- ordinary `NO_DATA` immediately before the deadline;
- fault `03` at the exact deadline;
- a valid value winning at the deadline and renewing the window;
- unsigned-time overflow;
- a fresh window after successful recovery;
- supervision remaining inactive during a user-controlled calibration wait.

The `native_app` suite now contains `65` tests. The complete expected native
inventory is `293` tests across `12` suites. Recovery and terminal faults still
share the existing HIGH-LED fault pattern until the dedicated indicator commit.

---

## 33. Recovery and terminal fault indicator result

The watchdog remains disabled.

The shared operation-indicator module now owns a persistent recovery mode:

```text
recovery backoff / ready wait -> LOW and HIGH alternate every 250 ms
terminal fault               -> HIGH blinks at the existing 500 ms rate
```

`app` selects `OPERATION_INDICATOR_RECOVERY` only after the fault policy has
classified a cause as recoverable. Direct terminal faults select the existing
fault mode, and exhausting the third recovery attempt replaces the alternating
pattern immediately with the terminal HIGH-LED pattern.

The pattern is non-blocking, starts with LOW illuminated and uses unsigned
elapsed-time subtraction across `millis()` overflow. Normal level indication
remains reset and does not own the shared LEDs in either fault-handling state.

Two new operation-indicator tests cover the exact 250 ms boundary, LOW/HIGH
alternation, persistent-mode semantics and millisecond overflow. Integral
application expectations now prove that every recoverable detection selects
the recovery mode, terminal causes never select it and retry exhaustion changes
to the terminal mode. The `native_operation_indicator` suite now contains `18`
tests; the complete expected native inventory is `295` tests across `12` suites.

---

## 34. Persistent configuration load-status result

The watchdog remains disabled.

Calibration and tare storage now share one explicit load contract:

```text
STORAGE_LOAD_VALID        -> decoded value is returned
STORAGE_LOAD_ABSENT       -> erased or deliberately cleared record
STORAGE_LOAD_INVALID      -> bytes read, but magic/version/CRC/value is invalid
STORAGE_LOAD_ACCESS_ERROR -> capacity or read operation failed
```

The output argument is modified only for `STORAGE_LOAD_VALID`. Erased magic
bytes (`0xFF`) and deliberately cleared magic bytes (`0x00`) are classified as
absent; partial or otherwise malformed identifiers remain invalid rather than
being mistaken for an intentional clear.

Startup applies the status according to the existing safe policy:

- absent calibration uses the compile-time default with an explicit message;
- corrupt calibration also uses the default, but reports corruption;
- absent tare enters `TARE_REQUIRED` with the first-start message;
- corrupt tare enters `TARE_REQUIRED` with a corruption diagnostic;
- a real access failure for either record enters terminal `FAULT 09` and does
  not continue applying a partial startup configuration.

Four new integral application tests cover corrupt and access-error outcomes for
both records. One application-fault test fixes code `09` and its terminal
policy. Existing storage tests now assert all four statuses while continuing to
prove that non-valid loads preserve their output arguments. The `native_app`
suite now contains `69` tests, `native_app_fault` contains `5`, and the complete
expected native inventory is `300` tests across `12` suites.

---

## 35. AVR watchdog and reset-cause HAL result

The watchdog is now active in both ATmega328P firmware environments with a
two-second hardware period.

The AVR backend owns an early `.init3` startup hook. Before ordinary C/C++
initialization and before either `main()` or Arduino `setup()`, it:

1. copies the raw `MCUSR` flags into `.noinit` storage;
2. clears the reset flags, including `WDRF`;
3. disables any watchdog inherited from a previous watchdog reset.

The public C-compatible HAL translates the captured AVR flags into independent
application bits for power-on, external, brown-out and watchdog reset. A zero
mask means that no supported cause remains visible to the application. This is
intentionally described as application-visible evidence because the Nano
bootloader may inspect or clear `MCUSR` before the firmware startup hook runs.

`app_init()` reads the cause once after the console is available and emits one
line for every visible flag. It does not enable, disable or feed the watchdog.
The two entry points apply the same supervision boundary:

```text
app_init()
hal_watchdog_enable()

app_update()
hal_watchdog_kick()
```

There is no watchdog feed in an interrupt, inside `app_update()` or before an
application iteration. A cooperative terminal fault continues to return from
`app_update()` and is therefore still fed; only software that stops completing
iterations should cause a watchdog reset.

One new integral application test verifies unknown-cause reporting, a single
HAL query and simultaneous reporting of every supported cause bit. The
`native_app` suite now contains `70` tests, and the complete expected native
inventory is `301` tests across `12` suites. Native tests do not claim to prove
AVR register timing, the real two-second period or bootloader preservation of
`MCUSR`; those remain gates for the dedicated hardware-validation build.

---

## 36. Watchdog hardware-validation build result

The direct AVR and Arduino-reference validation environments now reuse their
respective ordinary entry points with one compile-time-only hook inserted
between:

```text
app_update()
validation trigger / intentional stall
hal_watchdog_kick()
```

`watchdog_validation.c` contains only the deterministic arming rule. It is
excluded from both production environments and included explicitly by the two
validation environments. The entry points print the validation-build warning,
sample the active-low D4 and D8 inputs and enter the deliberate infinite loop
only after the trigger requests it.

The release-before-press rule prevents an automatic reset loop even when the
user keeps both buttons pressed through the watchdog reset. It also prevents a
board powered up with both buttons held from stalling before the operator has
seen a normal startup.

Six new native tests cover null-state safety, held-at-startup behaviour,
one-button states, explicit arming, the simultaneous-press requirement and
reinitialization. The expected native inventory is now `307` tests across `13`
suites. Native tests prove only trigger logic; the real reset interval, safe
restart and bootloader handling of `MCUSR` still require the physical procedure.

---

## 37. Physical watchdog validation result

Physical validation was completed on 2026-08-10 using the real Arduino Nano,
HX711, load cell, buttons, LEDs and the 115200-baud serial monitor with timestamp
filtering. Both dedicated validation environments were tested:

```text
nanoatmega328new_watchdog_validation
nanoatmega328new_arduino_watchdog_validation
```

Before each forced stall, the application was operating normally with stored
tare offset `-217186`, the default calibration factor `45.589332 counts/g` and
a load of approximately `930 g`. The normal level was `MEDIUM`, represented by
the steady medium/yellow LED.

### Measured resets

| Environment | Stall diagnostic | New startup header | Measured interval | Visible reset cause |
| --- | --- | --- | ---: | --- |
| Direct AVR | `19:47:11.532` | `19:47:13.787` | `2.255 s` | `unknown` |
| Arduino reference, run 1 | `19:53:06.955` | `19:53:09.215` | `2.260 s` | `unknown` |
| Arduino reference, run 2 | `19:53:21.017` | `19:53:23.277` | `2.260 s` | `unknown` |

The additional approximately `255–260 ms` beyond the configured two-second
watchdog period is consistent with reset, bootloader execution and startup code
before the first serial header is emitted.

### Observed recovery behaviour

- Pressing D4 and D8 started the deliberate stall immediately after a complete
  `app_update()` and before the next watchdog feed.
- The medium/yellow LED retained its last valid state during the stall because
  the stalled program did not rewrite the AVR GPIO output registers.
- All three LEDs turned off during reset and early startup while the GPIO pins
  returned to their initial input/high-impedance state.
- The stored tare offset was loaded again after every reset.
- Normal measurements resumed near the previous `930 g` value.
- The medium/yellow LED returned only after initialization and a valid level
  decision completed.
- Keeping D4 and D8 pressed through reset did not cause another automatic
  stall or a repeated-reset loop.
- Both entry-point environments exhibited the same watchdog timing and safe
  recovery behaviour.

### Reset-cause limitation

Every post-watchdog startup reported:

```text
Reset cause visible to application: unknown.
```

The physical reset interval proves that the hardware watchdog caused the
restart, but no supported `MCUSR` flag survived to the application `.init3`
hook. On this Nano bootloader path, reset-cause reporting is therefore recorded
as unavailable. The most likely explanation is that the bootloader reads,
changes or clears `MCUSR` before control reaches the application image.

This is a diagnostic limitation, not a watchdog failure. The two-second
hardware recovery, safe restart, persistent-state restoration and reset-loop
protection all passed. These watchdog results alone do not validate HX711
power cycling or application sensor recovery. Section 38 records the later
normal-measurement recovery test; sections 39 through 42 record the later
electrical, interruption, input-boundary and final-regression results that
closed those gates.

---

## 38. Physical DOUT disconnection and pull-up validation result

Physical validation was performed on 2026-08-10 with the production direct-AVR
environment, the real Nano, HX711, load cell, stored tare offset `-217186`,
default calibration factor `45.589332 counts/g` and a load near `925–930 g`.

### Floating-input observation

An initial test disconnected HX711 `DOUT` without keeping Nano D2 at a defined
logic level. The floating input produced implausible values including:

```text
4763.94 g
-986.15 g
-6736.27 g
7639.00 g
```

A floating D2 can cross the digital threshold because of noise or intermittent
contact. The driver may then observe a false LOW, interpret it as "conversion
ready" and clock 24 meaningless bits. The application also prints the last
stored weight periodically, so one false reading can be repeated until a new
reading arrives or the readiness deadline expires.

Consequently, an unconnected floating input can both delay the intended
timeout and make `HX711 recovery succeeded` mean only that D2 was observed LOW,
not that the physical connection is reliable. This test method was rejected.

### Controlled test circuit

A 10 kΩ resistor was connected from Nano D2 to the shared 5 V logic rail before
disconnecting the HX711 `DOUT` conductor:

```text
Nano +5 V ---- 10 kΩ ----+
                          +---- Nano D2
HX711 DOUT ---------------+
```

The resistor remained on the Nano side throughout the test. Therefore:

- connected HX711: `DOUT` could still drive D2 LOW, drawing approximately
  0.5 mA through the pull-up;
- disconnected HX711: D2 remained deterministically HIGH;
- no direct short between D2 and 5 V was possible.

### Recovery after reconnection

The baseline was stable near `924–930 g` with level `MEDIUM`. The controlled
disconnection then produced:

| Event | Timestamp | Result |
| --- | --- | --- |
| Runtime fault detected | `20:15:25.602` | `FAULT 03`; normal level indication disabled |
| Attempt 1 power cycle | `20:15:26.102` | No ready conversion within 2 s |
| Attempt 2 power cycle | `20:15:28.617` | Waiting for reconnection/conversion |
| Recovery declared | `20:15:29.075` | Attempt 2 succeeded |
| First recovered weight | `20:15:29.083` | `924.08 g`, `MEDIUM` |
| Following valid weight | `20:15:29.586` | `925.83 g`, `MEDIUM` |

The LOW and HIGH LEDs alternated during recovery. After reconnection the
application preserved the active tare and calibration, resumed a physically
reasonable weight and restored the MEDIUM/yellow level without resetting the
Nano.

### Exhausted-retry path

`DOUT` was disconnected again and was deliberately not restored before the
retry budget expired:

| Event | Timestamp |
| --- | --- |
| New runtime fault detected | `20:15:31.250` |
| Attempt 1 power cycle | `20:15:31.758` |
| Attempt 1 timeout | `20:15:33.763` |
| Attempt 2 power cycle | `20:15:34.259` |
| Attempt 2 timeout | `20:15:36.271` |
| Attempt 3 power cycle | `20:15:36.769` |
| Attempt 3 timeout and terminal fault | `20:15:38.775` |

The serial output reported `Recovery attempts exhausted` and `Reset required`.
The alternating recovery pattern stopped and the HIGH LED blinked, matching the
designed latched terminal-fault indication. Reconnecting `DOUT` alone does not
leave this state; the deliberate recovery policy requires a Nano reset after
the wiring fault is corrected.

### Hardware decision

The external 10 kΩ pull-up is retained as part of the prototype wiring. It
makes a broken or disconnected `DOUT` conductor fail HIGH, which maps naturally
to the firmware's finite no-conversion deadline. The resistor must connect to
the common logic rail and remain physically on the microcontroller side of the
connection being supervised.

The pull-up improves detection of the HX711 digital connection. It does not
detect every possible load-cell bridge-wire fault, prove that a recovered value
is coherent, or replace the remaining repeated `PD_SCK` timing tests.

### Internal ATmega328P pull-up alternative

The same fail-high behaviour can be requested without the external resistor by
enabling the ATmega328P's internal pull-up on D2/PD2.

In a simple Arduino program:

```cpp
pinMode(LOADCELL_DOUT_PIN, INPUT_PULLUP);
```

This must remain the effective configuration after HX711 initialization. A
driver or library that later executes `pinMode(DOUT, INPUT)` would disable the
pull-up again.

The current project already provides the portable HAL operation:

```c
hal_gpio_configure_input_pullup((hal_gpio_pin_t)pin);
```

To select the internal alternative in this repository,
`hx711_platform_configure_input()` in `src/hx711_platform.c` would call that
operation instead of `hal_gpio_configure_input()`. No higher HX711-driver or
application change would be necessary. The existing backends implement it as:

```cpp
// Arduino-reference backend
pinMode(pin, INPUT_PULLUP);
```

```c
// Direct AVR equivalent for Nano D2 / ATmega328P PD2
DDRD  &= (uint8_t)~(1U << DDD2);
PORTD |= (uint8_t)(1U << PORTD2);
```

Clearing the `DDRD` bit makes PD2 an input. Setting its `PORTD` latch bit while
it is an input enables the internal pull-up.

The internal option has three relevant limitations compared with the selected
external 10 kΩ resistor:

- it is active only after software configures the GPIO;
- reset, bootloader execution or later reconfiguration can temporarily leave
  the pin without that pull-up;
- its resistance is device-dependent rather than a selected 10 kΩ value.

For those reasons the external resistor remains the project decision and the
internal pull-up remains a documented alternative. The production firmware
should not enable both merely for redundancy; leaving the internal pull-up off
keeps the effective pull-up equal to the known external 10 kΩ value.

### Later validation completed

This section originally closed only the normal-measurement reconnection and
exhausted-retry cases. Sections 39 through 42 close the remaining `v1.3`
physical gates. Requiring several mutually coherent measurements before
declaring recovery remains a possible `v1.4` measurement-robustness change, not
an unrecorded requirement of this release.

---

## 39. Physical PD_SCK and repeated power-cycle result

The production direct-AVR power-down pulse was captured on the real Nano and
HX711 with a x10 probe, 20 MHz bandwidth limit, 50 us/div timebase and a
positive-pulse trigger at 2.5 V.

![HX711 PD_SCK power-down pulse](images/hx711-pd-sck-power-down-pulse.png)

| Measurement | Result |
|---|---:|
| Automatic positive width | `82.49783 us` |
| HX711 requirement | `>60 us` |
| Stable HIGH level | `4.91458 V` |
| Stable LOW level | `-2.083 mV` |

The manual cursor capture independently places HIGH near `4.95 V` and LOW near
`-0.03 V`:

![HX711 PD_SCK level cursors](images/hx711-pd-sck-level-cursors.png)

The complete fault matrix exercised at least 16 automatic HX711 power-cycle
attempts: ordinary reconnection, exact retry exhaustion, interrupted tare,
both calibration phases and input-boundary tests. Successful attempts returned
to real conversions and reasonable pre-fault weights. No successful recovery
showed a gain-scale jump, changed the active tare or factor, or enabled normal
level indication before readiness returned.

Electrical timing, levels and repeated application-path cycling: **PASS**.

---

## 40. Interrupted tare and calibration result

### Operational tare

A `1500 g` load remained on the platform while tare sampling was interrupted.
`FAULT 05` entered recovery on attempt 1. The pre-test tare `-175517` remained
active and was loaded again after reset. Loaded measurements changed only from
a pre-fault mean of `1525.95 g` to a recovered mean of `1521.98 g`; removing the
mass returned near `-5 g`. No partial offset was printed, applied or saved.

### Calibration zero

Calibration-zero sampling was interrupted with the reference load still in
place. Recovery succeeded on attempt 1. The last five pre-fault readings
averaged `1504.35 g`; all 17 recovered loaded readings before unloading
averaged `1495.83 g` instead of becoming zero. After unloading and reset,
factor `45.589332 counts/g` and tare `-176304` were restored. No partial
calibration tare was committed.

### Calibration mass

The zero phase first completed and correctly committed tare `-176317`. The
reference-mass collector was then interrupted. Recovery succeeded on attempt 2
and the loaded mean remained `1548.71 g`. After unloading, the mean was
`-2.29 g`. Reset reported no stored calibration, restored the default
`45.589332 counts/g` factor and loaded tare `-176317`. The completed zero
boundary remained committed while the incomplete factor was discarded.

All three transactional interruption cases: **PASS**.

---

## 41. Recovery input-boundary result

D4 and D8 were held across physical recovery boundaries, including consecutive
recoveries that succeeded on attempts 2 and 3. Neither button produced a
delayed tare or calibration while held, on recovery, or on later release.

The serial sequence `ctq` was sent during recovery. `Recovery in progress.` was
reported during attempts 1 and 2, the bytes were consumed immediately, and no
command executed after recovery succeeded on attempt 2. Normal measurement
resumed near `14-15 g`.

Button suppression, UART discard and absence of replay: **PASS**.

The startup-not-ready condition was physically injected during `v1.2`. Native
`v1.3` application coverage proves that `FAULT 02` enters the same recovery FSM
physically exercised here through runtime and sample-collection timeouts. No
separate `FAULT 02` serial trace was retained for this release.

---

## 42. Final regression and acceptance result

After all fault injection, serial `t` completed normally and saved tare
`-175467`. The first five post-tare readings averaged approximately `0.66 g`.
Serial `c` started calibration, serial `q` cancelled it without changing the
factor, and reset loaded `-175467` with the default `45.589332 counts/g` factor.
The first five post-reset readings averaged approximately `-1.45 g`.

The final native inventory is `307` tests across `13` suites with zero
failures. Final memory usage is:

| Environment | Static SRAM | Flash |
|---|---:|---:|
| Direct AVR production | `230 B` | `17738 B` |
| Arduino reference | `239 B` | `18032 B` |
| Direct AVR watchdog validation | `231 B` | `18076 B` |
| Arduino watchdog validation | `240 B` | `18370 B` |

The milestone satisfies every functional, physical, native and build criterion.
No further fault injection is required before integration. Full evidence and
the release-facing acceptance table are in
[`v1.3-fault-recovery-and-watchdog-validation.md`](v1.3-fault-recovery-and-watchdog-validation.md).
