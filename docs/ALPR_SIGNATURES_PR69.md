# PR69 — Conservative vendor-associated ALPR candidates

This PR expands the existing Wi-Fi BSSID matcher with manufacturer-registered OUI prefixes associated with Motorola Solutions and Genetec. These companies sell ALPR solutions (Motorola/Vigilant; Genetec AutoVu), but their OUIs are also usable by **other products**. Accordingly, all new `ALPR` matches are **LOW-confidence candidates only**, not declarations of a license plate reader.

## Verified-source entries
Motorola: `00:04:7D`, `00:18:85`, `00:1F:92`, `4C:CC:34`.
Genetec: `00:BF:15`, `0C:BF:15`.

Source: SquachWatch-CYD [Detection signature provenance and IEEE registry audit](https://github.com/skizzophrenic/SquachWatch-CYD/blob/master/docs/DETECTIONS.md), checked Oct 2026. The entries are vendor-associated but **not individually validated against physical ALPR camera broadcasts**. Do not infer vendor ownership of an observed device merely because its address matches: locally administered/randomized and spoofed addresses exist.

Known exclusion: `00:0E:58` is registered to **Sonos**, and was erroneously called Vigilant in older external detectors. We explicitly reject it, along with generic Espressif addresses. A Flock-associated `B4:1E:52` remains handled by the existing Flock classifier, with prior confidence rules unchanged.

## Scope
- Reuses PR64 Wi-Fi BSSID data, existing `classifyWifi` result, PR66 alert formatting and 30-second cooldown. **No new radio capture logic, BLE changes, packet injection, GPS data or private addresses transmitted**.
- Passive Wi-Fi test environment `cardputer-blade-passive-wifi` introduced in PR68 **replaces active Wi-Fi scans** and therefore does not drive these OUI rules. For classification trials, use the known-good `cardputer-blade-details` profile.
- No additional Flock chipset-vendor prefixes were added: unrelated ESP32 devices would cause high false-positive rates.
- Other ALPR model signatures are deferred until captured and independently corroborated.

## Validation
```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/wifi_signatures_test.cpp -o /tmp/callingcard-wifi-signatures
/tmp/callingcard-wifi-signatures
pio run -e cardputer-blade-details
```
When one of the OUI signatures appears during ordinary Wi-Fi scan, expected USB serial output is e.g. `DETECT candidate=ALPR confidence=LOW rule=alpr-genetec-00bf15 ...`. With the PR66 Blade HUD installed, it may show `POSSIBLE ALPR / LOW`. This reflects **vendor association only**, not positive ALPR identification.

Build and hardware tests are pending.
