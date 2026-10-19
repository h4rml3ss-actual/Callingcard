# PR 3 — Alternating Wi-Fi / BLE Scanning

## Objective
Extend the PR2 Cardputer ADV smoke test to discover nearby BLE advertisements **without promising simultaneous, lossless Wi-Fi and BLE capture**. The ESP32-S3 uses shared 2.4 GHz radio resources.

## Behavior
1. Initialize the Cardputer LCD and the Arduino ESP32 Wi-Fi/BLE stacks.
2. Perform a blocking 2.4 GHz Wi-Fi survey; display up to five access points.
3. Pause 2.5 seconds.
4. Perform a **passive** four-second BLE advertisement scan; display up to five device names (or `<unnamed BLE>`).
5. Pause 2.5 seconds, then repeat.
6. Print observation samples and per-cycle duration/count/heap diagnostics to serial.

This is **alternating collection**, not true concurrency. During each BLE survey Wi-Fi observation is paused, and vice versa. BLE's unique result count may not equal raw advertisement callback count. The code avoids retaining stable identifiers such as BLE addresses by default. The on-screen lists are short samples, not exhaustive event histories.

## Test
```sh
pio run -e cardputer-adv
pio run -e cardputer-adv -t upload
pio device monitor -b 115200
```

In the serial monitor look for `Callingcard PR3`, `OBS`, and alternating `CYCLE mode=wifi` / `CYCLE mode=ble`. Record hardware revision, framework version, successful and failed scans, 30-minute heap trends, reset cause, scan durations and actual battery life. Confirm BLE entries appear when a known nearby BLE advertiser is active; many devices randomize addresses and may not advertise names.

### Review/acceptance criteria
- [ ] Compiles on the chosen PlatformIO/Arduino-ESP32 toolchain.
- [ ] Uploads and boots on **Cardputer ADV**.
- [ ] Wi-Fi and BLE survey modes alternate for >= 30 minutes.
- [ ] No crashes or unchecked growth of free-heap consumption.
- [ ] Scan count, duration and observation output are understandable.
- [ ] No pairing, connection, or transmission to surrounding BLE devices occurs during passive BLE scan.
- [ ] No microSD writing or Vuzix pairing yet.

## Engineering cautions
- BLE library API signatures depend on Arduino-ESP32 core version. This branch is targeted to the existing platform version and **has not been compiled or hardware tested here**.
- ESP32 Wi-Fi scans are not completely passive over the air by default; the current Wi-Fi survey can transmit probe requests. In environments needing strictly passive wireless observation, replace with an explicitly reviewed passive capture strategy.
- Never infer continuous coverage or absence of devices from a missing observation.
- The short strings are intentionally truncated; BLE devices often have no advertised name.
- Use bounded telemetry structures and explicit drop accounting before adding durable logs or the Blade BLE link.
- Do not add SquachWatch code without GPL review.

## Next
Introduce bounded observation buffering and schema v0, then bring up LoRa-1262 GNSS/SX1262. After that, integrate microSD logging and a Vuzix BLE GATT telemetry service.
