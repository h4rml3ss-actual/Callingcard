# PR55: Blade CSTP client — development-only acceptance test

**Not production secure.** CSTP service differs from legacy BladeTelemetry. Launch the separate **Callingcard CSTP (Lab)** app icon, not the standard Callingcard HUD. Neither the legacy Android app nor its BT bonding workflow are modified.

## Implemented

- Scans for the CSTP service only and opens a single GATT connection, without requiring BLE bonding.
- Subscribes to control and encrypted data; reads Cardputer's HELLO and generates a 256-bit Blade nonce via Android `SecureRandom`.
- Constructs v1 transcript and CLIENT_PROOF (HMAC-SHA256). Rejects unauthorized SERVER_PROOF with constant-time comparison before permitting any telemetry rendering.
- Derives C2B AES-256-GCM key and nonce prefix from the same PSK, challenges and HKDF contract as PR53/54; rejects wrong tag, session identifier, sequence, direction, type, length and unexpected plaintext schema.
- For one authenticated session accepts only encrypted *status* messages, no details/alerts yet. On disconnect/auth failure, drops keys/session and shows no telemetry. Does not discover or subscribe to legacy plaintext service.
- Dev-only **raw 32-byte key** in Android app-private file `cstp.psk`; never embedded in source, APK, logcat or BLE. This is not Keystore-backed at-rest encryption and should be replaced with reviewed provisioning/key storage before field use.

## Build

```sh
cd blade2
./gradlew :app:assembleDebug :app:testDebugUnitTest
```

Flash Cardputer with PR54 profile `cardputer-cstp-dev` and provision *the same* 32-byte cryptographically random PSK via its USB lab mechanism. The Cardputer PSK persists in ordinary NVS and is **not protected from physical flash extraction**.

For this laboratory Blade build, install its debug APK. Provision `cstp.psk` as **exactly 32 binary bytes** via authorized, physically controlled ADB into the package's private files directory. One possible method with local private file `/path/to/cstp.psk` is:

```sh
adb shell run-as org.callingcard.blade mkdir -p files
cat /path/to/cstp.psk | adb shell run-as org.callingcard.blade sh -c 'cat > files/cstp.psk'
adb shell run-as org.callingcard.blade chmod 600 files/cstp.psk
```

Verify provisioning by app UI, **never print the raw key**. `run-as` requires a debuggable APK; if it is unavailable, stop and choose a reviewed provisioning workflow instead. Do not commit the PSK, use terminal history with literal key bytes, or paste it into issue comments.

**Important:** the Cardputer USB provisioning command uses a hex encoding of the same raw 32 bytes. Keep a single private, randomly generated binary key file for the Blade; encode it to hex *only inside a trusted, non-recorded provisioning utility* for Cardputer. Do not derive a PSK from the BLE PIN.

## Validation

1. Start the **Callingcard CSTP (Lab)** icon and permit Bluetooth scan/connect privileges.
2. A missing key must show `CSTP NOT PROVISIONED`; never show legacy telemetry.
3. On discovery, confirm `CSTP authenticating` and `CSTP authenticated=1` in filtered logcat only after successful SERVER_PROOF verification.
4. Ensure encrypted status renders and advances sequence with no plaintext, wrong-tag or replay acceptance.
5. Unprovisioned/wrong-key Cardputer, wrong Blade PSK, forged SERVER_PROOF, bad GCM tag, replay and disconnection must fail closed.
6. Confirm MTU 100 and bound status notifications. At MTU 23 the Cardputer intentionally does not send oversized encrypted frames; no fragmentation is supported yet.
7. Validate reconnection only after restarting the lab activity (no automatic retry). Confirm new challenges/session ID and sequence reset per **fresh session**.
8. Run Android unit tests and the PR53 Python known-answer tests. Independently compare Cardputer and Blade derived keys and ciphertext with the reference protocol before relying on the link.

## Known limitations / follow-up

- PR54 Cardputer currently marks itself authorized after proof_B and **before explicit proof_C acknowledgement**. Blade validates proof_C and refuses to render before that, but this is not a full client-finished handshake.
- Callback ordering, negotiated ATT MTU and GATT transport timing need device validation; no live tests are claimed.
- No encrypted alert/detail support, no session rekey on counter exhaustion beyond fail-closed, no fragment reassembly, no key rotation UX or Keystore-backed PSK storage.
- Separate PR56 should validate wire interoperability, add packet framing/reassembly and revisit key storage and session confirmation.
- `cstp.psk` in app-private files and NVS are lab-only secrets and remain vulnerable to endpoint compromise, local debugging and some physical attacks.

**No Android SDK build or physical hardware tests have been run by the assistant.**
