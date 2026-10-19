# Callingcard V1 — Architecture and Implementation Plan

Status: proposed; design assumptions require hardware validation.

## Product boundary
Callingcard is a standalone Cardputer ADV field tool with optional Vuzix Blade 2 read-only HUD. A lost HUD link must never stop collection, local controls or storage.

## Components
| Layer | Cardputer firmware | Blade Android app |
|---|---|---|
| Observers | Wi-Fi survey, BLE advertisements, SX1262 receive diagnostics, GNSS | none required |
| Core | Event normalization, confidence/evidence, bounded device table, counters | telemetry decode, ephemeral view model |
| Outputs | microSD append-only structured log, built-in LCD | glanceable status, latest detections and alarms |
| Control | keyboard, settings, start/stop modes | connection and display controls only for V1 |

Prefer distinct tasks/queues over blocking radio calls. Measure stack, heap, packet loss and queue saturation. Default to **survey**, not indefinite promiscuous capture.

## Radio coexistence and failure policy
The ESP32-S3 shares 2.4 GHz resources for Wi-Fi and BLE. BLE scanning and an active BLE HUD link may reduce Wi-Fi/BLE observation rates. Implement periodic scans, event batching, queue caps and instrumentation; do not promise concurrent lossless collection. SX1262 adds an independent RF transceiver but still shares MCU cycles and board resources.

- HUD disconnect: continue standalone; reconnect opportunistically.
- Missing GNSS fix: save `position: null`, not stale coordinates.
- Buffer full: count drops; never corrupt persistent logs or freeze inputs.
- microSD missing/full: keep scanner usable and clearly show degraded logging.
- Low memory / watchdog restart: persist diagnostics when feasible and recover safely.
- Battery low: show warning; avoid writes during unsafe shutdown.

## Event contract (draft, v0)
```json
{
  "schema": "callingcard.event.v0",
  "id": 42,
  "kind": "observation",
  "source": "wifi",
  "uptime_ms": 83122,
  "utc": null,
  "time_quality": "unavailable",
  "rssi_dbm": -65,
  "classification": "access_point",
  "classification_confidence": null,
  "evidence": ["beacon_observed"],
  "position": null,
  "attributes": {"channel": 6}
}
```
All values above are illustrative. `id` is monotonic within a boot (not globally unique). Logs also need a boot/session identifier. Do not claim confidence from a vendor OUI alone. Human-readable JSONL may be used on SD; compact versioned binary messages can be introduced for BLE. Stable identifiers, MAC addresses and exact coordinates require data-minimization and retention decisions before enabling default persistence.

## BLE transport spike
Start with a GATT service designed for small status and event notifications; establish actual pairing/permissions and reconnection behavior on Blade 2. Specify service/characteristic UUIDs only when implementation begins. Use notification fragmentation or MTU negotiation, sequence numbers, batching and a maximum transmission budget. Treat transport as best-effort; microSD, not HUD BLE, is the durable log.

## Cap bring-up checklist
1. Inspect exact official Cardputer ADV and LoRa-1262 cap schematics/example code. **Do not rely on conversational pin tables without verification.**
2. Check display, keyboard, SD and antenna coexistence and power rails.
3. Validate GNSS UART, location fix and timestamps outdoors.
4. Validate SX1262 SPI/IRQ/busy/reset wiring and regional radio configuration.
5. Never transmit LoRa until applicable regional parameters and legal constraints are validated. Receive-only bring-up first.
6. Record firmware/library versions, hardware revision and test results.

## SquachWatch upstream decision
Before importing code: check current release/tree, ADV board target, flash partition size, keyboard/display implementation, libraries, radio scheduling, logging, and GPL-3.0 obligations. A derived distributed build must honor applicable license terms. Maintain a traceable version/fork strategy if adopted.

## Delivery milestones

### M1 — Standalone baseline
- Verified reproducible build and recovery/flashing instructions.
- Screen, keyboard, battery reading and SD tested on real ADV.
- Wi-Fi survey + BLE advertising scans produce labeled observations.
- Memory and scan-rate diagnostics visible.
**Exit:** 30-minute local-only session without fatal fault; counts/log behavior documented.

### M2 — Cap sensors
- GNSS fix quality, time validity and position available without UI blocking.
- SX1262 SPI probe and receive-only statistics shown.
**Exit:** tests report status for valid/no-fix GNSS and RX/no-RX scenarios.

### M3 — Data/logging
- Event envelope documented and versioned.
- microSD JSONL session logger with bounded queues; clean handling on SD removal/full.
**Exit:** sample logs parse, missing location remains null, overflow accounted for.

### M4 — HUD transport
- BLE GATT telemetry implemented and tested against Blade 2.
- Connection status and dropped-telemetry counters visible locally.
**Exit:** 30-minute connected mixed-scan test including forced disconnect and reconnection.

### M5 — Blade HUD
- Minimal Android app: scanner status, detections, RSSI, time/fix indicators and alerts.
- No dependency on Blade Wi-Fi scan or privileged Android interfaces.
**Exit:** live Cardputer data visible in headset; disconnect state explicitly shown.

## Validation log template
| Test date | Hardware revision | Firmware commit | Test | Result | Measurements/issues |
|---|---|---|---|---|---|
| TBD | TBD | TBD | Standalone 30-minute scan | Pending | |

## Deferred
Meshtastic interoperability, location maps, advanced Wi-Fi capture, crowdsourced observations, cloud sync, cross-device fusion and detailed device tracking.
