# Pairing-safe BLE coexistence (PR #47)

## Change

The **secure Cardputer profile only** now suspends its passive BLE survey scanning while the Blade GATT link is unauthenticated or pairing. This includes the connected-but-not-bonded phase seen in the earlier logs. Wi-Fi survey, LoRa receive, GPS, display and main loop remain operational. Once the firmware security callback reports MITM authentication success, BLE surveys resume after a 30-second settling delay, then use the original short 2-second / 30-second coexistence cadence.

If authentication drops while a BLE survey is already active, the survey is stopped and its pending sample queue reset. The normal `cardputer-blade-details` firmware profile is not changed.

New logs:
- `BLADE COEX auth=1 ble_survey=enabled`: authenticated and post-auth survey scheduling begins
- `BLADE COEX auth=0 ble_survey=suspended`: lost auth; don't start BLE scans
- `BLADE COEX ble_survey_cancelled=1 reason=not_authenticated`: active scan stopped to protect pairing

## Verification

1. Merge PR 47, pull main, and upload `pio run -e cardputer-blade-secure -t upload`. No new Blade APK is necessary.
2. Before connecting the Blade, confirm `ble_scans=0` and Wi-Fi/LoRa/GPS are still active.
3. Connect and start pairing once. While `secure_auth=0`, confirm `ble_scans` remains unchanged even if the GATT connection is established. Capture `BLADE AUTH` events and `fail_reason` if pairing still fails.
4. On `secure_auth=1`, wait at least 30 seconds and confirm `BLADE COEX scan_start` and BLE observations resume with the usual cadence.
5. Disconnect and reconnect a bonded Blade; confirm survey suspension/restart and no repeat PIN prompt.
6. Recheck telemetry notifications, scanner list/detail, LoRa RSSI/SNR and SD recording, noting any independent SD errors.

### Important

This is a controlled isolation test, **not a confirmed repair** for `fail_reason=99`. If pairing still times out while BLE survey scanning is suspended, investigate the negotiated pairing mode and ESP32 security configuration, particularly `onConfirmPIN()`. Do not remove MITM, suppress security errors or blindly accept numeric comparison.

Current PIN serial reminders expose enrollment codes to anyone with serial access. Do not share unredacted logs. This patch has not been compiled or hardware-tested by the assistant.
