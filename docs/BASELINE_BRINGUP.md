# PR 2 — Standalone ADV baseline bring-up

This PR provides a deliberately narrow, independently written firmware smoke test. **It is not SquachWatch**, does not import its source, and does not yet claim any BLE/LoRa/GNSS/HUD behavior.

## Rationale
The upstream SquachWatch build guide identifies `cardputer-adv` as a **beta** target. That build can be evaluated separately with `pio run -e cardputer-adv`, without embedding a GPL-derived codebase in this repo before deciding our upstream strategy. The upstream target uses a hyphen: **cardputer-adv**, not `cardputer-adv` with a different spelling or `cardputer`.

## Prerequisites
- M5Stack Cardputer ADV and data-capable USB-C cable
- PlatformIO CLI or VS Code PlatformIO extension
- No LoRa Cap needed for this first test. For later LoRa testing, attach the appropriate antenna before powering its radio.

## Build and flash
From repository root:

```sh
pio run -e cardputer-adv
pio run -e cardputer-adv -t upload
pio device monitor -b 115200
```

PlatformIO may require a serial `--upload-port` in some environments. Use a known-good USB cable and verify device selection before flashing. This will replace firmware currently on the Cardputer; preserve a recovery path beforehand.

Expected LCD: Callingcard title, survey iteration, 2.4 GHz AP count, up to five abbreviated SSIDs and RSSI, free heap. Expected serial: a boot marker and one summary per scan. Each scan is **blocking** and repeats after an interval: counts aren't continuous or exhaustive. No network joining or active traffic transmission is intended.

## Physical verification checklist
- [ ] Compile with the chosen environment (record PlatformIO and framework versions).
- [ ] Flash and boot on **ADV**, not original Cardputer.
- [ ] LCD text has correct rotation and no clipped fatal errors.
- [ ] Serial monitor is readable at 115200 baud.
- [ ] Nearby 2.4 GHz AP count appears and is plausible.
- [ ] Survey repeats and free heap is roughly stable over 30 minutes.
- [ ] Record any crash/reset, memory trend and battery effect.
- [ ] Test with Cap physically installed only after checking mechanical/electrical conflicts.

If the initial toolchain or display API fails, record the exact compiler error and hardware revision; adjust before adding features. No hardware run or compilation is claimed by this PR.

## Upstream research runbook
```sh
git clone https://github.com/skizzophrenic/SquachWatch-CYD.git
cd SquachWatch-CYD
git rev-parse HEAD  # record exact upstream revision
pio run -e cardputer-adv
make -C test
```

Read upstream `LICENSE`, `docs/BUILD.md`, `docs/DESIGN.md`, `platformio.ini` and Cardputer-specific guards. Compare what is available in its beta target with our baseline. Never flash an unreviewed release or assume support for the LoRa-1262 Cap. If adopting upstream code, retain applicable copyright/license notices and address GPL-3.0 obligations before distribution.

## Next PR
Bring up BLE advertisement scanning, with intentional Wi-Fi/BLE time scheduling and observability for detection gaps. Then add Cap GNSS and SX1262, followed by durable logging and Blade telemetry.
