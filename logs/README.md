# Measurement Capture Inventory

This directory retains unmodified serial-monitor captures from physical
measurement experiments.

The firmware's machine-readable records use:

```text
DATA,sequence,timestamp_ms,raw_counts,tare_offset,net_counts,weight_grams
```

Human-readable startup, control and shutdown messages are intentionally kept in
the same files because they preserve firmware state and configuration context.

## HX711 captures

| File | DATA rows | Duration | Known context |
| --- | ---: | ---: | --- |
| `device-monitor-260825-171711.log` | 8896 | 747.8 s | Restart after approximately 16-17 minutes without power; approximately 18 degC; original shared breadboard return |
| `device-monitor-260825-174520.log` | 2090 | 175.6 s | Same USB cable and source; contains a deliberate load placement/removal; PC coil whine noticed during serial traffic |
| `device-monitor-260826-091013.log` | 1116 | 93.8 s | Follow-up after operating overnight; unloaded zero near +5 g |
| `device-monitor-260827-164211.log` | 0 | n/a | Short human-readable observation after power-return rewiring; capture mode was not started |
| `device-monitor-260827-164621.log` | 1464 | 123.1 s | HX711 supplied directly from Nano; LED/button return separated; DOUT pull-up had been left disconnected |
| `device-monitor-260827-170408.log` | 1557 | 130.9 s | Warm follow-up after restoring the DOUT pull-up to the Nano 5 V rail |
| `device-monitor-260827-171637.log` | 1719 | 144.5 s | Follow-up capture with the restored pull-up |
| `device-monitor-260827-174251.log` | 1678 | 141.1 s | Breadboard decoupling experiment; connections still unsoldered |
| `device-monitor-260827-175623.log` | 1392 | 117.0 s | Follow-up after changing the electrolytic capacitor; connections still unsoldered |

## Preservation rules

- Do not edit raw captures to remove transitions or console messages.
- Derived datasets must use a different file name.
- Record hardware changes, power source, tare, calibration, mass and ambient
  conditions for every new capture.
- Use timestamps and sequence fields instead of assuming a nominal converter
  sample rate.
- Store ADS1232 captures separately from HX711 captures and identify the active
  backend in their metadata.

The conclusions derived from these files are recorded in
[`../docs/hx711-prototype-characterization.md`](../docs/hx711-prototype-characterization.md).
