# PR65 — BLE advertisement classification (conservative)

## Changes
- At callback time, snapshot advertised **name**, manufacturer **company ID** (first two bytes, little-endian) when present, and one advertised **service UUID**. These are copied into the existing fixed-size BLE prequeue. No unbounded payload or dynamic storage added to `Observation`.
- On the **main loop**, after dequeue, call `classifyBle()`. Do not run classification or Serial output from BLE callback context.
- Name-only candidate heuristics: `Tile` and `SmartTag` for tracker; `AXON` for Axon; `GoPro` for camera. All are **LOW confidence** and can be trivially spoofed. They do not prove camera/tracker presence or identify an individual.
- Apple company ID (0x004C), Samsung company ID, and generic UUIDs are **not** used as device-type matches. In particular, **AirTag cannot be reliably identified from Apple manufacturer ID alone**. `FLOCK`, `ALPR`, `META`, and `SKIMMER` BLE matches are not implemented pending independently verified, device-specific packet formats.
- Write concise candidate events to **USB Serial only**; no raw BLE address, company bytes, service payload, or location enters unencrypted Blade telemetry. Existing status/alert payloads and scan cadence unchanged.

## Host tests
```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/ble_signatures_test.cpp -o /tmp/callingcard-ble-signatures
/tmp/callingcard-ble-signatures
pio run -e cardputer-blade-details
```

Optional runtime: `pio device monitor -e cardputer-blade-details`, look for `DETECT candidate=TRACKER confidence=LOW`, `DETECT candidate=AXON confidence=LOW`, etc. Ordinary BLE observations still work and should not be promoted to named alerts on Blade yet.

## Caveats
Manufacturer data is heterogeneous; only the assigned two-byte company ID is decoded. Service UUID capture is limited to the first UUID exposed by the Arduino API; more robust parsing and device-specific evidence will be required in a later PR. BLE addresses can rotate, fields can be absent, and names can be spoofed. The classification is diagnostic, not a statement that police hardware or a tracking device is nearby.

This PR has not been built on the Cardputer by the assistant.
