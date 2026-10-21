# PR 15 — GNSS observation context

Callingcard now captures **GNSS UTC** and **GNSS position** at the moment each Wi-Fi, BLE, or LoRa observation is enqueued. Both fields remain `null` unless their source is valid and fresh. The existing `callingcard.event.v0` JSONL keys and SD/serial mirroring remain unchanged.

## Exact behavior

- `utc` becomes a string in `YYYY-MM-DDTHH:MM:SSZ` (GNSS UTC, second precision) only when both date and time have valid fixes and ages **at most 2000 ms**, with a plausible year in 2020–2099 and basic component range checks.
- `position` becomes `{"lat":42.000000,"lon":-71.000000}` only when a location fix is valid and its age is **at most 2000 ms**.
- Time and position validity are independent. A valid time does not imply a valid location.
- The observation is annotated **when enqueued**, not when its JSON line is flushed or read from the SD card; no late fixes are applied retroactively.
- LoRa event annotations occur immediately after the completed radio receive is serviced in `CapSensors::poll()`. Wi-Fi/BLE event annotations occur during their existing observation collection code.
- If the GNSS updates are delayed by synchronous multi-second Wi-Fi/BLE scans, fields remain null rather than using stale data. UTC seconds reflect the last fresh GNSS sentence; the firmware does not interpolate UTC or assert sub-second precision.
- GPS coordinates and timestamps are now deliberately recorded to serial and microSD. Treat logs as sensitive location history and share only with consent. Existing packet-payload-free LoRa policy remains unchanged.

## Hardware tests

1. Build and upload the current ADV environment; verify existing Wi-Fi/BLE, LoRa receive and SD session states.
2. Indoors, verify `position:null` when `CAP GPS location=NO_FIX`. If date/time have not been validated, `utc:null` is correct.
3. Outside with an unobstructed sky, wait for `CAP GPS location=FIX`; verify *some* later events contain valid numeric lat/lon and plausible UTC. Never mistake a non-null date alone for a position fix.
4. Shield the GNSS antenna or move indoors until its fix becomes stale; ensure **new** events revert to null rather than retaining old coordinates. Existing queued records must stay unchanged.
5. Verify both Serial and `/CALLINGCARD/*.JSONL` contain matching source events and no JSON parse errors; test pause/resume and new sessions.
6. Confirm RSSI/SNR metadata, LoRa receive counts, scan rates, queue drops and free heap remain stable in a 30-minute test.

## Known constraints

This is conservative GNSS tagging. With synchronous BLE scans lasting around four seconds, GNSS parsing can lag and fresh timestamps/position may often remain null even outdoors. Follow-up work should decouple GNSS UART ingestion from blocking scanner activity. There is no RTC persistence, inferred location, interpolation, or GNSS accuracy threshold. This PR is **not hardware-tested or compiled by the assistant**.
