# PR 22 — Per-session survey summaries

Callingcard now records bounded aggregate metrics for each microSD session without adding device identifiers or packet payloads.

## Definition of metrics

- **processed** counts complete JSONL observation records emitted to Serial while the session is active, **including paused periods**. It is not the number of raw radio callbacks or physical devices.
- **saved** increments only after the mirror file reports a full write of the serialized observation. The file is flushed/closed at stop/limit; the counter does not independently prove post-power-loss durability.
- **unsaved** covers serial observations during pause or mirror failures.
- **wifi/ble/lora** counts are per-source processed events. **wifi_saved/ble_saved/lora_saved** are successfully mirrored counts.
- **GNSS coverage** is exposed as `utc_tagged` and `position_tagged` event counts, not a percentage of time outdoors. Null UTC/position fields do not count.
- **SSID hashes**: up to 32 distinct SSID hashes, **not AP or device identities**; truncated/hidden names and collisions can distort counts. Reaching the capacity sets `ssid_limit=1`. BLE friendly names are *not* treated as unique devices.
- **RSSI** strongest/weakest signed dBm values for emitted observations, separated by radio source.
- **pipeline_lost** and **ble_prequeue_lost** are differences from the beginning of the session; they represent different stages. BLE callback loss can occur even with pipeline drop zero.
- **duration_ms** is elapsed session time from creation until stop or size limit, including pauses. **bytes** is the finalized microSD file size.

## User experience

At **S stop**, **4 MiB limit**, or storage error, a `SESSION SUMMARY` line and up to three `SESSION RSSI` lines are printed to USB serial. The display switches to a compact summary when the session is stopped or at size limit. Press **R** to create a fresh session. Pausing does not finalize a session and normal collection continues; stopping recording does not stop scans. Prior JSONL observation schema is unchanged; summaries are *diagnostic lines*, not JSONL rows added to the microSD file.

## Acceptance tests

1. Build using `pio run -e cardputer-adv`, upload, and confirm normal Wi-Fi/BLE/LoRa/GNSS/SD telemetry.
2. Collect a few scans, press **P**, allow scans to continue, press **P** to resume, then **S**. Confirm `processed > saved` if observations occurred while paused, and confirm per-source saved counts sum to `saved`.
3. Confirm the post-stop display shows summary values and pressing **R** creates a new session with counters reset.
4. Compare `saved` with the actual count of JSONL rows in the microSD file and inspect lines for validity. Check byte total against file size.
5. Verify `pipeline_lost` and `ble_prequeue_lost` are separate, no unique-device claim is made for SSIDs or BLE names, and RSSI ranges are plausible.
6. Validate 4 MiB limit and storage-failure behavior. Review memory usage and radio RX during a 30-minute soak.

**Development status:** The PR has been authored through the GitHub connector, but not compiled or hardware-tested by the assistant. Some aggregate metrics are intentionally approximate (SSID hash distinctness, GNSS event coverage).
