# PR 34 — Wearable passive event alerts

**Purpose:** Show short source-only notices on the Blade 2 for passive observation activity, without exposing scanned device names, MAC addresses or coordinates through BLE.

## Protocol
The existing service UUID and status characteristic remain unchanged. An optional READ/NOTIFY characteristic is added:
- `cabb1e00-ccad-4abc-a112-24fc00000003`
- Payload: `E,<sequence 0-999>,<W|B|L>`. W=Wi-Fi observation, B=BLE advertisement, L=LoRa packet reception.
- Events are coalesced to a single pending source and sent at most once per 4 seconds. This is a **low-volume activity cue**, not one alert per device or a lossless event feed. The last displayed alert expires after 8 seconds.
- The alert characteristic is optional. Older firmware still works with the new Android app for status-only display. Older Android apps continue reading status from the original UUID.

## Installation and validation
1. Merge the PR; pull the latest GitHub main branch.
2. Build and flash the new **opt-in** profile:
   ```sh
   pio run -e cardputer-blade-alerts
   pio run -e cardputer-blade-alerts -t upload
   pio device monitor -e cardputer-blade-alerts
   ```
3. Rebuild the Android `blade2/` APK in Android Studio and reinstall on the Blade 2.
4. Connect as usual. Confirm regular Wi-Fi/BLE/LoRa counters still update and brief `ALERT: Wi-Fi observation`, `ALERT: BLE observation`, or `ALERT: LoRa reception` appears when the corresponding passive observation is received.
5. Check serial `BLADE alert=...` messages, SD health and the BLE scan cadence. Move out of range and return to exercise reconnect.
6. Roll back to `cardputer-blade-coexist` firmware if alerts or connectivity destabilize the existing survey path. The original GATT-only profile is also retained.

The alert indicates that an observation occurred, **not** that it was suspicious or a new unique device. No active probing or automatic attack attribution. GATT characteristics remain unauthenticated and should not carry sensitive content.

Code is submitted for on-device build/test; no Android or ESP32 hardware validation was performed by the assistant.
