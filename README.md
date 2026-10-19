# Callingcard

**Callingcard** is an experimental, Cardputer ADV–first portable wireless observation platform with an optional Vuzix Blade 2 heads-up display.

## Hardware target
- M5Stack Cardputer ADV (ESP32-S3): primary controller, Wi-Fi/BLE discovery, keyboard, display, microSD
- M5Stack Cap LoRa-1262: SX1262 radio and onboard GNSS; exact pin mapping, power and software coexistence to be verified against manufacturer examples
- Vuzix Blade 2: optional Android-based HUD, receiving a compact telemetry feed

The Cardputer **must remain functional without the Blade**. A Raspberry Pi is not required for V1.

## V1 goals
1. Reproducible Cardputer ADV build and hardware smoke tests.
2. Wi-Fi discovery and BLE advertisement observations with bounded in-memory state.
3. Basic GNSS position/fix status; attach coordinates only when a valid fix exists.
4. SX1262 radio bring-up, status and receive diagnostics for compatible LoRa packets (no promise of decoding arbitrary traffic).
5. On-device UI, timestamped microSD event log and a consistent event schema.
6. Optional BLE connection to a minimal Blade 2 HUD for status, counts and alerts.

## Explicit non-goals for V1
- 5 GHz scanning on ESP32-S3
- Precise ranging or geolocation from RSSI alone
- Universal LoRa decoding or turnkey Meshtastic interoperability
- Continuous lossless Wi-Fi/BLE monitoring while maintaining BLE telemetry
- Android unrestricted Wi-Fi monitor-mode capture
- Passive collection of unnecessary packet payloads or personal data

## Architecture

```text
Cardputer ADV
  Wi-Fi / BLE observers --> Normalized events --> Ring buffer --> microSD
                                              \--> On-device UI
                                              \--> BLE telemetry --> Blade 2 Android HUD
  Cap LoRa-1262 --------> GNSS + SX1262 events
```

Event transport is versioned and independent of the UI. Telemetry can be dropped/retried without blocking local scanning; storage policy and overflow counters must be explicit. Only use the GNSS timestamp when valid; distinguish unknown time/fix. Scanning and BLE connection contend for ESP32-S3 radio time and require measurements.

## Upstream research
[SquachWatch-CYD](https://github.com/skizzophrenic/SquachWatch-CYD) is an *evaluation candidate*, not yet imported into this repository. Determine supported Cardputer ADV build target, pin conflicts, RAM pressure and GPL-3.0 implications before choosing between a compliant fork/integration or independent implementation. Do not copy upstream source before making that decision.

## First milestone — hardware and baseline
- Verify manufacturer Cardputer ADV and Cap LoRa-1262 pin maps and interfaces.
- Verify exact current SquachWatch build instructions and ADV support against the upstream source.
- Smoke-test display/keyboard/microSD, Wi-Fi discovery and BLE discovery.
- Smoke-test GNSS outdoors and SX1262 reception with the correct antenna attached.
- Record free heap, scan rate, drop counts and battery runtime; test radio coexistence.
- Keep a known-good firmware recovery path.

## Development workflow
Use focused feature branches and reviewed pull requests. Start with architecture and interface contracts; add firmware and Android builds only once upstream versions and hardware-specific drivers are verified.

## Responsible use
Only scan or log where authorized and where lawful. Prefer broadcast metadata and minimal retention, avoid capturing content, and treat vendor/signature classifications as uncertain hypotheses rather than definitive device identifications.

## Status
Planning and verification. **No functional firmware or HUD application has been implemented in this repository yet.**
