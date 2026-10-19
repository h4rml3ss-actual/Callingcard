# Callingcard — Complete Installation, Setup & Verification Guide

**Audience:** Developers with a Cardputer ADV and the M5Stack Cap LoRa-1262. **Status:** Developer prototype, not a verified release. This guide covers the repository as of PR #5 (merged), not a SquachWatch binary or a Vuzix Android app.

## 1. Before you begin

### Hardware checklist
- Cardputer **ADV** (ESP32-S3; verify labeling; this is not the original Cardputer).
- Cap **LoRa-1262**, correct antenna attached **before applying power**.
- USB-C **data** cable; a charge-only cable will not work.
- Mac, Linux or Windows machine; enough space for PlatformIO packages/toolchains.
- Optional microSD card, but **current code does not log to it**.
- No Raspberry Pi, no Blade 2 and no wireless infrastructure are required to run the current prototype.

**Firmware warning:** Uploading overwrites firmware on the Cardputer. If your ADV has applications or settings you need, back them up and make sure you know how to restore your previous firmware before uploading. Disconnect/reseat the Cap only when the system is powered off. Avoid short circuits and never power a LoRa transmitter without its correct antenna.

### Software prerequisites
- Git command-line client.
- Python 3 and `python3 -m pip` (Windows: `py -m pip`).
- PlatformIO Core CLI **or** VS Code with PlatformIO IDE.
- USB serial access to the device. Some Windows configurations require a USB driver; install a driver matching the USB interface actually enumerated by your hardware rather than guessing one.

### Step A — install PlatformIO Core

macOS or Linux, in a terminal:

```sh
python3 --version
git --version
python3 -m pip install --user -U platformio
pio --version
```

On Linux distributions with externally managed Python packages, prefer a dedicated Python virtual environment:

```sh
python3 -m venv ~/.venvs/callingcard-pio
source ~/.venvs/callingcard-pio/bin/activate
python -m pip install -U platformio
pio --version
```

On macOS, the same virtual-environment procedure is a reliable alternative if the user install is blocked or `pio` is absent from `PATH`.

Windows PowerShell:

```powershell
py --version
git --version
py -m venv .pio-venv
.\.pio-venv\Scripts\Activate.ps1
python -m pip install -U platformio
pio --version
```

You may instead use the **PlatformIO IDE extension in Visual Studio Code**. The extension manages its own Core installation; use **Project Tasks → cardputer-adv → Build/Upload/Monitor**. Do not mix multiple Python environments without knowing which `pio` is invoked.

## 2. Download the firmware

```sh
git clone https://github.com/h4rml3ss-actual/Callingcard.git
cd Callingcard
git status
git log -1 --oneline
```

For subsequent updates:

```sh
git pull --ff-only
```

Repository entrypoint is `platformio.ini`; environment name is **`cardputer-adv`** (with a hyphen).

**Source inspection:** Before flashing, review `platformio.ini`, `src/main.cpp`, `src/CapSensors.cpp`, and the project [README](../README.md). Current GitHub-based dependencies are not tagged to fixed revisions; repeatable build outputs are therefore not guaranteed until versions are locked.

## 3. Build without flashing

```sh
pio run -e cardputer-adv
```

Builds download a PlatformIO platform, Arduino core, M5Cardputer, RadioLib and TinyGPSPlus dependencies. On the first run, allow package downloads to complete.

**Expected success:** an output ending with **SUCCESS** for the `cardputer-adv` environment and a firmware image under `.pio/build/cardputer-adv/`.

**If it fails:** capture the **first actual error** (not just the final `FAILED` summary), PlatformIO version and dependency resolutions. No clean build has been independently established for this source. Avoid making multiple simultaneous dependency changes before identifying the first failure.

Useful diagnostics:

```sh
pio --version
pio pkg list -e cardputer-adv
pio run -e cardputer-adv -v
```

The PlatformIO board ID is currently `esp32-s3-devkitc-1` with an 8 MB flash setting. These are configuration candidates, **not proof that the board-specific partition, PSRAM and peripheral settings are correct**. If firmware flashes but cannot boot, investigate the board profile and boot log before proceeding.

## 4. Connect the Cardputer

1. Turn the device off. Securely seat the Cap LoRa-1262 and install the proper antenna.
2. Plug the Cardputer into the computer with a **known-good USB data cable**.
3. Enumerate serial ports:

