# Callingcard

**Callingcard** is an experimental, standalone wireless observation instrument built around the **M5Stack Cardputer ADV (ESP32-S3)** and **M5Stack Cap LoRa-1262**. A **Vuzix Blade 2 heads-up display** is planned as an optional companion; it is **not yet implemented**. No Raspberry Pi is required.

> **Project status (October 2026):** PRs 1–5 have been merged, but merging code is **not equivalent to building or validating it on hardware**. The current tree is a development prototype; do not assume that it compiles, boots, or operates reliably until you have completed the [setup and verification guide](docs/INSTALL_SETUP.md).

## What works in source vs. what is planned

| Capability | Code in repository | Validated on ADV? |
| --- | --- | --- |
| Cardputer LCD status / serial diagnostics | Implemented candidate | Not confirmed |
| 2.4 GHz Wi-Fi AP survey | Implemented candidate | Not confirmed |
| Passive BLE advertisement scan, alternating with Wi-Fi | Implemented candidate | Not confirmed |
| SX1262 915 MHz receive-only test configuration | Implemented candidate | Not confirmed |
| GNSS UART / fix status | Implemented candidate | Not confirmed |
| 32-entry in-memory observation queue | Implemented candidate | Not confirmed |
| USB serial JSONL observation stream | Implemented candidate | Not confirmed |
| microSD persistent logging | Not implemented | No |
| SquachWatch signature/detection engine | Not integrated | No |
| Blade 2 BLE telemetry and Android HUD | Not implemented | No |
| Meshtastic interoperability | Not implemented | No |

**Important limitations:** The ESP32-S3 only supports 2.4 GHz Wi-Fi. Wi-Fi and BLE surveys **alternate** and have blind spots; the Wi-Fi survey can transmit probe requests. The LoRa receiver is not a universal spectrum analyzer or a Meshtastic decoder. The firmware does not associate GNSS coordinates with individual observations yet. BLE device identities and content are not intentionally retained by the event format.

## Hardware

Required for the current combined prototype:

- M5Stack **Cardputer ADV**, not the original Cardputer
- M5Stack **Cap LoRa-1262**, firmly attached
- Correct LoRa antenna, installed **before power-on**
- Data-capable USB-C cable and a development computer

Optional for now: microSD card (not used by current firmware), USB battery/power supply (take care with power ratings), Vuzix Blade 2 (no app available yet).

The Cap supplies an **SX1262** LoRa transceiver and **GNSS** receiver. The Cap shares SPI wiring with Cardputer peripherals, so SD integration needs separate verification.

## Quick start

Install **Git**, **Python 3**, and **PlatformIO Core** (or VS Code + PlatformIO IDE). See [full installation instructions](docs/INSTALL_SETUP.md) for macOS, Linux, Windows, serial permissions, bootloader recovery and troubleshooting.

```bash
git clone https://github.com/h4rml3ss-actual/Callingcard.git
cd Callingcard
pio --version
pio run -e cardputer-adv
pio device list
pio run -e cardputer-adv -t upload
pio device monitor -b 115200
```

Do **not** flash until the build succeeds and you understand that uploading will overwrite the Cardputer's existing firmware. If your computer exposes multiple serial devices, select the correct port with `--upload-port`. These commands describe the expected workflow; **they are not evidence of a successful verified build**.

After boot, the device should *attempt* alternating Wi-Fi and BLE surveys, GNSS parsing and receive-only LoRa initialization. Look for `CYCLE`, `CAP`, `PIPE` and JSON observations on serial. A GNSS fix may require outdoor sky visibility. LoRa RX packets require a compatible transmitter and matching modem settings.

## Example telemetry

Illustrative JSON Lines emitted through USB serial (not stored to microSD):

```json
{"schema":"callingcard.event.v0","seq":1,"kind":"observation","source":"wifi","uptime_ms":2145,"rssi_dbm":-64,"channel":6,"label":"example-network","utc":null,"position":null}
```

The sequence number resets on reboot. Fields `utc` and `position` are presently `null`, even when GNSS has a fix; observation-to-fix correlation is not implemented. Device lists are sampled/truncated and should not be interpreted as full capture.

