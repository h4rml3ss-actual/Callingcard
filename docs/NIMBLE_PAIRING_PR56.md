# PR 56 — Isolated NimBLE native authenticated pairing test

**Purpose:** Determine whether NimBLE-Arduino succeeds at authenticated BLE Secure Connections bonding with the Blade 2 where Bluedroid timed out. **This is only a pairing test, not a full telemetry migration.**

## Scope

- New PlatformIO environment `cardputer-nimble-pairing-test` builds **only** `src/nimble_pairing_test.cpp` using NimBLE-Arduino 2.5.1, excluding `src/main.cpp`.
- No scanner surveys, SD writing, Cap hardware, LoRa, GPS, or CSTP code runs. Normal firmware profiles remain unchanged.
- Advertises as `Callingcard` with the **legacy** Blade service/status UUID, allowing existing Android `MainActivity` to find it.
- Requires bonded, MITM-authenticated, encrypted BLE Secure Connections (`setSecurityAuth(true,true,true)`). Uses DisplayOnly passkey on USB serial; secure status has `READ_ENC | READ_AUTHEN` properties.
- No real collected telemetry; the status value is `1,0,0,0,0,0`. No unauthorized telemetry data. Do not confuse successful GATT discovery with successful MITM authentication.

## Build and test

```sh
pio run -e cardputer-nimble-pairing-test
pio run -e cardputer-nimble-pairing-test -t upload
pio device monitor -e cardputer-nimble-pairing-test
```

Open the existing **Callingcard HUD** app on Blade, **not CSTP (Lab)**. Its BLE scan should find the `Callingcard` device and request a bond. Enter the six-digit passkey shown on Cardputer's USB serial. Do not paste your actual passkey into public logs.

Expected serial diagnostic after successful bond:

```text
NIMBLE auth encrypted=1 mitm=1 bonded=1 accepted=1
```

The HUD may show no live telemetry (by design), and it may report a missing alert/detail characteristic. The sole success criterion is authenticated bonded native Bluetooth security, followed by reconnect without a fresh PIN. Capture:

```sh
adb logcat -c
adb logcat -v time -s CallingcardBlade BluetoothGatt
```

On failure capture `NIMBLE auth...`, disconnect reason, Android `bond state`, and connection status, while redacting device addresses and passkey. If no PIN prompt appears, check that the existing Android app doesn't have a stale failed-bond latch (force-stop/relaunch) and that the firmware appears in BLE scan; don't remove security requirements as a workaround.

**No forced bond deletion or NVS erase is performed.** If stale pairing records are suspected, remove this *test device's* pairing record manually on both devices only with user approval and after confirming it cannot affect other devices.

### Boundaries

- This test does not transmit real sensor data or validate scanner coexistence.
- This test does not include device-visible Cardputer display of passkey, only USB serial output; a future UX should show the PIN locally.
- If NimBLE works, migrate BLE surveying and telemetry in a separate PR and continue enforcing authenticated reads/writes/notifications.
- If NimBLE fails in this minimal setup, investigate Android SMP compatibility and peripheral capabilities before returning to custom application crypto.
- Arduino/PlatformIO version compatibility, compilation and physical pairing **have not been verified by the assistant**.