```sh
pio device list
```

On macOS the port might resemble `/dev/cu.usbmodemXXXX`; on Linux `/dev/ttyACM0` or `/dev/ttyUSB0`; on Windows `COM3` or another `COMx`. These are **examples**, not guaranteed ports. Disconnect/reconnect to identify the actual device. Avoid uploading to unrelated serial equipment.

**Linux permissions:** If upload/monitor fails with permission denied, check `ls -l /dev/ttyACM0` (use your actual port) and membership in the appropriate group for your distribution (often `dialout`). Group changes generally require a fresh login. Do not run your whole IDE as root to work around port permissions.

## 5. Flash the board

Only after a successful local build:

```sh
pio run -e cardputer-adv -t upload
```

If auto port selection fails:

```sh
pio run -e cardputer-adv -t upload --upload-port <YOUR_PORT>
```

For example, replace `<YOUR_PORT>` with `/dev/cu.usbmodem1234` or `COM3`, without angle brackets. If the upload tool cannot enter the bootloader, follow M5Stack's recovery/download-mode instructions for the **ADV hardware revision** and try again. Do not assume a single button sequence applies to every revision. If upload baud rate is unreliable, try lowering `upload_speed` locally.

**Recovery principle:** A failed flash is not proof the board is dead. Confirm power, cable, correct port and download mode first; only then consider erase/reflash. An erase will remove current firmware and stored data.

## 6. View serial output

```sh
pio device monitor -b 115200
```

If needed, select the explicit port using `-p <YOUR_PORT>`. Close other applications holding the serial port before upload or monitoring. The on-device display is only a short summary; **USB serial is the primary verification channel**.

Expected *types* of boot and runtime messages, assuming code initializes successfully:

```text
Callingcard PR5 event pipeline boot
CAP SX1262 init status=0
CYCLE mode=wifi count=... duration_ms=... heap=...
CYCLE mode=ble unique=... callbacks=... duration_ms=...
CAP GPS bytes=... location=NO_FIX sats=... lora_ready=... packets=...
PIPE queue=... emitted=... dropped=... heap=...
{"schema":"callingcard.event.v0", ...}
```

Lines may appear in a different order; these are examples and not a recorded hardware test. A nonzero SX1262 status indicates a problem to investigate. The GNSS `NO_FIX` state indoors can be normal. Serial JSONL may be mixed with human-readable status messages; consumers must filter lines before parsing JSON.

## 7. What each feature should do

### Wi-Fi
The current scanner performs a periodic **2.4 GHz access-point survey** and shows a limited sample of network names and RSSI. It is not monitor-mode packet capture and not exhaustive. The API can send probe requests; **do not represent it as fully passive**.

### BLE
After a Wi-Fi window and pause, a finite **passive BLE advertisement scan** runs. Named and unnamed advertisers may appear. Some devices do not advertise continuously, and Bluetooth privacy features can rotate addresses. The BLE count and callback count have different meanings.

### GNSS
The Cap GNSS UART parser reports received characters, satellites and valid/fresh location state. With a clear sky view, it may obtain a fix. If `bytes` stays at zero, check UART pin mapping, baud and Cap installation. **The JSONL observation stream does not yet include GNSS coordinates or UTC timestamps**, even if the Cap reports a fix.

### LoRa
The SX1262 is initialized in a **receive-only 915 MHz test profile**. Packet reception depends on exact matching modulation/settings and presence of a known source; arbitrary LoRa signals will not automatically yield packets. The radio does not transmit, decode Meshtastic, or perform a general RF sweep. Check region and radio parameters before future transmission support.

### Event pipeline
Up to **32 observation records** are buffered in memory; at most eight are drained per call through USB serial as JSON Lines with schema `callingcard.event.v0`. The display shows selected recent samples; queue drop count is diagnostic. Sequence IDs start over after reboot. Neither SD persistence nor durable logging is enabled.

### Blade 2 HUD
**Not implemented.** There is no Android APK to install, pairing sequence or BLE GATT service in this release. Connecting the Blade 2 is a future development milestone; do not hunt for nonexistent pairing instructions.

### SquachWatch
**Not included.** No SquachWatch signatures, vendor classification or threat-scoring features are enabled by this firmware. It remains an upstream research candidate requiring a compatibility and licensing decision.

