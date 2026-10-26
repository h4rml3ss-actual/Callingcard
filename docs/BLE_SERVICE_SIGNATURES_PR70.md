# PR70 — BLE device-specific service signatures

Extends PR65's captured first advertised BLE service UUID and company ID with **service-based candidates** without changing BLE scanning, callbacks, GATT telemetry or the secure/unencrypted selection.

## Recognized service UUIDs

| Service | Candidate | Confidence | Provenance |
| --- | --- | --- | --- |
| `0xFD5A` | Tracker / Samsung SmartTag family | Medium | Bluetooth SIG Samsung allocation; SquachWatch detector audit |
| `0xFEED`, `0xFEEC` | Tracker / Tile family | Medium | SquachWatch detector audit |
| `0xFD5F` | Meta camera glasses family | Medium | Bluetooth SIG Meta allocation; SquachWatch detector audit |

Sources:
- https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/index-en.html
- https://github.com/skizzophrenic/SquachWatch-CYD/blob/master/docs/DETECTIONS.md

These are **candidate classifications**, not verified device identities. A service UUID can be spoofed, and service assignment alone does not prove a particular product was observed. The PR63 scoring policy limits a single service indicator to medium confidence.

**Deliberate exclusions:** `0xFEAA` is used by generic Eddystone beacons as well as potential Google Find My network devices; it must not identify trackers on its own. Company ID `0x004C` (Apple) or Meta company IDs alone are also insufficient for AirTag / glasses identification. No unsupported AirTag payload parser, skimmer, or Flock BLE signature has been added.

## Validation

```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/ble_signatures_test.cpp -o /tmp/callingcard-ble-signatures
/tmp/callingcard-ble-signatures
pio run -e cardputer-blade-details
```

Monitor `DETECT candidate=TRACKER confidence=MEDIUM rule=samsung-smarttag-fd5a-service` or analogous Meta/Tile alerts on USB serial and Blade HUD if the transmitter advertises the service.

## Limitations / next work

Current Arduino BLEScan code snapshots **one** advertised service UUID. Devices that announce multiple UUIDs may not match when a different UUID is first. Manufacturer company ID is collected but payload patterns are not yet verified or decoded; do not call company ID alone proof. Follow-up should add bounded full-advertisement AD-element parsing and vetted, tested manufacturer payload signatures. The unencrypted Blade link still carries only category, confidence and RSSI, no raw BLE addresses or locations.

Build and hardware testing pending.
