# PR60 — GATT-triggered BLE authentication experiment

**Isolated test only.** Launch **Callingcard GATT Auth (Lab)** on the Blade, with the Cardputer running `cardputer-nimble-pairing-test` (PR56/57). The standard Callingcard HUD and CSTP client remain unchanged.

Unlike the original HUD, this test **never calls `createBond()`**. It connects to the advertised NimBLE service, discovers the status characteristic, and attempts to read it. The NimBLE characteristic uses encrypted/authenticated read permissions. This tests whether Android's GATT security upgrade initiates native bonding correctly on Vuzix Blade 2.

## Build
```sh
cd blade2
./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb logcat -c
adb logcat -v time -s CallingcardGattAuth BluetoothBondStateMachine bt_smp
```

## Expected logs
- `connect: NO explicit createBond()`
- `protected_read_request=initial bond=10`
- Optional `PAIRING_REQUEST variant=...` when the OS broadcasts it to the app
- `bond_state=11` followed by `bond_state=12` on success, or `bond_state=10` on failure
- `protected_read status=0 bond=12` means the authenticated protected GATT read succeeded; value intentionally not rendered

An initial `protected_read` with insufficient authentication status is **not success**. This test does not automatically retry failed bonding or bypass encryption. Android may complete bonding after the first read returns an authentication error. The activity attempts one additional protected read after `BOND_BONDED`; to avoid two simultaneous operations it skips retry while a read callback remains pending. If a pending operation never completes, force-stop/relaunch for another trial.

**Limits:** The device/system stack may not initiate pairing on read; some Android versions automatically retry internal authentication, while others immediately return ATT errors. The test only demonstrates whether security is established and protected access works, not app data transmission or reliable coexistence. No change to NimBLE/MITM policy, keys, SD, scanner, LoRa or production telemetry. No build/hardware test executed by assistant.

**Security:** PINs and device identifiers should be redacted in uploaded logs. Do not weaken authentication, treat unbonded GATT read status 0 as authenticated, or share credentials.
