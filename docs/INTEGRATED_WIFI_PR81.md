# PR81 — Integrated active and passive Wi-Fi survey

Experimental build `cardputer-blade-wifi-integrated` combines **normal active AP scans** (SSID/BSSID signatures), **passive management frame counters** (beacons, probe requests/responses, deauth, disassoc), and the existing **BLE survey** under one non-overlapping scheduler.

## Cadence

1. Run existing asynchronous `WiFi.scanNetworks()`, parse BSSID/SSID observations, and publish detections to SD/Blade normally.
2. Mark passive phase pending after Wi-Fi scan ends and after `WiFi.scanDelete()` has released scan results.
3. On next scheduled phase, turn on promiscuous management-filter capture for channels **1, 6, 11**, dwelling **400 ms** per channel. Frame callback only increments atomic counters; no payload storage, injection or BLE APIs in callback.
4. Disable promiscuous mode before normal BLE survey slot. The existing `CALLINGCARD_BLADE_COEXIST` rules still skip BLE scanning while disconnected and limit BLE scanning to the configured cadence.
5. Repeat from active Wi-Fi.

The opt-in integrated feature uses the same radio *serially*, never attempts active and promiscuous scanning at the same time. The management window yields counts, **not identified APs or device signatures**. Passive-only `cardputer-blade-passive-wifi` and regular `cardputer-blade-evidence-v3` remain separate.

## Test commands
```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/integrated_wifi_schedule_test.cpp -o /tmp/callingcard-wifi-integrated
/tmp/callingcard-wifi-integrated
pio run -e cardputer-blade-wifi-integrated
pio run -e cardputer-blade-wifi-integrated -t upload
pio device monitor -e cardputer-blade-wifi-integrated
```

Expected USB log sequence:
- `CYCLE mode=wifi count=...`
- `PASSIVE window=... beacon=... probe_req=... ...`
- Connected Blade: `BLADE COEX scan_start=...` and `CYCLE mode=ble ...` when the BLE scan cooldown permits.

**Important:** Since BLE survey remains gated by connection and a 30-second limit, several integrated Wi-Fi/passive cycles may occur before another BLE scan. Verify that Blade status GATT notifications stay healthy during Wi-Fi passive windows. A `PASSIVE start_failed` event skips that phase and continues to future surveys; it doesn't automatically fall back to simultaneously active/passive capture.

## Rollback
```sh
pio run -e cardputer-blade-evidence-v3 -t upload
```

## Validation criteria
- Verify AP/BSSID signature events still appear in the integrated profile.
- Confirm passive counts on 1/6/11 during Wi-Fi windows, then promiscuous mode shuts off.
- Measure BLE notification delivery, GATT disconnects, free heap and scan-loop time during ten minutes and a longer field session.
- Observe that active scan and promiscuous capture never overlap.
- Ensure `gps_fix=0` continues yielding null positions; GPS tagging isn't modified.
- Scan window is deliberately short (1.2 sec) and cannot observe every event or channel.
- No attack classification inferred from unvalidated deauth counts.

Host scheduling tests are provided, but PlatformIO compilation and physical hardware tests remain pending.