## 8. First hardware acceptance run

Record this information in a GitHub issue or developer notes:
- OS and version, `pio --version`, firmware Git SHA.
- Exact hardware variant/revision, whether Cap is installed and antenna type.
- Build success/failure with first error if failed.
- Serial boot logs (redact identifying SSIDs or coordinates).
- Wi-Fi scan counts/durations and BLE scan counts/durations.
- GNSS bytes, fix status outdoors, SX1262 initialization status.
- Heap at startup and after **30 minutes**; resets and any queue drops.
- Packet reception only if you have a known compatible LoRa transmitter.
- Power/battery runtime measured under actual scan conditions.

**Pass criteria:** reproducible clean build, stable boot, correct display, regular Wi-Fi and BLE windows, understandable Cap errors/status, memory stability for 30 minutes and valid JSONL syntax. A valid GNSS fix and LoRa packet RX should be verified separately under conditions where those signals are actually available.

## 9. Troubleshooting

| Symptom | Likely causes / actions |
| --- | --- |
| `pio: command not found` | Activate your venv or install PlatformIO Core; check `PATH`. |
| Git or dependency download fails | Check network, Git availability, GitHub access and Python/PlatformIO version. |
| C++ compiler error | Inspect first error; capture exact package versions; M5Stack/RadioLib APIs can change because the git refs are unpinned. |
| No USB serial port | Replace charge-only cable, try another port, inspect OS device list/driver. |
| Upload permission denied | Inspect Linux serial group/permissions or Windows port ownership; close other monitors. |
| Upload hangs/timeouts | Confirm port, cable, power; use ADV-specific bootloader recovery; lower upload baud. |
| Flash succeeds, blank display | Inspect UART boot log, board target/power/display initialization; verify this is an ADV. |
| Wi-Fi count always zero | Ensure a nearby 2.4 GHz AP, not only 5 GHz; check boot and Wi-Fi scan errors. |
| BLE count always zero | Test with a known advertising peripheral; BLE names may be absent. |
| GNSS `NO_FIX` | Check increasing GPS character count, outdoor sky view and allowed fix time; indoor no-fix is common. |
| GNSS bytes always zero | Check Cap seating, UART RX/TX mapping, firmware baud and board initialization. |
| SX1262 init nonzero | Inspect Cap antenna/fit, I²C expander P0, SPI mapping and driver API/CS pin state. |
| No LoRa RX events | Match frequency, bandwidth, spreading factor, coding rate and sync word to a controlled transmitter. |
| Queue drops increase | Logging sink may be slow; serial drains are bounded; inspect counts, memory and scan cadence. |
| JSON parsing fails | Ignore status text lines; only parse lines beginning with `{` and validate complete JSON records. |
| No microSD files | Expected: persistent logging is not implemented yet. |
| Blade can't pair | Expected: BLE telemetry service and HUD app are not implemented yet. |

## 10. Development and upstream safety

Read `docs/V1_ARCHITECTURE.md`, `docs/WIFI_BLE_BRINGUP.md`, `docs/CAP_1262_BRINGUP.md`, and `docs/EVENT_PIPELINE.md`. Prior PR-specific notes may discuss an earlier firmware stage; this guide describes the **merged PR5** feature set. Avoid claims of continuous RF visibility; Wi-Fi and BLE scans intentionally alternate with blind spots.

If experimenting with SquachWatch, clone it to a **different directory**, inspect its own `LICENSE` / build guide, and verify its ADV build against its current code. Do not overwrite Callingcard source with it. Any future integration of GPL code requires appropriate compliance.

## 11. Next implementation steps

1. Perform a **real compile and 30-minute ADV hardware test**; correct toolchain/hardware API problems.
2. Establish safe SPI bus arbitration and explicit chip-select handling for SX1262 and microSD.
3. Add session-based SD JSONL logs, bounded retention and recoverable failure behavior.
4. Add timestamped GNSS association and LoRa receive metadata to the normalized event format.
5. Implement a custom BLE GATT service and Vuzix Blade 2 Android client.

**Reporting:** When troubleshooting, share the **first compiler error**, `pio --version`, hardware revision, `git rev-parse HEAD`, and a redacted serial log rather than only a screenshot of the last line.
