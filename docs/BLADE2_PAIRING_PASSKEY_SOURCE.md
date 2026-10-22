# Blade 2 secure pairing passkey source

## Problem
The ESP32 secure profile generated a six-digit startup PIN and periodically reprinted it, but the firmware's `onPassKeyNotify(passkey)` callback **discarded the actual passkey supplied by the Bluedroid pairing stack**. If those codes differ, entering the startup PIN can result in a timeout/failure. This PR makes the active BLE pairing code visible and uses it in subsequent serial reminders during the attempt.

## Test
1. Merge, build and upload `cardputer-blade-secure`. No Blade APK changes.
2. On Blade, forget the old failed Callingcard pairing if present, and force-stop/relaunch the application for one clean attempt.
3. On Cardputer serial watch for `BLADE AUTH event=passkey_notify pairing_code=XXXXXX`. **Enter that actual code**, not an earlier startup PIN if different. The 15-second reminder will also display it as `active pairing code (from BLE stack)`.
4. Confirm `BLADE secure_auth=1`, stable status/details, and repeat connection without a new PIN.
5. If the pairing still fails, compare pairing method and `fail_reason` with a newly captured redacted Android bond-state trace. If `numeric_comparison rejected_by_policy` occurs, the device needs an explicit confirmation UI, not silent approval.
6. Do not share raw pairing codes, MACs or SSIDs in issue comments or PR logs.

Existing MITM GATT requirements, IO capability (display-only), refusal of unconfirmed numeric comparison, BLE scan suppression during pairing and Android connect workflow are unchanged. This is a **targeted code-visibility correction and test**, not proof of the timeout's root cause.

Not compiled or hardware-tested by assistant.
