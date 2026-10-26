# PR58 — Blade 2 pairing variant diagnostics

The Blade's system Bluetooth service launches `com.vuzix.hud.settings.bluetooth.BladeBluetoothPairingDialog` on `ACTION_PAIRING_REQUEST`. Prior logs show `pinRequestCallback`, then ~30 seconds before bonding drops. This does **not by itself** prove the pairing variant is legacy PIN instead of LE passkey.

## What changed
- Callingcard HUD observes `ACTION_PAIRING_REQUEST` for **only its current target device**.
- Logs `BluetoothDevice.EXTRA_PAIRING_VARIANT` (numeric and symbolic) alongside current connection attempt.
- Logs current/previous bond state and elapsed time for that attempt.
- Does **not** read/log PIN contents, call `setPin`, request privileged permissions, abort pairing broadcasts, change authentication, or replace the Vuzix dialog.
- Existing NimBLE test firmware and CSTP code are unchanged.

## Test
Build and install debug Blade application:

```sh
cd blade2
./gradlew :app:assembleDebug
adb logcat -c
adb logcat -v time -s CallingcardBlade BluetoothBondStateMachine bt_smp bt_btif_dm
```

Run one pairing attempt with the isolated `cardputer-nimble-pairing-test` firmware, using the Cardputer's displayed passkey. Redact device identifiers/passkeys before sharing logs.

Look for:
```text
SMP pairing_request variant=... name=... receiver_observed_only=1
SMP bond state=11 previous=10 ... attempt=1
SMP bond state=10 previous=11 ... elapsed_ms=... attempt=1
```

**Important limitation:** A normal application may not receive `ACTION_PAIRING_REQUEST` on every Android/Vuzix version because platform broadcasts and pairing APIs can be restricted to privileged Bluetooth/system handlers. If `SMP pairing_request` is absent but the Vuzix dialog appears, do not infer that Android never requested pairing. Check system logs/HCI snoop or vendor APIs instead.

No app-level PIN dialog is included; routing an unprivileged dialog into system SMP input without confirmed API access would not fix the exchange. PR58 is diagnosis, not confirmed remediation. Capture `EXTRA_PAIRING_VARIANT` and native stack failure code before deciding between a vendor fix and a new transport. 

**Build and physical testing have not been executed by assistant.**
