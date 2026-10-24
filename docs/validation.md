# Validation record and limits

## Hardware observations, October 23–24, 2026

The owner identified the stove as the **Grand Teton Collection Gros Ventre
F500460**, sold as the **2200 sq. ft. model with a 60 lb. hopper**. The retail
listing also identifies it as N60WTS; see [tested stove details](../README.md#tested-stove-and-compatibility).
The original installation used ESPHome 2026.7.4, ESP-IDF 5.5.5, an ESP32-S3
development board, a level shifter, and the stove's ATmega64A/Tuya MCU v1.0.0.
The ESP was USB-powered and the original Wi-Fi module disconnected in
controller mode. The separate hopper switch did not replace the factory
interlock.

| Check | Observation |
| --- | --- |
| Target write | MCU acknowledged 24 °C; physical panel displayed 75 °F |
| Preset/ECO controls | P1–P4 and ECO1/ECO2 writes acknowledged in standby |
| Normal start | Panel progressed through Pre_heating → Feeding → Lighting → ECO2; visible flame reported |
| Normal Off | MCU acknowledged Off; panel showed Switch off |
| Warm restart | Panel progressed Cooling → Lighting → ECO2 after one Heat request |
| Cooling threshold | Exhaust passed the configured 122 °F region before later Lighting; exact threshold semantics not isolated |
| ECO2 reporting | Code 12 correlated with panel ECO2, target 68 °F, room above target, hot exhaust |
| Activity fix | API and web reported Heating through multiple room-temperature changes; user confirmed HA |
| Hopper response | Card movement changed voltage; pellets at ~23 cm gave ~0.444 V |
| Lid gating | HA showed unknown while open and resumed voltage three seconds after closing |
| Provisional level | Approximately 30% reported in API/web; user confirmed HA; later screenshot shows ~23% |

These tests used the installation configuration from which this repository was
extracted. They do not prove that every public configuration variant has been
flashed on physical hardware. The public packaging adds independent optional
sensors, owner-specific calibration, encrypted API, authenticated OTA, and an
optional authenticated web UI. The running stove was not reflashed as part of
publication preparation.

## Repository checks

`scripts/check.py` tests the C++ reporting helpers, validates the minimal,
lid-only, level-only, combined, and receive-only examples, checks that optional
entities are present/absent as intended, and validates the web option. With
`--compile`, it compiles every variant plus the authenticated web/calibrated
hopper combination using ESPHome 2026.7.4. CI runs this same command.

Host tests cover the observed ECO2 mapping and unknown codes, link timeout and
timer wraparound, optional lid gating and settling, the measured reference
estimate, invalid calibration, and invalid/out-of-range sensor readings.
All build credentials are temporary test values. Checks never contact or
upload to a stove. Build artifacts are not release assets.

Initial public export checks on **2026-10-24**, using ESPHome **2026.7.4**:

| Configuration | Validation | Firmware compilation |
| --- | --- | --- |
| Minimal controller | Passed | Passed |
| Lid only | Passed | Passed |
| Level only | Passed | Passed |
| Lid + level | Passed | Passed |
| Receive-only sniffer | Passed | Passed |
| Authenticated web + calibrated lid/level | Passed | Passed |

The lid-related builds were repeated after explicitly enabling the initial
GPIO state callback and moving guard initialization before hardware setup.
This ensures a closed lid can become known after boot without requiring a
physical open/close cycle. Host tests and all configuration-composition checks
also passed. Physical verification of the refactored public packages remains
separate from these build checks.

## Still needed

- PCB revision, carrier details, and level-shifter part.
- Numeric phase captures for preheating, feeding, lighting, normal firing,
  ECO1, shutdown, and cooldown.
- Complete shutdown to cold standby and a broader power/network outage matrix.
- Longer unattended-runtime evidence; this is not a claim of certified overnight reliability.
- Actual pellet calibration at several levels, including full/empty mounting geometry.
- Decoded fault/status bits and any other hardware variants.

## Images

The README screenshots and prototype photo were supplied by the project owner
from the working installation. Publication copies have camera/location and
timestamp metadata removed; decoded pixels are unchanged. The screenshots
include optional/custom UI elements beyond the minimal firmware. The prototype
photo is not a recommended enclosure or hot-surface mounting plan.
