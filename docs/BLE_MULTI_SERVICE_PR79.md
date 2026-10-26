# PR79 — Bounded multi-service BLE advertisement evidence

The existing Arduino-ESP32 BLEAdvertisedDevice parser already exposes `getServiceUUIDCount()` and indexed `getServiceUUID(i)` ([upstream API](https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/src/BLEAdvertisedDevice.h)). Previously Callingcard copied only the first advertised UUID; it now snapshots **up to four** services per callback and searches each before falling back to name heuristics.

The observation also captures the **first 16 bytes** of the manufacturer-data AD element, including company ID, with a length and truncation flag. The raw bytes are **not yet used for product identification**: company IDs alone cannot establish a particular model, and no vetted payload signatures have been supplied. This avoids inventing Apple AirTag, skimmer or body-camera signatures.

## Limits
- Capacity: 4 service UUID strings of 36 characters max + terminator, 16 manufacturer data bytes, all owned by the fixed observation structure.
- Overflow indicated by `bleAdvertisementTruncated`, no unbounded callback allocations beyond those already made by Arduino BLE's wrapper.
- Library-parsed service list and manufacturer data are captured; this is **not a complete raw AD-TLV decoder** or multi-packet scan response reconstruction. That would need distinct tests with malformed lengths and duplicate elements.
- The first service UUID remains in the legacy field for compatibility. Existing Blade notifications, detection confidence and cooldown, GPS timestamp snapshot, scan cadence, and GATT connection are untouched.
- Manufacturer data is not emitted on the unencrypted BLE link or the existing JSONL exporter in this PR, pending a schema version and source validation.
- A later UUID that is a supported service outranks a generic first UUID or spoofable name.
- Apple company ID `0x004C` and generic Eddystone `FEAA` continue to be negative identification cases.

## Validation
```bash
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/ble_signatures_test.cpp -o /tmp/callingcard-ble
/tmp/callingcard-ble
pio run -e cardputer-blade-details
pio run -e cardputer-blade-evidence-v3
```

Flash the desired profile and confirm known-good Wi-Fi, BLE, LoRa, GPS, and Blade connectivity. In a controlled test, advertise `180F` then `FD5A` service and check that the candidate is found even when the distinctive service is not first. Check devices with many advertised services and large manufacturer data for prequeue drop count, heap stability, and no crashes.

**Firmware and hardware builds are not yet independently verified.**
