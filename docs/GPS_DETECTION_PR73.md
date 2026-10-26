# PR73 — GPS-correlated detection events (phase 1)

## Scope
Persist **candidate classifications in the same observation snapshot** as GNSS time and location, in the existing on-device JSONL session and USB Serial event stream. This is the prerequisite for trustworthy map export.

- Wi-Fi: on the main loop, classify the SSID/BSSID and add type/confidence/rule to the **already GNSS-annotated** observation before enqueuing.
- BLE: the callback already copies only a **fresh (<=500 ms)** GNSS anchor; classification is assigned in main-loop prequeue drain to that same snapshot. No call to live GPS during BLE callback or late location correction.
- Event format: add an optional `detection` JSON object with `type`, `confidence`, `rule` alongside existing `utc`, `position`, `uptime_ms`, `channel` and `rssi_dbm`.
- A missing/stale GPS fix leaves `position:null`. A missing/stale GPS time leaves `utc:null`. Do **not** populate with latest location when classifying later.
- When observation is not classified, `detection` is omitted, preserving normal JSONL record shape.

Example **illustrative** (not a measured detection):
```json
{"schema":"callingcard.event.v0","seq":5,"kind":"observation","source":"wifi","uptime_ms":49000,"rssi_dbm":-62,"channel":6,"label":"flock-test","utc":"2026-10-26T20:00:00Z","detection":{"type":"FLOCK","confidence":"LOW","rule":"flock-ssid-prefix"},"position":{"lat":42.0,"lon":-71.0}}
```

This PR intentionally does **not** extend Blade wire messages yet: keeping the working unencrypted GATT/Android alert contract stable while validating storage and observation-to-fix association. A follow-up can design versioned BLE detection-location packets that respect negotiated MTU and include an unambiguous correlation identifier, avoiding a location being matched to the wrong alert. Existing BLE notifications, card UI, scan scheduling and security are unchanged.

## Validation
```sh
pio run -e cardputer-blade-details
pio run -e cardputer-blade-details -t upload
pio device monitor -e cardputer-blade-details
```
1. Generate controlled `flock-test` SSID, watch emitted Wi-Fi candidate JSONL for `detection` and same-observation `utc`/`position`.
2. Validate with an actual fresh GNSS fix outdoors; `gps_fix=0` correctly produces `position:null`.
3. Inject a simulated BLE name such as `Tile Pro`; confirm it is classified only after queue dequeue and keeps its callback-time GNSS anchor.
4. Verify unclassified observations remain unchanged, JSONL remains parseable, and Blade status/detail/alert paths continue working.
5. Watch SD flush behavior and `PIPE ...` dropped counters: larger records may encounter the existing 512-byte fixed-line cap.

## Boundaries
This is **candidate evidence for field surveys**, not proof of a physical Flock/ALPR installation. Coordinates and labels are already present in SD JSONL event records where valid. Sensitive-data minimization on unencrypted BLE is no longer a blocking roadmap requirement, but future format changes should still avoid spoofing/accidental misassociation.

Compilation and on-device testing pending.