## Architecture

```text
Cardputer ADV (standalone)
  Wi-Fi survey ---------\
                         +--> sampled observations --> 32-slot FIFO --> serial JSONL
  Passive BLE scan -----/                              \\--> LCD summary
  LoRa-1262 Cap --> SX1262 receive statistics ---------> serial / LCD status
                 --> GNSS fix/time/location ----------> serial / LCD status

Future: SPI-arbitrated microSD persistence and optional BLE telemetry --> Blade 2 HUD
```

See [V1 architecture](docs/V1_ARCHITECTURE.md), [event pipeline](docs/EVENT_PIPELINE.md), [Cap bring-up](docs/CAP_1262_BRINGUP.md), and [Wi-Fi/BLE scheduling](docs/WIFI_BLE_BRINGUP.md).

## Project layout

```text
platformio.ini             PlatformIO ADV environment and dependencies
src/main.cpp               Scanner scheduler, LCD, serial diagnostics
src/CapSensors.cpp         GNSS UART and SX1262 receive-only bring-up
src/EventPipeline.cpp      Fixed-size observation queue and JSONL serializer
include/CapSensors.h       Cap module interface
include/Observation.h      Wi-Fi / BLE observation model
include/EventPipeline.h    Event queue interface
docs/INSTALL_SETUP.md      Detailed first-time install, flash, and troubleshooting
docs/                       Architecture and milestone notes
```

## Troubleshooting

- **No serial port:** use a *data* USB-C cable, inspect `pio device list`, verify USB permissions/driver, and reconnect.
- **Build fails:** retain the **first** compiler error and full PlatformIO version; library dependencies are referenced by GitHub URL and are not version-pinned yet.
- **Screen blank:** confirm ADV hardware revision/board selection; check the serial boot message before changing wiring.
- **`CAP SX1262 init status` nonzero:** check Cap seating, antenna, I²C expander initialization, SPI pins and RadioLib compatibility.
- **GNSS no fix:** move outside and wait with clear sky; check whether `CAP GPS bytes` increases.
- **No LoRa packets:** default is a narrow receive-only test profile; arbitrary nearby LoRa traffic will not necessarily decode.
- **No BLE entries:** verify a nearby BLE advertiser; many do not broadcast a name.
- **No SD files:** expected! MicroSD logging is **not implemented**.

See the [full troubleshooting matrix](docs/INSTALL_SETUP.md#troubleshooting).

## Roadmap

1. **Validate build and hardware** of merged prototype; stabilize library versions and Cap interfaces.
2. Test Wi-Fi/BLE scan rates, contention, heap and GNSS UART data loss.
3. Add explicit SD/SX1262 SPI arbitration and persistent sessions.
4. Add GNSS timestamp/fix association to events, LoRa event metadata and retention options.
5. Add optional Cardputer-to-Blade BLE telemetry and Blade 2 Android HUD.
6. Evaluate SquachWatch integration separately, including licensing, hardware support and signature quality.

## Security, privacy and legal use

Observe only networks/signals you are authorized to survey and comply with local laws. The tool is intended for nearby **broadcast metadata** and diagnostic status, not collection of private content. Do not infer device ownership or precise location from RSSI or a vendor prefix. No LoRa transmission is enabled in the current implementation. Log data carefully when adding storage: Wi-Fi SSIDs, coordinates and timestamps can still be sensitive.

## Upstream and licensing

[SquachWatch-CYD](https://github.com/skizzophrenic/SquachWatch-CYD) is a potential reference/integration candidate, **not embedded** here. If we import GPL-licensed code in future, review and follow its license requirements. Individual hardware and firmware dependencies have their own licenses; inspect those before redistributing binaries. Project-wide licensing is not asserted here; do not assume a license from this README.

## Contributing / reporting bugs

For reproducible issues, include Cardputer revision, Cap revision, OS, PlatformIO version, build command, **first error**, serial log with sensitive network identifiers redacted, and hardware test steps. Use focused PRs and do not claim hardware validation without measurements.
