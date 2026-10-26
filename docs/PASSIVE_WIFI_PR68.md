# PR68 — Opt-in passive Wi-Fi coexistence experiment

This lab-only PlatformIO profile substitutes **short passive 802.11 management-frame sampling** for the normal asynchronous AP scan during the Wi-Fi half of Callingcard's alternating scan schedule. It does **not** replace the stable `cardputer-blade-details` build.

## Usage

```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/passive_wifi_frames_test.cpp -o /tmp/callingcard-passive-test
/tmp/callingcard-passive-test
pio run -e cardputer-blade-passive-wifi
pio run -e cardputer-blade-passive-wifi -t upload
pio device monitor -e cardputer-blade-passive-wifi
```

Run the **regular Blade HUD** already installed from PR66, then watch the serial lines like:

```text
PASSIVE window=1 beacon=52 probe_req=4 probe_resp=9 deauth=0 disassoc=0 connected=1 heap=...
BLADE COEX connected=1 ble_scans=... skipped=... dropped=... prequeue=...
```

Compare with the unchanged baseline:

```sh
pio run -e cardputer-blade-details -t upload
```

## Design and limitations

- ESP32-S3 Wi-Fi STA radio in opt-in promiscuous mode.
- Capture **management frame counters only**, by parsing Frame Control; **no MACs, SSIDs, frame bodies, raw payloads, GPS coordinates, or packet recording**.
- Wi-Fi callback only increments fixed counters. No serial/SD, dynamic allocation, GUI or BLE notifications from the callback.
- Samples channels **1, 6, 11 for 400 ms each**. Channel changes run on the application main loop, after returning from callback. This is *not continuous monitoring*; it may miss short events.
- No deauthentication injection, active probe transmissions or MAC tracking. A nonzero deauth count alone **does not imply an attack**.
- Scan scheduler retains the existing BLE-survey window and GATT connection profile; passive operation may still disrupt BLE coexistence due to shared radio and is not yet validated on hardware.
- In this experimental environment normal Wi-Fi scan results and therefore Wi-Fi SSID/BSSID signature classification **do not run** during passive windows. This is deliberate isolation, not a replacement of PR64. Restore `cardputer-blade-details` for normal Wi-Fi signature checks.
- Counters are USB serial diagnostics only, not classified detections or Blade alerts.
- If channel switch/setup fails, the run logs an error and normal scheduling resumes on the next window; the radio capture is stopped. BLE discovery/alerts are otherwise unchanged.
- **No build or device testing performed by assistant.**

## Acceptance tests

1. Build host test and passive environment cleanly.
2. Boot with Blade disconnected, verify `PASSIVE window=` periodically.
3. Connect Blade; compare BLE connection uptime and status refresh with baseline for 10 minutes.
4. Verify BLE survey activity and telemetry still arrive.
5. Reflash baseline if there are disconnects, stalled BLE scanning, or watchdog resets.
6. Validate counts on a controlled AP under your administration; do not assume environmental frames identify Flock or ALPR hardware.

Follow-on work: per-channel counters and richer, validated classifiers only after coexistence is measured.
