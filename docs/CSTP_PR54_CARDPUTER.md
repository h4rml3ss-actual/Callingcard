# PR 54 — Cardputer CSTP secure GATT prototype

**SECURITY STATUS: EXPERIMENTAL / NOT READY FOR LIVE SECURE USE.** This changes **only the opt-in** `cardputer-cstp-dev` firmware profile. No Blade 2 CSTP client exists yet, and the default/legacy profiles are unchanged.

### What is actually implemented

- Separate CSTP service `cabb1e00-ccad-4abc-a112-24fc00000100`, control `...0101` and encrypted telemetry `...0102`; **no legacy Blade plaintext status/alert/detail service in this profile**.
- Cardputer-generated random 32-byte HELLO challenge, bounded 30-second enrollment deadline, one HMAC-SHA-256 client proof over the full transcript, server proof, HKDF-SHA-256 derivation of C2B encryption key and GCM nonce prefix.
- Session is reset on disconnect; PSK mismatch or malformed handshake fails closed.
- Encrypted status aggregate uses AES-256-GCM and a monotonic 64-bit outbound sequence in the 21-byte authenticated header.
- Prototype-only physical USB serial `CSTP-SET <64 hex chars>` provisioning within **30 seconds after boot**, storing 32 bytes in regular ESP32 Preferences/NVS. Key input is not intentionally echoed by firmware. Never share this command or captured terminal output.

### Provisioning risk / test procedure

A secure client is **not yet available**: PR 55 must implement Blade challenge/proof verification, AES-GCM decryption, and proper session gating before this prototype becomes interoperable. For laboratory tests:

1. Flash `pio run -e cardputer-cstp-dev -t upload`, then open serial monitor at 115200 quickly after boot.
2. Generate a fresh random 32-byte PSK using a cryptographically secure local generator; never choose a passphrase, six-digit PIN, or universal key. Avoid shell history, screen recording, CI and log files when provisioning.
3. Within 30 seconds, send `CSTP-SET ` followed by 64 hex digits over a **physically controlled USB** connection. Reboot and verify `CSTP service=isolated provisioned=1`; do not print or log the PSK. If provisioned while a client is already connected, disconnect/reconnect before testing handshake.
4. Later Blade client must subscribe to the control characteristic and read its 34-byte HELLO (`0x01,0x01,C[32]`); return 66-byte `0x02,0x01,B[32],proof_B[32]` by writing control. Server writes/notifies `0x03,0x01,proof_C[32]`. Only if client verifies proof_C is the session mutually authenticated. Encrypted data notifications begin after Cardputer checks proof_B.
5. ATT MTU must accommodate the full 21-byte header, plaintext payload and 16-byte GCM tag; no fragmentation implemented. No data notification if the negotiated MTU is too small.

### Important limitations / release gates

- **Regular NVS is not cryptographically protected against flash readout.** This prototype assumes physical trust and must not be advertised as tamper-resistant; production needs secure boot/flash encryption or a reviewed hardware-backed secret arrangement.
- The USB provisioning command exposes the raw PSK to the terminal input path; firmware does not intentionally print it but serial/terminal tooling might retain it. Use an isolated lab environment and do not reuse real credentials.
- The Cardputer treats a valid client proof as authorization and sends its proof. A future Blade must verify proof_C before showing any telemetry. No commands or Blade→Cardputer encrypted data are supported yet; replay handling for future incoming data is still outstanding.
- GATT status/details/alerts are not fully migrated. The prototype encrypts only the aggregate status. Missing-key devices never transmit ciphertext or legacy plaintext.
- The BLE data notifications are full frames; ATT MTU 23 is unsupported until fragmentation (PR56). MTU-100 lab validation required.
- Secure random challenges, key erasure behavior, callback ordering and concurrent BLE client behavior require hardware and security testing. This PR is not an endorsement of the cryptographic implementation.
- Existing Cardputer SD `INVALID_FILE_SIZE` error remains a separate issue.

### Validation

Build: `pio run -e cardputer-cstp-dev`. Ensure existing `cardputer-blade-secure-quiet` builds and default scanner builds remain intact. Negative test missing PSK => no data, wrong proof => no data, disconnect => session reset, oversize/invalid control => session reset, no legacy service present. Cross-check raw proof/key ciphertext against PR53's Python/Android primitives before treating encrypted data as trustworthy.

**No firmware build or hardware test was executed by the assistant.**
