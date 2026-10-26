# PR71 — Blade HUD classified detections

Adds a fifth **DETECTIONS** page to the existing Blade 2 HUD. The Cardputer firmware and GATT wire protocol remain unchanged.

## UI
- Navigate left/right among Overview, Wi-Fi, BLE, LoRa, and **Detections**.
- Detections lists the 24 most recently received candidate alerts in memory, newest first. Up/down selects; Center/Enter opens details; Back returns to list.
- Detail view includes candidate category, confidence, RSSI, Wi-Fi/BLE source, and age since alert reception.
- All classifications display **POSSIBLE**: no advertiser name, MAC address, or GPS is present on the detection wire packet.
- No persistence across app restarts. This is an in-memory display history, *not* cross-device or per-device deduplication; repeated candidate notifications may occupy multiple rows. PR72 will address aggregation and persistence.
- Existing generic E event alerts, observation pages, navigation, and telemetry subscriptions remain intact.

## Test
```sh
cd blade2
./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am force-stop org.callingcard.blade
adb shell am start -n org.callingcard.blade/.MainActivity
adb logcat -v time -s CallingcardBlade
```

With the Cardputer running `cardputer-blade-details` firmware including PR66 candidate alert support, trigger a controlled test beacon such as `flock-test`. Navigate right from the Overview to Detections, select the candidate, and inspect the detail view. Do not represent synthetic test SSIDs as actual surveillance hardware.

## Limitations
App-side only; classified detections require PR66 Cardputer firmware and do not appear if no matching candidates occur. Bluetooth connection remains intentionally unencrypted. UI has not been compiled or tested by assistant.
