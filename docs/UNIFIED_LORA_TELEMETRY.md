# PR 12 — Unified LoRa metadata events

A successfully received SX1262 packet now generates an `Observation` through the same **32-entry bounded FIFO** as Wi-Fi and BLE. The source is `lora`, with `uptime_ms`, integer `rssi_dbm`, and LoRa-only decimal `snr_db`. The Wi-Fi/BLE JSON shape remains unchanged; schema remains `callingcard.event.v0`.

Example (illustrative, not a captured packet):

```json
{"schema":"callingcard.event.v0","seq":11,"kind":"observation","source":"lora","uptime_ms":14520,"rssi_dbm":-94,"channel":null,"label":"sx1262","snr_db":-7.0,"utc":null,"position":null}
```

**Privacy / scope:** No LoRa payload, node identifiers, decrypted content, or GNSS coordinates are emitted. The receive-only PHY profiles from PR #11 are unchanged. The original human-readable `CAP LORA RX` diagnostic is retained. Radio receive work still happens in the main loop, not in the interrupt handler.

**Acceptance test on Cardputer ADV:**
1. Build/upload `pio run -e cardputer-adv` / `pio run -e cardputer-adv -t upload`; listen at 115200.
2. Confirm recurring `CAP DIAG stage=READY ... sx1262_init=0 rx_start=0` and active profile on boot.
3. While compatible LongFast transmissions are nearby, check that each successful `CAP LORA RX count=N` produces one JSONL `source":"lora"` event with SNR and RSSI. Events may appear after the next blocking Wi-Fi/BLE scan completes; don't assume instantaneous emission.
4. Verify sequence numbers are shared among Wi-Fi, BLE, LoRa and restart on reboot. Confirm `PIPE dropped=0` during normal reception. Trigger stress traffic if available to observe queue overflow diagnostics, not silent loss.
5. Confirm Wi-Fi/BLE events retain their old fields (without `snr_db`), GNSS continues increasing checksum-valid sentences, and no packet payload bytes appear on serial.
6. Soak test for 30 minutes; check radio restarts, dropped queue counts and heap.

**Limitations:** The radio's RSSI is truncated to an integer for the shared event format, while the human-readable log retains a decimal. `channel=null` because LoRa PHY frequency is not a Wi-Fi channel; the active frequency is reported in `CAP LORA PROFILE`. Event uptime is the **time the queued receive is serviced**, not hardware interrupt time; blocking Wi-Fi/BLE scans can delay it. Current firmware still materializes received payload into a temporary String to complete the RadioLib packet read, but does not store or emit that payload. This PR has not been compiled or physically tested by the assistant.