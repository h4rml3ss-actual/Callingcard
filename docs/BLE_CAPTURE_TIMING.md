# PR 21 — BLE prequeue loss and capture-time GNSS

Previous builds stored only five BLE callbacks per four-second scan, then applied GNSS time/position when the scan finished. This change uses a **48-item fixed BLE ring**, drained by the main loop while scans are running, to preserve many more advertisements without running SD, TinyGPSPlus, or event-queue work on the BLE callback task.

## Timing and privacy
- The main loop refreshes a GNSS metadata anchor after each `cap.poll()`; BLE callbacks copy that anchor under the same critical section used for the ring.
- An anchor is copied only if it was published within **500 ms before** the callback's `uptime_ms`. A stale anchor leaves UTC and position null. The GNSS validity/freshness tests still apply.
- UTC remains second-resolution, and its displayed second may differ by one near a second boundary. This is a **bounded capture-time approximation**, not hardware-synchronized or high-precision GNSS time. GPS locations are sensitive.
- Wi-Fi annotations and LoRa annotations remain on their existing main-loop paths. No packet payload retention is added.

## Buffering and loss
- `kBleQueueCapacity=48`; `drainBleSamples()` transfers up to eight records each main-loop iteration while BLE scanning, and drains the remainder at scan completion. This is independent of the display's five-line presentation cap.
- `CYCLE mode=ble ... prequeue_dropped=N` counts ring saturation separately from `PIPE dropped=N`, which tracks failures in the downstream observation FIFO. Both are cumulative and should remain low in normal use.
- No dynamic memory allocation was added to the BLE observation ring. Radio scan completion and storage behavior otherwise remain unchanged.

## Acceptance tests
1. Build/upload with `pio run -e cardputer-adv`. Verify BLE scans complete and `prequeue_dropped` stays low while the JSONL stream emits more than five BLE observations per scan.
2. Confirm Wi-Fi/BLE alternation, increasing GNSS sentence count, LoRa RX events, and SD JSONL mirroring.
3. Outdoors with GNSS fix, inspect BLE event `uptime_ms`, UTC and coordinates; new events should show live UTC/location, not the same end-of-scan snapshot for every BLE advertisement. Near second boundaries, UTC is approximate.
4. Remove GNSS fix or move indoors; after source expiry, new BLE events must have null fields, with no retroactive enrichment.
5. Stress nearby BLE advertisers, monitor `prequeue_dropped` and `PIPE dropped` separately, check heap and serial JSONL parsing for 30 minutes.
6. Confirm keyboard session actions remain responsive during BLE scanning and data is readable on microSD.

**Not compiled or tested on physical hardware by the assistant.** High advertisement rates can still overflow the ring or downstream queue; the ring counters measure these losses. No claims of zero packet loss or precise radio arrival times.
