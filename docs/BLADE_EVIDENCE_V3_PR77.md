# PR77 — Opt-in BLE detection identifier and signature evidence

Adds `cardputer-blade-evidence-v3`, extending PR74's `H/P/T` geographic telemetry with **sequence-tagged evidence fragments**. Stable `cardputer-blade-details` remains unmodified. The existing alert characteristic, scan scheduling, and BLE authentication approach do not change.

## Format
After a classified `H` header and optional `P` coordinate / `T` UTC, send up to 10 evidence fragments:
```
X,15,I,0,3,B4:1E:52
X,15,I,1,3,:01:02:0
X,15,I,2,3,3
X,15,R,0,3,flock-ss
X,15,R,1,3,id-prefi
X,15,R,2,3,x
```
- `I`: Wi-Fi BSSID or BLE advertiser address, if captured.
- `R`: classification rule ID.
- `seq`: same 0–999 sequence as the `H/P/T` observation.
- `index` starts at 0; `total` is bounded at 9 for each field.
- Up to 8 ASCII bytes per fragment, under 20 ATT payload bytes including framing. No negotiated MTU required.
- Reject unexpected punctuation and lengths rather than truncating evidence.
- Only one outstanding record on Cardputer; no heap-allocated fragment queue. A dropped notification may result in incomplete evidence, never automatic reconstruction from the wrong sequence.

## Blade behavior
- Android parses X fragments for the **latest matching H sequence only**, within a 20-second timeout.
- Bounded reassembly arrays permit up to 9 chunks per field. Validate MAC format before displaying identifier.
- Completed identifier/rule is logged and shown in the live HUD alert; the persistent PR72 candidate history is not converted to per-device identity tracking in this PR.
- Android still accepts legacy `E/C` and v2 `H/P/T` data.
- Because the link is unencrypted, peers nearby can observe/spoof messages. Use the opt-in environment for controlled tests.

## Test
```bash
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/evidence_frames_test.cpp -o /tmp/callingcard-evidence-test
/tmp/callingcard-evidence-test
pio run -e cardputer-blade-evidence-v3
cd blade2 && ./gradlew :app:assembleDebug
```
Install the updated Blade APK; flash the opt-in v3 firmware. Use a controlled `flock-test` SSID. Monitor `BLADE geo=H...`, `BLADE geo=X...`, and `evidence seq=...` in Android logcat. Check a missing GPS fix, missing BLE address, incomplete fragments, reconnects, and rollback by reflashing `cardputer-blade-details`.

## Known constraints
- A MAC address may be randomized or spoofed; evidence fields do **not prove physical identity**.
- Notifications have no acknowledgement/retransmission and take up to ~13 seconds for long evidence. Strict end-to-end reliability and history enrichment will follow after hardware testing.
- No standalone BLE GATT encryption or device authorization was added.
- Builds and physical hardware tests are pending.
