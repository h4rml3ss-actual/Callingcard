# CSTP v1 primitive implementation — PR 53

**Status: dormant primitives, not deployed encrypted telemetry.** These files do not change BLE GATT, Android MainActivity, Cardputer scanning, key provisioning or any existing firmware profile. No end-to-end cryptographic channel exists until later integration PRs.

## Contents

- `tools/cstp_v1.py`: Python host reference implementation of the v1 transcript, role-specific HMAC-SHA-256 proofs, HKDF-SHA-256 session keys and nonce prefixes, strict canonical frame header, AES-256-GCM sealing/opening, counter/replay pre-check and control-message shape validation.
- `blade2/app/src/main/java/org/callingcard/blade/CstpV1.java`: Java/Android JCA companion utility (HmacSHA256, SHA-256 and AES/GCM/NoPadding), including directional session keys, authenticated payload encoding and verification. Not invoked by the application yet.
- `tools/tests/test_cstp_v1.py`: unit tests for NIST zero-key/zero-IV AES-256-GCM known-answer tag, basic roundtrip, directional key distinction, wrong proof, replay, tampered ciphertext/AAD, malformed lengths and incorrect sessions.
- `tools/requirements-cstp.txt`: Python cryptography test dependency.

## Test host reference

```sh
python3 -m venv .venv-cstp
. .venv-cstp/bin/activate
python -m pip install -r tools/requirements-cstp.txt
python -m unittest discover -s tools/tests -v
```

Build Android to check Java source compatibility:

```sh
cd blade2
./gradlew :app:assembleDebug :app:testDebugUnitTest
```

**Not run on hardware or compiled in a locally configured Android SDK in this PR.** Execute in your environment before merging. Review/reference tests should be independently checked with deterministic cross-language CSTP vectors before integrating the cryptographic channel.

## Implementation cautions

- The Java `long` sequence representation currently supports the nonnegative half of the 64-bit unsigned sequence range. This conservatively fails closed at `2^63`, earlier than the protocol maximum, and must trigger rekey; it must never wrap into negative values.
- The Python reference primitive does not track persistent receive/send sequence state. **The future session state machine must own sequence advancement, forbid nonce reuse, reject replays and enforce a single authorized client.** Calling the seal method twice with the same key/sequence is forbidden.
- Fragmentation/reassembly for MTU 23 is not implemented. The 256-byte payload cap is only a *logical* message bound.
- A provisioned PSK, secure storage, flash protections, a new secure-only GATT service and fail-closed controls remain future work.
- No code here should cause current plaintext telemetry to be described as encrypted or authenticated.
