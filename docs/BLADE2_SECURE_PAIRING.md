# Blade 2 secure pairing and reusable connection (PR #41)

## Goal and trust model

**First-time enrollment requires a human-confirmed six-digit PIN. Subsequent sessions reconnect automatically using persistent Android/ESP32 BLE bond keys.** There is no shared default password in the repository. This is an **opt-in experimental firmware profile** and the existing `cardputer-blade-details` profile remains the tested rollback.

Security uses ESP32 Bluedroid LE Secure Connections + MITM + bonding, and makes status, alert and detailed-observation GATT characteristic reads and notification CCCD writes require MITM-authenticated encryption. Before authenticated pairing, **no application telemetry notifications are sent**. This is designed to fail closed; verify behavior on the actual ESP32-S3 and Blade 2.

## First enrollment

1. Connect Cardputer ADV by USB to the development Mac; open a serial monitor and keep Cardputer physically nearby in a trusted location.
2. Build/flash:
   ```sh
   pio run -e cardputer-blade-secure
   pio run -e cardputer-blade-secure -t upload
   pio device monitor -e cardputer-blade-secure
   ```
3. The startup log prints `BLADE PAIRING CODE NNNNNN`. The code is freshly generated at each Cardputer boot and is *only for initial bond setup*.
4. Rebuild/reinstall the `blade2/` Android APK in Android Studio. Start the app with Android Bluetooth enabled.
5. The Blade app discovers Callingcard by its existing GATT service/name, then invokes Android `createBond()` **only if the system does not already report a bonded device**. The **Android system pairing UI** should request a PIN. Enter the code printed in serial; never blindly approve unmatched passkeys.
6. After `BOND_BONDED`, Android automatically connects GATT and subscribes to the same status/alert/detail characteristics; the app should show all existing scanner pages. The Cardputer logs `BLADE secure_auth=1` on an authenticated connection.

Do not expect a fully unattended *first* pairing. A verified first trust decision is required. Android pairing dialog usability on Vuzix Blade 2 is a hardware validation item, not yet proven.

## Subsequent use

Turn both devices off and on again. Leave the bonded device saved. The Blade should discover Callingcard and reconnect without requesting a PIN; the stored keys protect traffic. The Cardputer will generate a new code at boot for *new* enrollment requests, but existing bond keys should be reused automatically by the Bluetooth stacks.

This implementation uses Android's system bond store, not hard-coded MAC addresses or an app-maintained secrets file. Repeated discovery is retained as a fallback; scan filters are not pinned to an address that could change under BLE privacy.

## Revoke / recover

- **Forget on Blade:** In Android Bluetooth settings, forget/unpair Callingcard. The next app connection should initiate a fresh bond.
- **Reset Cardputer trusted bonds:** Make a backup of any needed internal device settings, then use `pio run -e cardputer-blade-secure -t erase` while connected by USB, followed by a firmware upload. **WARNING:** full flash erase deletes *all* onboard flash data/configuration, not just BLE bonds; files on the separate microSD are not erased by this command. There is not yet a selective in-app bond reset UI.
- **Always reset both sides** when intentionally rotating trust, and re-enroll together. A one-sided reset may leave stale keys and cause repeated authentication failures.
- **Rollback:** Reflash the validated `cardputer-blade-details` profile and install prior APK if secure handshake breaks the normal survey. **Important:** the legacy details profile permits unauthenticated GATT access; rollback removes new BLE protections.

## Verification / failure tests

1. From an *unpaired* BLE test device, attempt to read status/detail/alert and subscribe to notifications: it must not receive data unless authenticated with the legitimate PIN. Check the CCCD writes fail before authentication.
2. Observe successful PIN enrollment, then power-cycle both devices and confirm automatic trusted reconnect **without repeating enrollment**.
3. Re-run Wi-Fi and BLE surveys during the connected session; verify coexistence, LoRa RX, RSSI trends, touchpad interactions and microSD recording still work.
4. Test incorrect PIN, rejected pairing, disconnect during pairing and revoked trust; no detailed data should leak, and UI should show a recoverable state.
5. Check Android logcat `CallingcardBlade` for `bond state=` and ESP32 serial for `BLADE secure_auth=`. Never publish live PINs or Bluetooth bond keys in logs/issues.
6. Confirm Cardputer on-screen status and serial performance after reconnect and BLE survey windows.

## Constraints

This is an experimental **stack-level security change**, not a complete device authorization management system. Bonding may allow multiple peers unless the ESP32 bond store is restricted, and knowledge of a PIN during enrollment can add a peer. Bonding alone does not define an administrative allowlist. For strong single-owner enforcement, next implement a provisioning window, known-bond allowlist, and selective bond removal with a visible physical confirmation on Cardputer.

Do not transmit persistent MAC addresses, BSSIDs, coordinates or other enriched identifiers until the actual GATT permission behavior and MITM pairing have been validated.

No builds or hardware tests were run by the assistant.
