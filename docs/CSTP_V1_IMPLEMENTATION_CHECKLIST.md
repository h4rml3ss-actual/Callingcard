# CSTP v1 implementation gates

This checklist defines the acceptance criteria for the staged rollout documented in [CSTP protocol](CSTP_V1_SECURITY_PROTOCOL.md). PR #52 is design-only: it does **not** change the existing BLE application or provide encrypted telemetry.

## Mandatory review questions

- [ ] Confirm AES-256-GCM, HMAC-SHA-256, HKDF-SHA-256 on both Arduino-ESP32 and Blade Android versions (reliable, well-tested libraries only)
- [ ] Independent review of challenge freshness and nonce uniqueness under reboot/power loss
- [ ] Independent review of protocol frame / AAD / session identifier / directional key labels
- [ ] Confirm MTU-23 fragmentation bounds and maximum per-message memory requirements
- [ ] Identify physical PSK provisioning channel and secure reset/rotation UX
- [ ] Audit where current firmware broadcasts plaintext status, alerts and details and enforce fail-closed guard in future secure-app profile
- [ ] Confirm existing BLE security profiles remain separate and clearly labeled experimental/non-app-encrypted
- [ ] Confirm pairing code is never reused as PSK and no cryptographic secrets appear in stdout/logcat
- [ ] Validate negative tests for unknown peers, tampering, replay, truncated frames, and concurrent clients
- [ ] Validate security claims with on-device packet inspection and reconnect tests before release

## Open design decisions for PR53+

1. **Root PSK storage:** Cardputer encrypted NVS + flash encryption/secure boot versus explicitly documented prototype-only flash threat model. Do not claim physical key-extraction resistance without verified protections.
2. **Key import:** USB serial development provisioning and Android ADB import must not log or persist raw import values in command history, CI output or public logs.
3. **Control characteristic design:** Define dedicated secure service/characteristic UUIDs without exposing old plaintext characteristics in the opt-in secure application profile.
4. **Maximum payload and reassembly:** Establish bounded payload sizes, fragment counters and deadlines; unauthenticated fragment floods cannot exhaust RAM.
5. **Session invalidation:** Terminate pending handshakes on disconnect, malformed packets, authorization errors and elapsed deadline. Separate transport connections and cryptographic sessions.

The SD `INVALID_FILE_SIZE` / `UINT32_MAX` fault and ESP32 native BLE bonding timeout are distinct issues, and are not implicitly fixed by this security protocol design.
