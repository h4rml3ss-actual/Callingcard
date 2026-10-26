# Temporary rollback — unencrypted Blade telemetry

Use this rollback only if you accept that any nearby BLE client may connect, read characteristics, subscribe to notifications, or impersonate a peripheral advertising the same name/service. This is **not** a trusted or encrypted telemetry channel. RF advertisements already being public does **not** make all derived scanner telemetry or labels automatically harmless. Do not send GPS coordinates, passwords, tokens, private device data or sensitive identifiers on this profile.

## Firmware

The existing **cardputer-blade-details** profile is already the original unencrypted full-telemetry mode. It includes Wi-Fi/BLE scanning, LoRa/GPS, aggregate status, alerts and short observation labels. It does **not** require native BLE bonds:

```sh
pio run -e cardputer-blade-details
pio run -e cardputer-blade-details -t upload
pio device monitor -e cardputer-blade-details
```

**Do not flash** `cardputer-nimble-pairing-test`, `cardputer-blade-secure`, `cardputer-blade-secure-quiet`, or `cardputer-cstp-dev` for this rollback: their authentication or protocol requirements do not match the unencrypted Android client.

## Blade

PR61 changes the **standard Callingcard HUD** Android activity to proceed to service discovery immediately after GATT connection, without calling `createBond()`. It also disables application bond/pairing broadcast listeners for this path, while preserving single-owner GATT connection management. Existing CSTP Lab and GATT Auth Lab remain separate development activities.

```sh
cd blade2
./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am force-stop org.callingcard.blade
adb shell am start -n org.callingcard.blade/.MainActivity
adb logcat -v time -s CallingcardBlade BluetoothGatt
```

Expected: `GATT insecure_telemetry=1 no_bond_requested=1`, followed by `GATT discovery=1 require_bond=false`, descriptor subscription and telemetry updates. **No PIN dialog should be needed.** A spontaneous pairing dialog indicates the Cardputer still runs a protected firmware profile, a stale bond/security state or another protected endpoint; don't dismiss this without checking flashed environment.

No bond reset, NVS erase or cleanup of existing secure credentials is performed automatically. The earlier NimBLE/CSTP experiments are retained for future work.

## Deferred work

- Revisit optional secure pairing only when desired.
- Fix the independent SD `INVALID_FILE_SIZE` issue; unencrypted rollback does not resolve it.
- Limit observation detail labels and reduce advertised information if privacy needs change.

**Not compiled or device-tested by the assistant.**
