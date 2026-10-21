# PR 17 — Cardputer ADV status dashboard

The former scan-result screen is replaced by a compact operational status view:

- Latest **Wi-Fi** and **BLE** discovery counts, cumulative successful **LoRa RX** count.
- GNSS fix status, satellites and valid/fresh GNSS **UTC** (or `--` if unavailable).
- microSD session state (`RECORDING`, `PAUSED`, `STOPPED`, `NO_CARD`, `LIMIT`, `ERROR`), file basename, bytes, and error count.
- FIFO queue depth / dropped count, radio readiness and current free heap.
- Reminder for **P** pause/resume, **S** stop, **R** new session/resume.

The dashboard redraws on a **750 ms scheduler** checked in the main loop, instead of only when a Wi-Fi/BLE cycle finishes. This does *not* make the blocking Wi-Fi (~1.7 s) or BLE (~4 s) survey calls asynchronous. During those calls screen refresh and key polling can still be delayed. A later architectural change should move to nonblocking scanning and more frequent GNSS ingestion.

The existing session controls, radio modes and SD/serial JSONL are unchanged. The display shows counts, not captured payloads or raw positions. The displayed UTC is explicitly gated by GNSS validity using the same criteria as observation enrichment. A missing fix shows `--`.

## Hardware acceptance checks

1. Build `pio run -e cardputer-adv` and upload. Verify the dashboard is legible without clipped labels on the 240×135 display.
2. Verify Wi-Fi/BLE counts update, LoRa RX count increments with local compatible transmissions, GPS fix/satellites and UTC match serial GNSS diagnostics.
3. Press P/S/R and verify header state/file/bytes update; inspect valid JSONL on microSD.
4. Confirm `PIPE dropped=0`, serial JSONL emission, no regressions to LoRa RX or GNSS UART.
5. Repeat with no microSD: display must show `NO_CARD` while surveying continues.
6. Confirm no crash or unusual heap decline during a 30-minute soak.

**Development status:** Changes are committed but not compiled or hardware-tested by the assistant.
