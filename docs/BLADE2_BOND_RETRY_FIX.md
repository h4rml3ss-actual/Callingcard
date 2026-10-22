# PR 43 — Secure BLE pairing loop fix

## Root cause
After PR 41, `MainActivity.discovery.onScanResult()` **still directly invoked** `device.connectGatt(...)`, bypassing the new `createBond()`/bond receiver workflow. This could establish a GATT session before Android and ESP32 finished bonding, while authenticated CCCD writes caused extra pairing prompts. The new branch waits for Android `BOND_BONDED` before connecting and uses `connectPaired()` for both first-time and subsequent connections.

Additional fail-safe: if bonding fails or is declined, the app stops automatic enrollment attempts and requires a manual restart to retry. Normal reconnection after an established bond remains automatic.

## Validate
1. Merge PR 43; pull main and rebuild/reinstall `blade2/` on Blade 2. **No new Cardputer firmware upload required.**
2. Close the Callingcard HUD application on Blade 2. In Blade Android Bluetooth settings, **Forget** the Callingcard peripheral if listed, then restart the app. A stale Android bond may refer to keys that no longer match the Cardputer.
3. If the Cardputer also has stale bonding keys, clear its BLE bond state *selectively* using platform tooling once implemented. **Do not use full flash erase** unless you have backed up configuration and accept losing onboard data.
4. Start the Cardputer's secure firmware/serial monitor. Use the fresh pairing code printed on serial. Pair once on the Blade 2.
5. Confirm ESP32 `BLADE secure_auth=1`, Android `bond state=12` (BOND_BONDED) and normal HUD counters, scanner details, Wi-Fi/BLE scans, and LoRa continue.
6. Power cycle both devices. The second connection should use the existing keys **without a new PIN dialog**.
7. Deliberately reject a new first-time pairing once: the app should show `PAIRING FAILED - restart app to retry`, **not** automatically issue repeated new prompts.

## Debug if pairing still fails
Capture sanitized Cardputer serial lines containing `BLADE secure_auth=`, `BLADE connection_changed=` and Android logcat:
```sh
~/Library/Android/sdk/platform-tools/adb logcat -s CallingcardBlade BluetoothGatt BluetoothManager AndroidRuntime
```
Do not publish six-digit pairing codes, MAC addresses, bond keys or nearby SSIDs in logs.

**Security caveat:** The actual Android and ESP32 BLE encryption/bond persistence on this library version still requires field validation; passing `BOND_BONDED` alone does not prove MITM. Do not revert to an unauthenticated profile while handling sensitive observations.

The source was not built or tested on the user's hardware by the assistant.
