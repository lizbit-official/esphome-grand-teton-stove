# Observed Tuya MCU protocol

Serial transport: 9600 baud, 8N1, 5 V logic at the stove header. The tested
Grand Teton Collection **Gros Ventre F500460** controller is an ATmega64A,
Tuya MCU firmware 1.0.0, product ID `mkawgq5ywi0feooe`. The retail listing also
identifies this stove as N60WTS; see [tested stove details](../README.md#tested-stove-and-compatibility).
These observations do not establish compatibility with other board revisions,
other Grand Teton models, or Cleveland Iron Works stoves.

## Exposed datapoints

| DP | Type | Meaning | Evidence / access |
| --- | --- | --- | --- |
| 1 | Boolean | Heat request | Heat and Off acknowledged; startup/shutdown seen on panel; read/write |
| 101 | Enum | Heat preset: 0=P1, 1=P2, 2=P3, 3=P4 | All panel options and writes confirmed |
| 102 | Integer | Target °C | 24 °C write acknowledged; panel showed 75 °F; read/write |
| 104 | Enum | ECO: 0=ECO1, 1=ECO2 | Panel and both writes confirmed; ECO-disable value unknown |
| 107 | Integer | Room °C | Matched panel/nearby temperature; read only |
| 108 | Integer | Pipe/exhaust °C | Tracked ignition and cooldown; read only |
| 109 | Integer | Protection temperature °C | Reported by MCU; read only |
| 112 | Enum | Operating phase | 10 observed in standby, 12 correlated with ECO2/running; read only |
| 113 | Bitmap | Undecoded status | Zero and 0x40 observed; bit meanings unverified; read only |
| 122 | Integer | Running hours | 4176 matched panel information; read only |

The target is reported in Celsius even when the panel displays Fahrenheit.
The climate advertises 1 °C target/current resolution, with a 10–30 °C UI
range inherited from the tested configuration. The entire selectable range
has not been exhaustively written to the stove.

P1–P4 represent the selected heat preset. ECO2 may internally use a different
low-output setting, so the selected preset is not a measurement of actual
instantaneous feed or fan output.

## Activity reporting

Without active-state feedback, ESPHome's Tuya climate infers action from room
and target temperatures. That reported Idle during confirmed ECO2 burning
above the setpoint. The helper now recognizes code 12 with an acknowledged
Heat request and active MCU communication as Heating.

The mapping is intentionally incomplete. Unknown phase codes remain visible
as `Unknown (code N)`. The climate has no Unknown action and uses Idle for
those codes or a stale link. Off mode reports Off; this does not mean a hot
stove has completed its shutdown/cooldown sequence. No independent flame
sensor is part of this project.

The implementation uses `climate.on_state`, which in the tested ESPHome version
runs before API/web clients are notified. It changes reporting only. **Do not
configure DP112 as Tuya climate `active_state`**: the component also writes
that enum during some mode commands. Recheck source behavior when upgrading
ESPHome.

## Observed but not exposed for writing

| DP | Type | Association |
| --- | --- | --- |
| 106 | Raw, 2 bytes | Stir duration in seconds, interval in minutes; panel changes isolated |
| 114 | Raw | Time settings; some fields identified in batches |
| 115 | Raw | Temperature settings; several fields tentative |
| 116 | Raw | Function settings; individual flags not fully isolated |
| 118 | Raw | Blower settings |
| 119 | Raw | Feeding settings |
| 120 | Raw | Matched E1/E2 counters in one panel snapshot |

Permitted ranges, write semantics, and several byte assignments are unresolved.
In particular, DP113=0x40 appeared during a batch of function-menu changes and
cleared when reverted; that does not identify a particular fault bit. Zero
values alone did not establish mappings for E8 time, air gap, or exhaust fan
information. No remote writes to these settings are included.

## Mapping another phase

Record the exact panel label alongside `Stove Operating State Code`, selected
ECO/preset, and room/exhaust temperature. Capture transitions during ordinary,
attended operation; a selected ECO mode is not itself proof of the current
operating phase. Report unknown codes before adding new control writes.

References: [ESPHome Tuya MCU](https://esphome.io/components/tuya/),
[Tuya climate active-state detection](https://esphome.io/components/climate/tuya/#active-state-detection).
