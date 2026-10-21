# PR 19 — Asynchronous survey state machine

Wi-Fi and BLE discovery now use **asynchronous scan launches**, with scan completion handled by the regular Arduino main loop. There are no blocking `WiFi.scanNetworks(false,...)` or `BLEScan::start(seconds,false)` calls.

## Architecture

- Survey state machine: `Idle -> Wifi/Ble -> Idle` with a 2.5-second idle delay between surveys and alternating Wi-Fi/BLE rounds.
- **Wi-Fi:** `WiFi.scanNetworks(true,true)` and `WiFi.scanComplete()`. Results collected on the main loop after scan completion, then discarded.
- **BLE:** `BLEScan::start(4, bleComplete, false)` with a scan completion callback. The per-advertisement callback copies at most five bounded metadata observations into a fixed array under an ESP32 critical section; callback never touches Serial, display, SD, GNSS or the EventPipeline. The completion callback only sets a guarded completion flag/count; main loop moves the snapshot into the observation queue and clears scan results.
- **Main loop:** keyboard controls, GNSS UART, LoRa interrupt processing, JSONL draining, SD flushing and dashboard rendering remain serviced while scans are running. The event format remains unchanged.
- **Diagnostics:** `CYCLE mode=wifi/ble ... duration_ms ... timeouts ...`; BLE also reports how many additional observations were discarded by the bounded sample buffer. Wi-Fi has a 12s timeout; BLE 9s.

## Constraints

Radio discovery is **asynchronous**, not continuous. Bluetooth/Wi-Fi radio coexistence remains platform-managed and scans are still alternating (not simultaneous). Only five events per scan are emitted as in PR #18; additional BLE advertisements contribute to `overflow` instead of exhausting memory. Wi-Fi and BLE hardware may still have coexistence delays. Callback happens in BLE stack context and uses a critical section; no callbacks enqueue events directly.

GNSS time/location annotations for BLE are currently applied when BLE scan completion is processed, whereas each BLE `uptime_ms` is captured when the callback fired. This can cause position/time association to lag an observation by up to the BLE survey window; a future improvement should capture GNSS snapshots synchronized with observation time.

## Physical acceptance tests

1. Run `pio run -e cardputer-adv` and upload. Confirm continuous `CAP GPS` sentence parsing and correct GNSS tagging, especially during BLE windows.
2. Observe `CYCLE` lines for both Wi-Fi and BLE; record `duration_ms` and ensure they alternate.
3. Press P/S/R while a BLE scan is underway; check keyboard responsiveness and SD session transitions.
4. Verify LoRa RX still reports packets while Wi-Fi/BLE scans run and JSONL/SD entries remain valid.
5. Check `PIPE dropped`, `overflow`, `timeouts`, SD errors, heap and dashboard responsiveness for at least 30 minutes.
6. Test without SD and with no GNSS fix (events must still flow with null UTC/position).

**Status:** The assistant committed source code but could not run the ESP32 PlatformIO toolchain or test physical hardware. BLE callback and Wi-Fi async APIs should be verified on your exact installed ESP32 Arduino/BLE versions during the first build.
