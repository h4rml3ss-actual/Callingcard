# Blade secure pairing diagnostics

Use the opt-in `cardputer-blade-secure` firmware profile. This change only improves UART/USB serial logs for ESP32 BLE authentication; it does not relax permissions, silently confirm pairing, or alter Wi-Fi/BLE survey behavior.

## Build and observe

```sh
pio run -e cardputer-blade-secure
pio run -e cardputer-blade-secure -t upload
pio device monitor -e cardputer-blade-secure
```

Launch the Blade app and attempt enrollment once. Inspect lines beginning `BLADE AUTH` and `BLADE secure_auth=`:

- `event=security_request`: ESP32 received a security request.
- `event=passkey_request`: ESP32 pairing stack requested the configured passkey.
- `event=passkey_notify`: stack asked to display a passkey (printed separately by the existing temporary serial PIN reminder).
- `event=numeric_comparison rejected_by_policy`: pairing tried numeric comparison, which the current callback refuses rather than bypassing MITM.
- `success`, `mode`, `fail_reason`: the ESP32 authentication-complete event, including the underlying failure code. `secure_auth=1` requires successful MITM authentication.

If authentication fails, capture the event sequence and `fail_reason`, plus Android bond state transitions. Redact Bluetooth addresses, Wi-Fi SSIDs, any pairing PIN, device keys, and other personal network identifiers. Do not publicly share full raw UART logs.

The presence of an auth callback is not proof that bonding succeeded. Do not use a nonsecure firmware profile as a security workaround. The current serial PIN reminder is for local development and should be removed or hidden behind a diagnostic build flag later.

**Build and hardware behavior need verification.**
