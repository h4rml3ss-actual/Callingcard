# PR 49 — BLE SMP enrollment isolation

The previous secure firmware returned `fail_reason=99` (0x63, pairing timeout) despite correct PIN-entry UI, active pairing-code display and no Cardputer BLE survey scans. The current Android main branch also began MTU/service discovery before bonding finished.

This PR provides two controlled changes:

1. New **opt-in** `cardputer-blade-secure-quiet` profile. During the interval where Blade has a live GATT link but is not MITM authenticated, **Wi-Fi and BLE surveys are suspended**. Wi-Fi/LoRa receive, GPS and other main-loop services remain configured normally; Wi-Fi surveys resume after disconnect or completed authentication. Existing `cardputer-blade-secure` is unchanged.
2. Android now calls `createBond()` only once after a successful physical GATT connection; waits until `BOND_BONDED` before requesting MTU, discovering services, or writing MITM-protected CCCDs. Previously bonded devices skip PIN entry.

## Validation

- Rebuild and install the changed Blade APK; this PR changes Android too.
- Build/upload `pio run -e cardputer-blade-secure-quiet -t upload` and monitor with `pio device monitor -e cardputer-blade-secure-quiet`.
- Restart the Blade app; if its bond is stale, forget the Callingcard bond in Android Bluetooth settings before relaunching. **Do not wipe ESP32 flash/NVS**.
- During pairing verify `BLADE SMP wifi_surveys_suspended=1 connected=1 authorized=0`, no new BLE survey cycles, and one `SMP bond_request=1` in Android logcat.
- Confirm Android `BOND_BONDED` and ESP `secure_auth=1`; verify HUD notifications, then restart both devices and check reconnection without PIN.
- If failure persists, capture `BLADE AUTH`, `secure_auth`, `fail_reason`, `SMP` and Android `bond state`. Redact pairing codes, MACs and SSIDs. Do not conclude the timeout was radio interference without controlled evidence.
- Compare against the original `cardputer-blade-secure` profile if the test is inconclusive. It remains available for quick firmware rollback.

**No MITM downgrade, numeric-comparison auto-approval or full-flash erase is introduced. The PR was not compiled or hardware-tested by the assistant.**
