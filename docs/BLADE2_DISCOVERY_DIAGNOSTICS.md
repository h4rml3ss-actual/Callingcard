# PR 31 — Blade 2 BLE discovery diagnostics

The previous Blade app stayed on "Searching for Callingcard". This PR separates BLE advertising from BLE surveying to isolate radio coexistence, and changes Blade discovery to an unfiltered diagnostic scan with local name/service matching.

## Test order

1. Merge PR. Start with the **isolated GATT-only diagnostic environment**:

       pio run -e cardputer-blade-gatt-only
       pio run -e cardputer-blade-gatt-only -t upload
       pio device monitor -e cardputer-blade-gatt-only

   Confirm log lines `BLADE init service=...`, `BLADE advertising_started=1`, and `BLADE mode=GATT_ONLY`. This diagnostic profile **intentionally disables only the BLE survey scan**, to help GATT advertising; Wi-Fi, LoRa and GNSS continue. Normal firmware is unchanged. If BLE discovery works here, coexistence needs a later scheduling fix.

2. Rebuild/reinstall the updated Android app from `blade2/` in Android Studio, then restart Callingcard HUD on Blade 2. Use Mac command:

       ~/Library/Android/sdk/platform-tools/adb logcat -s CallingcardBlade BluetoothGatt BluetoothLeScanner AndroidRuntime

   The diagnostic app scans without a UUID filter, but **only connects** when either the advertised local name equals `Callingcard` or the advertised service UUID matches. Search for `scan name=`, `uuidMatch=`, `connection state=`, and `services discovered`. With no matches, use nRF Connect on a second BLE device to inspect the advertisement payload.

3. If connection works in GATT-only, test coexistence with:

       pio run -e cardputer-blade-poc -t upload

   Compare BLE scanning, link stability and performance. If the isolated profile works but the full profile fails, stop and report results; do not ship it as production HUD yet.

4. SD file-size guard: invalid `UINT32_MAX` size is now detected as an error rather than misreported as a normal 4MiB session LIMIT. Examine suspect files on a PC, preserving a copy; this guard does **not repair** existing corrupted files.

## Security and limitations

GATT telemetry remains a **diagnostic, unauthenticated** aggregate-only channel. Nearby BLE clients can read the status; no sensitive data should be added before bonding/authentication. This PR does not fix coexistence; it exposes evidence to decide next design.

The Android app unfiltered scan logs nearby BLE device names and RSSI locally to logcat during discovery. Avoid publicly sharing unredacted full logs. Do not assume nearby device names identify their owners.

This is uncompiled, unverified source until tested with PlatformIO and Android Studio on real devices.
