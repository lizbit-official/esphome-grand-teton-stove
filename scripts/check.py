#!/usr/bin/env python3
"""Validate optional-package combinations with temporary, non-live credentials.

Use --compile to compile every variant. Never uploads or contacts a stove.
Run with the Python interpreter where requirements.txt is installed.
"""
import argparse
from importlib.metadata import version
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
VARIANTS = {
    "stove.yaml": (False, False),
    "stove-with-lid.yaml": (True, False),
    "stove-with-level.yaml": (False, True),
    "stove-with-hopper.yaml": (True, True),
    "stove-sniffer.yaml": (False, False),
}
FAKE_SECRETS = """
wifi_ssid: test-only-not-a-real-network
wifi_password: test-only-wifi-password
api_encryption_key: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=
ota_password: test-only-ota-password
web_username: test-only-user
web_password: test-only-web-password
"""


def run(command, cwd):
    started = time.monotonic()
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        print(result.stdout[-12000:], file=sys.stderr)
        print(result.stderr[-12000:], file=sys.stderr)
        raise SystemExit(result.returncode)
    return result.stdout, round(time.monotonic() - started, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile", action="store_true", help="also build all firmware variants")
    parser.add_argument("--variant", action="append", choices=VARIANTS,
                        help="limit compilation to these variants (all still validate)")
    parser.add_argument("--report", type=Path, help="write a JSON check summary")
    args = parser.parse_args()
    report = {"esphome_version": version("esphome"), "host_tests": False, "variants": {}}

    with tempfile.TemporaryDirectory(prefix="stove-check-") as directory:
        work = Path(directory)
        # Copy only build inputs; never copy local secrets, logs or firmware.
        for name in VARIANTS:
            shutil.copyfile(ROOT / name, work / name)
        for name in ("packages", "includes", "tests"):
            shutil.copytree(ROOT / name, work / name)
        (work / "secrets.yaml").write_text(FAKE_SECRETS)
        binary = work / "reporting-test"
        run(["g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
             str(work / "tests/stove-reporting-test.cpp"), "-o", str(binary)], work)
        output, _ = run([str(binary)], work)
        print(output.strip(), flush=True)
        report["host_tests"] = True

        for name, (has_lid, has_level) in VARIANTS.items():
            config, _ = run([sys.executable, "-m", "esphome", "config", name], work)
            # These check package composition, not just YAML syntax. Sensor
            # packages must work independently and merge shared state once.
            assert ("name: Hopper Lid\n" in config) == has_lid, name
            assert ("name: Hopper Pellet Level Estimated\n" in config) == has_level, name
            assert ("name: Hopper IR Voltage\n" in config) == has_level, name
            if has_lid:
                assert re.search(r"trigger_on_initial_state: (?:true|True)", config), name
            if has_lid or has_level:
                assert config.count("id: hopper_lid_enabled\n") == 1, name
            if name == "stove-sniffer.yaml":
                assert "tx_pin:" not in config and "climate:" not in config, name
            else:
                assert "active_state:" not in config, name
            report["variants"][name] = {"config": True, "compiled": False}
            print(f"Validated {name}: lid={has_lid}, level={has_level}", flush=True)
            if args.compile and (not args.variant or name in args.variant):
                print(f"Compiling {name} ...", flush=True)
                _, seconds = run([sys.executable, "-m", "esphome", "compile", name], work)
                report["variants"][name].update(compiled=True, seconds=seconds)
                print(f"Compiled {name} ({seconds}s)", flush=True)

        # Also validate the optional authenticated web package and the
        # calibrated combined setup, without publishing real credentials.
        (work / "stove-web-test.yaml").write_text("""
packages:
  base: !include stove-with-hopper.yaml
  web: !include packages/web-server.yaml
substitutions:
  hopper_reference_voltage: "0.444"
  hopper_reference_distance_cm: "23.0"
  hopper_full_distance_cm: "3.0"
  hopper_empty_distance_cm: "33.0"
""")
        run([sys.executable, "-m", "esphome", "config", "stove-web-test.yaml"], work)
        report["authenticated_web_config"] = True
        if args.compile and (not args.variant or "stove-with-hopper.yaml" in args.variant):
            print("Compiling authenticated web + calibrated hopper variant ...", flush=True)
            _, seconds = run([sys.executable, "-m", "esphome", "compile", "stove-web-test.yaml"], work)
            report["authenticated_web_compiled"] = True
            print(f"Compiled authenticated web variant ({seconds}s)", flush=True)

    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    print("All checks passed; no firmware uploaded.", flush=True)


if __name__ == "__main__":
    main()
