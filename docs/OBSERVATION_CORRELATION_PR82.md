# PR82 — Bounded observation correlation

A new header-only `ObservationCorrelator` tracks repeated **classified Wi-Fi/BLE observations** by `source + exact advertised address + detection category`, not by device name, RSSI or geographic proximity. Address case is normalized for comparison.

For matching observations, the JSONL event gains optional `correlation` metadata:
- `scope: "session_address_type"` — legacy label; this implementation is currently **runtime/boot-scoped** until explicit session lifecycle reset is wired.
- `sightings`: number of captured, classified observations for that key (saturates at 65,535)
- `peak_rssi_dbm`: strongest observed RSSI for that key
- `first_uptime_ms`: first recorded uptime for that key

All raw `detection`, `position`, `utc`, identifiers and original RSSI continue unchanged. Missing/malformed addresses and unclassified observations receive no correlation object. The table has **32 fixed slots**, with eviction of the least recently updated entry; reboot resets all state. This does not match rotating BLE addresses or claim physical device identity. Repeated observations **do not increase detection confidence**, because repetitions from the same source are not independent evidence.

No change to Blade GATT/Android, event survey cadence, cooldown rules, or GPS annotation.

## Test
```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/observation_correlator_test.cpp -o /tmp/callingcard-correlation
/tmp/callingcard-correlation
pio run -e cardputer-blade-wifi-integrated
```

Inspect USB or SD JSONL for `correlation.sightings` incrementing when a controlled `flock-test` AP repeats using the same BSSID. Check a changed BSSID creates a separate key. Different detection categories and Wi-Fi versus BLE should also remain separate. Watch free RAM and the SD pipeline for larger records.

**Limitations:** Current accumulator is reset on reboot rather than whenever an SD recording session is restarted. It is not persistent and does not carry a session UUID. Eviction of older entries resets their count. GPS is the receiver's location, not a transmitter location. Hardware/build verification pending.
