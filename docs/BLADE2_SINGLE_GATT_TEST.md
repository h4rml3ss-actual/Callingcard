# PR 51 — Single-owner Blade Android GATT lifecycle

## Cause under investigation
Previous Android logs showed two simultaneous `BluetoothGatt` clients, IDs 6 and 7, connected to the same Cardputer before one `createBond()` request. A 30-second bond timeout followed. This PR targets duplicate-client creation and callback races; it is not proof that they caused the timeout.

## Behavior
- Process BLE scan results and GATT connection-state callbacks on the main looper.
- Reserve the single `gatt` field as soon as `connectGatt` returns. Never close it and replace it from a second scan result.
- Only one bond attempt per connection; the bond-state receiver no longer initiates its own GATT connection.
- Reject callbacks from stale GATT instances. Only the active, connected, bonded GATT starts MTU discovery and notification setup.
- Keep secure GATT, BLE SMP/MITM configuration and the Cardputer firmware unchanged.

## Reproduction and validation
1. Merge PR #51, rebuild the Blade APK with `cd blade2 && ./gradlew :app:assembleDebug`, and install **that** APK.
2. Keep the existing `cardputer-blade-secure-quiet` firmware on the Cardputer.
3. Force-stop and relaunch the Blade app and collect `adb logcat -v time -s CallingcardBlade BluetoothGatt`. Observe a fresh pairing attempt.
4. Expected: exactly one new `onClientRegistered()` and one `GATT connect_attempt=1`, one `SMP bond_request=1`, and one GATT connection-state callback per transition. If it remains unbonded, collect Android bond transitions and Cardputer `BLADE secure_auth`/`fail_reason` (redacting any PIN, addresses, and SSIDs).
5. If successful, confirm `BOND_BONDED`, `secure_auth=1`, encrypted notification delivery and reconnect without PIN.
6. Verify no second GATT registration on repeated advertisements, disconnect or failed pairing, and that permissions prompts still work.

The repair has not been compiled on a local Android SDK or hardware-tested by the assistant.
