# Contributing

Please include the exact stove model, board revision, MCU firmware, ESP board,
ESPHome version, and whether the OEM Wi-Fi module is connected when reporting
a hardware issue. Keep the original interlocks and protections functional.

For new datapoints or phases, include the raw code and matching physical-panel
label, with relevant temperatures and selected mode. Distinguish observed
values from assumptions. Avoid broad writes or speculative control options
based on another brand's mapping.

Run these checks in a Python environment with `requirements.txt` installed:

```sh
python scripts/check.py
python scripts/check.py --compile
```

Both use temporary fake credentials and do not upload firmware. New optional
hardware should compile both with and without the other add-ons.

Do not commit `secrets.yaml`, local device configs, credentials, network dumps,
firmware binaries, or images with private metadata. Share only the relevant
UART frames or sensor observations. Image uploads should show the hardware
or behavior without account details or location metadata.

Changes to activity reporting must preserve read-only use of DP112. Review
ESPHome callback ordering when changing the pinned version. Add evidence and
tests for new behavior rather than labeling unobserved states as verified.
