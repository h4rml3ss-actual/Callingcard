# Callingcard Secure Telemetry Protocol (CSTP) v1 — design contract

**Status: DESIGN ONLY (PR #52).** No implementation in this PR. Do not advertise this design as deployed encryption. The present `cardputer-blade-secure` and `cardputer-blade-secure-quiet` profiles still depend on BLE SMP bonding, which has failed on Blade 2. The legacy `cardputer-blade-details` profile sends unauthenticated telemetry. Neither is an application-layer-secure substitute.

## Security objectives / threat model

Protect status, alert and detail data against passive observers and active unauthenticated GATT clients: confidentiality, integrity, source authenticity within a paired Cardputer/Blade trust relationship, and replay rejection. BLE advertising, addresses, connection timing and RF traffic metadata remain observable. A compromised endpoint, extracted Cardputer flash, compromised Android OS, physical attackers with exposed debug interfaces, deliberate jamming/DoS, and side channels are outside the guaranteed protections. Application-layer cryptography **does not** automatically secure BLE scanning or connection establishment.

**Fail closed**: no legacy plaintext telemetry, notifications, or details in the secure-application profile before mutual proof and session-key establishment. Failed proofs, malformed frames, tag errors, sequence rollback/duplication and counter exhaustion disconnect/rekey. No insecure fallback or automatic approval based on a six-digit PIN.

## Architecture

- BLE GATT is treated as untrusted transport; ordinary GATT may be unbonded. Expose only enrollment/handshake control characteristics and ciphertext on the new, separate secure service UUID. Do **not** reuse legacy unencrypted status/detail UUIDs or their access permissions in the secure profile.
- Independent per-device 256-bit uniformly random **root PSK**, provisioned out-of-band. Never derive the PSK from the BLE PIN, SSID, a MAC address or a human-chosen password. Never commit a sample PSK or ship a universal fallback.
- Cardputer generates a random 256-bit ephemeral challenge `C`; Blade generates a random 256-bit challenge `B`. Both contribute to session key derivation, so restarting or resetting a counter does not repeat a nonce/key pair.
- Two HMAC-SHA-256 proofs bind the protocol, both challenges, device/application roles and the root PSK. Abort unless both proofs verify in constant time.
- HKDF-SHA-256 derives **separate** directional AES-256-GCM keys and nonce bases for Cardputer→Blade and Blade→Cardputer. Domain separate all labels; include both challenges and a fixed versioned transcript.
- Each encrypted direction has an independent 64-bit unsigned sequence, initialized to 0 for every new session. A 96-bit GCM nonce = 4-byte derived per-direction prefix || 8-byte big-endian sequence. Never use a sequence twice with the same key. Refuse wrap.
- All authenticated ciphertext uses AES-256-GCM with a 16-byte tag and associated data including protocol version, direction, message type, session identifier and sequence. Check message sizes and counters **before** allocating or acting.
- A GATT disconnect destroys ephemeral keys and resets authorization. A reconnect performs a **new** two-challenge handshake, even if BLE link/bond persists; never resume stale session keys.
- Gate Cardputer queueing/transmission of sensitive telemetry and Blade parsing/rendering on secure session authorization, not BLE connected/bonded state.

## Canonical wire encoding (v1, normative for later implementation)

All integers unsigned, network byte order (big-endian). Byte strings are raw bytes, not hex text.

### 1. Versioned transcript

`T = ASCII("CSTP-v1") || 0x00 || C[32] || B[32]`

`sid = first 8 bytes of SHA256(ASCII("CSTP-v1 sid") || 0x00 || T)`

Never accept all-zero challenges. Each challenge MUST originate from a cryptographic RNG and be fresh for each connection.

### 2. Control messages

| Type byte | Sender | Payload | Meaning |
|---|---|---|---|
| 0x01 | Cardputer | `version=0x01 || C[32]` | HELLO |
| 0x02 | Blade | `version=0x01 || B[32] || proof_B[32]` | CLIENT_PROOF |
| 0x03 | Cardputer | `version=0x01 || proof_C[32]` | SERVER_PROOF |
| 0x7f | Either | `version=0x01 || generic_error[1]` | ABORT; no sensitive details |

`proof_B = HMAC-SHA256(PSK, ASCII("CSTP-v1 blade-proof") || 0x00 || T)`

`proof_C = HMAC-SHA256(PSK, ASCII("CSTP-v1 cardputer-proof") || 0x00 || T)`

Cardputer validates proof_B before responding; Blade validates proof_C before granting session authorization. Never log PSK, full challenge, proof, key or decrypted messages.

### 3. Session keys

`salt = SHA256(ASCII("CSTP-v1 salt") || 0x00 || T)`

`PRK = HKDF-Extract(salt, PSK)`

`K_c2b = HKDF-Expand(PRK, ASCII("CSTP-v1 key c2b") || 0x00 || sid, 32)`

`K_b2c = HKDF-Expand(PRK, ASCII("CSTP-v1 key b2c") || 0x00 || sid, 32)`

`N_c2b = HKDF-Expand(PRK, ASCII("CSTP-v1 nonce c2b") || 0x00 || sid, 4)`

`N_b2c = HKDF-Expand(PRK, ASCII("CSTP-v1 nonce b2c") || 0x00 || sid, 4)`

The nonce for direction D and sequence `seq` is `N_D[4] || BE64(seq)`.

### 4. Encrypted data

An unfragmented logical data frame: `version[1]=0x01 || direction[1] (0x01=c2b, 0x02=b2c) || type[1] (0x10=status,0x11=alert,0x12=detail,0x20=command) || sid[8] || seq[8] || length[2] || ciphertext[length] || tag[16]`.

AES-GCM AAD is **the complete 21-byte frame header** (all bytes preceding ciphertext). Encrypted payload is application-specific bytes with bounded maximum length, specified per type before implementation. Reject unknown types/directions, mismatched sid, wrong direction, bad tag, noncanonical length, duplicate or out-of-order sequence, and unsupported version **before dispatch**. Each valid received frame must advance the expected sequence by exactly 1; do not advance on authentication failure.

**GATT MTU / fragmentation:** frame above is logical, not guaranteed to fit ATT MTU 23 or 100. A later PR MUST specify bounded fragmentation/reassembly, strict memory/time limits and tamper-resistant framing before transporting encrypted detail payloads. Do not silently truncate ciphertext or disable tags. Avoid relying on an MTU upgrade.

## Enrollment and key lifetime

Initial developer provisioning for the experimental profile only: generate 32 bytes with OS CSPRNG in an isolated environment; transfer to Cardputer over physically controlled USB and Blade over authorized local ADB/application import. Require explicit physical/user approval and verify a short fingerprint over a separate trusted channel. Do not expose raw keys on LCD, UART/ADB logcat, GitHub Actions, build flags, QR codes shared publicly, or normal BLE characteristic reads. Reject an unprovisioned device.

Android: protect stored PSK with Android Keystore-backed encryption where supported; ensure backups do not silently clone identity. Note: Keystore alone does not make a raw provisioned PSK non-exportable after it has entered the application. ESP32-S3: plan use of encrypted NVS with hardware flash-encryption / secure boot for production physical-threat resistance; an unencrypted NVS PSK is exposed to flash readout. Avoid erasing NVS without explicit user consent.

Provide explicit rotate/revoke/reset pair operation, zeroize ephemeral keys where feasible, and retire old keys. When either device loses the root PSK, require new physical provisioning; never fall back to old insecure profile automatically.

## Required security tests (blocking before calling it secure)

1. Genuine credentials: both proofs succeed, AES-GCM round trip works, and HUD receives expected status/detail after mutual authorization.
2. Wrong PSK or unprovisioned client: no plaintext telemetry or authorization.
3. Tampered proof, tag, header, direction or sid: reject before rendering; never leak decrypted text.
4. Replay/duplicate/out-of-order/counter overflow: reject; disconnect/rekey as appropriate.
5. Reconnect and reboot: fresh challenges and keys, nonce reuse absent, no stale authorization.
6. ATT MTU 23 and 100, fragmented messages, disconnect mid-fragment, oversized frames and malformed lengths: bounded safe failure.
7. Two simultaneous GATT clients, race at handshake, stale callbacks: exactly one authorized session per enrolled Blade; other clients denied.
8. Device key rotation, revoked client, cold-boot unprovisioned behavior, accidental provisioning logs and nonproduction rollback.
9. Radio interference and induced disconnects: recover by renegotiating rather than leaking or reusing keys.

## Staged roadmap

- **PR52:** protocol/security review contract (this PR; zero runtime impact).
- **PR53:** shared spec-conformant primitives, byte codecs, known-answer test vectors for HMAC/HKDF/AES-GCM; unit/host tests. No BLE migration.
- **PR54:** Cardputer opt-in secure-app GATT service, provisioning, anti-replay state machine; fail closed.
- **PR55:** Blade opt-in authenticated client and secure key import; no unsecured fallback; independent Android/firmware test.
- **PR56:** bounded BLE framing/fragmentation, resilience tests and security review; only then discuss default deployment.

**Do not enable a new 'secure' runtime profile that still publishes legacy plaintext telemetry.**
