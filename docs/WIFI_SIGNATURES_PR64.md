# PR64 — Wi-Fi signature capture and conservative candidate classification

Built on PR63's classification core, this PR adds `WiFi.BSSIDstr(i)` to local observation snapshots and calls a header-only Wi-Fi signature classifier in `addObservation`. Candidate output goes to **USB serial only** (no raw BSSID in Blade telemetry), e.g. `DETECT candidate=FLOCK confidence=MEDIUM rule=flock-ieee-b41e52 rssi=-60 channel=6`.

## Initial sourced rules
- **Flock-owned IEEE prefix `B4:1E:52`**: moderate confidence *candidate*, never confirmation of camera identity.
- **SSID beginning with `flock-`** (case-insensitive): weak confidence candidate. Easy to spoof and not definitive.
- A combination stays at **MEDIUM** because the SSID and BSSID can be controlled by the same transmitter.
- Generic Espressif, LiteOn, and other shared chipset OUIs intentionally do **not** identify Flock.
- ALPR, Axon and other signatures remain **not implemented**, pending independent verification. This avoids false positives from stale vendor tables.

Source and caveats: [SquachWatch's signature audit](https://github.com/skizzophrenic/SquachWatch-CYD/blob/master/docs/DETECTIONS.md). Signature facts/prefixes, not code, were used. Do not advertise these as confirmed detection of an ALPR camera.

## Build and tests
```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/wifi_signatures_test.cpp -o /tmp/callingcard-wifi-signatures
/tmp/callingcard-wifi-signatures
pio run -e cardputer-blade-details
```

No changes to Wi-Fi scan scheduling, coexistence, GATT characteristic layout, or Android. The classifier runs once per found Wi-Fi network. The serial diagnostic intentionally does not expose the complete MAC address.

## Limitations
Standard `WiFi.scanNetworks` gets AP beacons/scan results only, not client probe requests or frames from dormant cameras. Some Flock devices won't emit a distinguishable beacon; PR67 is reserved for separately testing promiscuous monitoring. BSSID can be spoofed or randomized, so even an IEEE-registered prefix is insufficient to establish camera model/ownership. The broader detection rules and Blade alerts will follow in later PRs.

**Firmware compile/hardware testing pending.**
